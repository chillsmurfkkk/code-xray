#include "code/api.hpp"
#include "code/types.hpp"
#include "common/job.hpp"
#include "common/result.hpp"
#include <algorithm>
#include <cstddef>
#include <fstream>
#include <iterator>
#include <string>
#include <filesystem>
#include <system_error>
#include <memory>
#include <utility>
#include <variant>
#include <optional>
#include <unordered_set>
#include <vector>
#include <cstdint>
#include <string_view>
#include <atomic>
#include <random>

namespace xray::code {

namespace {

std::string pathToUtf8(const std::filesystem::path& path) {
    const auto utf8 = path.generic_u8string();
    return std::string(utf8.begin(), utf8.end());
}

std::string makeSnapshotId() {
    static const std::string sessionId = [] {
        std::random_device random;

        constexpr char digits[] = "0123456789abcdef";

        std::string id(32, '0');

        for (char& digit : id) {
            digit = digits[random() & 0x0F];
        }

        return id;
    }();

    static std::atomic<std::uint64_t> sequence{1};

    const auto number = sequence.fetch_add(1, std::memory_order_relaxed);

    return "local:" + sessionId + ":" + std::to_string(number);

}

Result<std::string> makeContentId(
    std::string_view bytes,
    const JobContext& job
)
{
    std::uint64_t hash = 14695981039346656037ULL;
    constexpr std::uint64_t prime = 1099511628211ULL;

    for (std::size_t index = 0; index < bytes.size(); ++index) {
        if (index % 4096 == 0 && job.isCancelled()) {
            return Cancelled{};
        }

        hash ^= static_cast<unsigned char>(bytes[index]);
        hash *= prime;
    }

    if (job.isCancelled()) {
        return Cancelled{};
    }

    constexpr char digits[] = "0123456789abcdef";
    std::string hex(16, '0');

    for (std::size_t index = 0; index < hex.size(); ++index) {
        hex[hex.size() - 1 - index] = digits[hash & 0x0F];
        hash >>= 4;
    }

    return "fnv1a64:" + hex;
}

bool isExcludedDirectory(
    const std::filesystem::path& directory,
    const FileSelection& selection
)
{
    const auto name = pathToUtf8(directory.filename());

    return std::find(
        selection.excludedDirectories.begin(),
        selection.excludedDirectories.end(),
        name
    ) != selection.excludedDirectories.end();
}

std::optional<std::string> filterExclusionReason(
    const std::filesystem::path& relativePath,
    const FileSelection& selection
)
{
    for (const auto& component : relativePath.parent_path()) {
        if (isExcludedDirectory(component, selection)) {
            return "Excluded directory: " + pathToUtf8(component);
        }
    }

    const auto extension = pathToUtf8(relativePath.extension());

    const auto found = std::find(
        selection.extensions.begin(),
        selection.extensions.end(),
        extension
    );

    if (found == selection.extensions.end()) {
        return "File extension is not selected: " + extension;
    }

    return std::nullopt;
}

Result<std::filesystem::path> validateRoot(
        const std::filesystem::path& root
)
{
    if (root.empty()) {
        return Error{ErrorCode::invalid_input, "Root path is empty"};
    }

    std::error_code ec;
    const auto status = std::filesystem::status(root, ec);

    if (status.type() == std::filesystem::file_type::not_found) {
        return Error{ErrorCode::not_found, "Root directory does not exist"};
    }

    if (ec) {
        return Error{ErrorCode::read_error, "Cannot inspect root directory"};
    }

    if (!std::filesystem::is_directory(status)) {
        return Error{ErrorCode::invalid_input, "Root path is not a directory"};
    }

    return root;
}

Result<std::filesystem::path> validateRelativePath(
    const std::filesystem::path& path
)
{
    if (path.empty() || path.has_root_path()) {
        return Error{ErrorCode::invalid_input, "File path must be non-empty and relative"};
    }

    for (const auto& component : path) {
        if (component == "..") {
            return Error{ErrorCode::invalid_input, "File path must not contain '..'"};
        }
    }

    const auto normalized = path.lexically_normal();

    if (normalized == "." || !normalized.has_filename()) {
        return Error{ErrorCode::invalid_input, "File path must identify a file"};
    }

    return normalized;
}

std::optional<Error> nestedGitExclusion(
    const std::filesystem::path& directory
)
{
    std::error_code ec;
    const auto status =
        std::filesystem::symlink_status(directory / ".git", ec);

    if (status.type() == std::filesystem::file_type::not_found) {
        return std::nullopt;
    }

    if (ec) {
        return Error{
            ErrorCode::read_error,
            "Cannot inspect nested Git metadata"
        };
    }

    if (std::filesystem::exists(status)) {
        return Error{
            ErrorCode::invalid_input,
            "Nested Git working trees (including submodules) are not supported"
        };
    }

    return std::nullopt;
}

Result<std::filesystem::path> validateSourceFile(
    const std::filesystem::path& root,
    const std::filesystem::path& relativePath
)
{
    auto current = root;
    std::filesystem::file_status status;

    for (const auto& component : relativePath) {
        current /= component;

        std::error_code ec;
        status = std::filesystem::symlink_status(current, ec);

        if (status.type() == std::filesystem::file_type::not_found) {
            return Error{ErrorCode::not_found, "Selected file/directory does not exist"};
        }

        if (ec) {
            return Error{ErrorCode::read_error, "Cannot inspect selected file path"};
        }

        if (std::filesystem::is_symlink(status) ||
            status.type() == std::filesystem::file_type::junction) {
            return Error{
                ErrorCode::invalid_input,
                "Symbolic links and junctions are not supported"
            };
        }

        if (std::filesystem::is_directory(status)) {
            if (const auto error = nestedGitExclusion(current)) {
                return *error;
            }
        }
    }

    if (!std::filesystem::is_regular_file(status)) {
        return Error{ErrorCode::invalid_input, "Selected path is not a regular file"};
    }

    return current;
}

bool isValidUtf8(std::string_view bytes) {
    std::size_t index = 0;

    while (index < bytes.size()) {
        const auto first = static_cast<unsigned char>(bytes[index]);

        if (first <= 0x7F) {
            ++index;
            continue;
        }

        std::size_t continuationCount = 0;
        std::uint32_t codePoint = 0;
        std::uint32_t minimum = 0;

        if (first >= 0xC2 && first <= 0xDF) {
            continuationCount = 1;
            codePoint = first & 0x1F;
            minimum = 0x80;
        }
        else if (first >= 0xE0 && first <= 0xEF) {
            continuationCount = 2;
            codePoint = first & 0x0F;
            minimum = 0x800;
        }
        else if (first >= 0xF0 && first <= 0xF4) {
            continuationCount = 3;
            codePoint = first & 0x07;
            minimum = 0x10000;
        }
        else {
            return false;
        }

        if (bytes.size() - index <= continuationCount) {
            return false;
        }

        for (std::size_t offset = 1; offset <= continuationCount; ++offset) {
            const auto next = static_cast<unsigned char>(bytes[index+offset]);

            if ((next & 0xC0) != 0x80) {
                return false;
            }

            codePoint = (codePoint << 6) | (next & 0x3F);
        }

        if (codePoint < minimum ||
            codePoint > 0x10FFFF ||
            (codePoint >= 0xD800 && codePoint <= 0xDFFF)) {
                return false;
        }

        index += continuationCount + 1;
    }

    return true;
}

Result<std::string> readFileBytes(
    const std::filesystem::path& path,
    const JobContext& job
)
{
    std::ifstream input(path, std::ios::binary);

    if (!input.is_open()) {
        return Error{ErrorCode::read_error, "Cannot open file"};
    }

    std::string bytes;
    char buffer[4096];

    while (input) {
        if(job.isCancelled()) {
            return Cancelled{};
        }

        input.read(buffer, sizeof(buffer));

        bytes.append(
            buffer,
            static_cast<std::size_t>(input.gcount()));
    }

    if (input.bad() || !input.eof()) {
        return Error{ErrorCode::read_error, "Cannot read file"};
    }

    if (job.isCancelled()) {
        return Cancelled{};
    }

    const std::string_view view = bytes;

    if (view.starts_with("\xFF\xFE") ||
        view.starts_with("\xFE\xFF") ||
        view.starts_with(std::string_view{"\x00\x00\xFE\xFF", 4})) {
        return Error{
            ErrorCode::unsupported_encoding,
            "UTF-16/UTF-32 are not supported; expected UTF-8"
        };
    }

    if (view.find('\0') != std::string_view::npos) {
        return Error{
            ErrorCode::invalid_input,
            "Binary content detected: NUL byte"
        };
    }

    const bool validUtf8 = isValidUtf8(view);

    if (job.isCancelled()) {
        return Cancelled{};
    }

    if (!validUtf8) {
        return Error{
            ErrorCode::unsupported_encoding,
            "File content is not valid UTF-8"
        };
    }

    return bytes;
}

struct DirectoryScan {
    std::vector<std::string> relativePaths;
    Coverage coverage;
};

Result<DirectoryScan> scanDirectory(
    const std::filesystem::path& root,
    const FileSelection& selection,
    const JobContext& job
)
{
    if (job.isCancelled()) {
        return Cancelled{};
    }

    std::error_code ec;
    std::filesystem::recursive_directory_iterator iterator(
        root,
        std::filesystem::directory_options::none,
        ec
    );

    if (ec) {
        return Error{ErrorCode::read_error, "Cannot open root directory", "."};
    }

    const std::filesystem::recursive_directory_iterator end;
    DirectoryScan scan;
    std::size_t visited = 0;

    auto recordSkip = [&](Error error, bool incomplete) {
        if (incomplete) {
            scan.coverage.completeness = Completeness::partial;
        }

        ++scan.coverage.skippedElements;
        scan.coverage.diagnostics.push_back(std::move(error));
    };

    job.report("scan-directory", visited);

    while (iterator != end) {
        if (job.isCancelled()) {
            return Cancelled{scan.coverage.diagnostics};
        }

        const auto path = iterator->path();
        const auto relativePath = path.lexically_relative(root);
        const auto relativeText = pathToUtf8(relativePath);

        const auto status = iterator->symlink_status(ec);

        if (ec) {
            iterator.disable_recursion_pending();

            recordSkip(Error{
                ErrorCode::read_error,
                "Cannot inspect directory entry",
                relativeText
            }, true);
        }
        else if (std::filesystem::is_symlink(status) ||
                 status.type() == std::filesystem::file_type::junction) {
            iterator.disable_recursion_pending();

            recordSkip(Error{
                ErrorCode::invalid_input,
                "Symbolic links and junctions are not supported",
                relativeText
            }, true);
        }
        else if (std::filesystem::is_directory(status)) {
            if (isExcludedDirectory(path, selection)) {
                iterator.disable_recursion_pending();

                recordSkip(Error{
                    ErrorCode::invalid_input,
                    "Directory excluded by selection",
                    relativeText
                }, false);
            }
            else if (auto error = nestedGitExclusion(path)) {
                iterator.disable_recursion_pending();

                error->path = relativeText;
                recordSkip(std::move(*error), true);
            }
        }
        else if (std::filesystem::is_regular_file(status)) {
            scan.relativePaths.push_back(relativeText);
        }
        else {
            recordSkip(Error{
                ErrorCode::invalid_input,
                "Directory entry is not a regular file",
                relativeText
            }, true);
        }

        job.report("scan-directory", ++visited);

        if (job.isCancelled()) {
            return Cancelled{scan.coverage.diagnostics};
        }

        iterator.increment(ec);

        if (ec) {
            recordSkip(Error{
                ErrorCode::read_error,
                "Directory traversal stopped before completion",
                relativeText
            }, true);
            break;
        }
    }

    if (job.isCancelled()) {
        return Cancelled{scan.coverage.diagnostics};
    }

    std::sort(scan.relativePaths.begin(), scan.relativePaths.end());

    if (job.isCancelled()) {
        return Cancelled{scan.coverage.diagnostics};
    }

    return scan;
}


} // namespace

Result<SourceSnapshot> DirectoryProvider::collect(
    const SourceRequest &request,
    const JobContext &job
)
{
    if (job.isCancelled()) {
        return Cancelled{};
    }

    auto rootResult = validateRoot(request.root);

    if (const auto* error = std::get_if<Error>(&rootResult)) {
        return *error;
    }

    const auto& root = std::get<std::filesystem::path>(rootResult);

    auto scanResult = scanDirectory(root, request.selection, job);

    if (const auto* error = std::get_if<Error>(&scanResult)) {
        return *error;
    }

    if (const auto* cancelled = std::get_if<Cancelled>(&scanResult)) {
            return *cancelled;
    }

    const auto& scan = std::get<DirectoryScan>(scanResult);

    auto filesRequest = request;
    filesRequest.selection.relativePaths = scan.relativePaths;

    FileListProvider provider;
    auto result = provider.collect(filesRequest, job);

    if (auto* snapshot = std::get_if<SourceSnapshot>(&result)) {
        snapshot->selection = request.selection;

        snapshot->coverage.skippedElements += scan.coverage.skippedElements;

        if (scan.coverage.completeness == Completeness::partial) {
            snapshot->coverage.completeness = Completeness::partial;
        }

        snapshot->coverage.diagnostics.insert(
            snapshot->coverage.diagnostics.begin(),
            scan.coverage.diagnostics.begin(),
            scan.coverage.diagnostics.end()
        );
    }
    else if (auto* cancelled = std::get_if<Cancelled>(&result)) {
        cancelled->diagnostics.insert(
            cancelled->diagnostics.begin(),
            scan.coverage.diagnostics.begin(),
            scan.coverage.diagnostics.end()
        );
    }

    return result;
}

Result<SourceSnapshot> FileListProvider::collect(
    const SourceRequest& request,
    const JobContext& job
)
{
    if (job.isCancelled()) {
        return Cancelled{};
    }

    auto rootResult = validateRoot(request.root);

    if (const auto* error = std::get_if<Error>(&rootResult)) {
        return *error;
    }

    const auto& root = std::get<std::filesystem::path>(rootResult);

    SourceSnapshot snapshot;
    snapshot.id = makeSnapshotId();
    snapshot.kind = SourceKind::local;
    snapshot.selection = request.selection;

    auto recordSkipped = [&](Error error, const std::string& path) {
        error.path = path;
        snapshot.coverage.completeness = Completeness::partial;
        ++snapshot.coverage.skippedElements;
        snapshot.coverage.diagnostics.push_back(std::move(error));
    };

    std::unordered_set<std::string> seenPaths;

    const auto total = request.selection.relativePaths.size();

    for (std::size_t index = 0; index < total; index++) {
        if (job.isCancelled()) {
            return Cancelled{snapshot.coverage.diagnostics};
        }

        job.report("collect-files", index, total);

        if (job.isCancelled()) {
            return Cancelled{snapshot.coverage.diagnostics};
        }

        const auto& selectedPath = request.selection.relativePaths[index];

        if (selectedPath.find('\0') != std::string::npos ||
            !isValidUtf8(selectedPath)) {
            recordSkipped(
                Error{
                    ErrorCode::invalid_input,
                    "File path must be valid UTF-8 without NUL bytes"
                },
                selectedPath
            );
            continue;
        }

        const std::u8string utf8Path(selectedPath.begin(), selectedPath.end());

        auto relativeResult = validateRelativePath(std::filesystem::path(utf8Path));

        if (const auto* error = std::get_if<Error>(&relativeResult)) {
            recordSkipped(*error, selectedPath);
            continue;
        }

        const auto& relativePath = std::get<std::filesystem::path>(relativeResult);

        const auto normalizedPath = pathToUtf8(relativePath);

        if (!seenPaths.insert(normalizedPath).second) {
            continue;
        }

        if (const auto reason = filterExclusionReason(relativePath, request.selection)) {
            Error diagnostic{
                ErrorCode::invalid_input,
                *reason
            };

            diagnostic.path = selectedPath;

            snapshot.coverage.diagnostics.push_back(std::move(diagnostic));
            ++snapshot.coverage.skippedElements;
            continue;
        }

        auto fileResult = validateSourceFile(root, relativePath);

        if (const auto* error = std::get_if<Error>(&fileResult)) {
            recordSkipped(*error, selectedPath);
            continue;
        }

        auto bytesResult = readFileBytes(std::get<std::filesystem::path>(fileResult), job);

        if (std::holds_alternative<Cancelled>(bytesResult)) {
            return Cancelled{snapshot.coverage.diagnostics};
        }

        if (const auto* error = std::get_if<Error>(&bytesResult)) {
            recordSkipped(*error, selectedPath);
            continue;
        }

        SourceFile file;
        file.relativePath = normalizedPath;

        file.bytes = std::make_shared<const std::string>(std::move(std::get<std::string>(bytesResult)));

        auto contentIdResult = makeContentId(*file.bytes, job);

        if (std::holds_alternative<Cancelled>(contentIdResult)) {
            return Cancelled{snapshot.coverage.diagnostics};
        }

        file.contentId = std::move(std::get<std::string>(contentIdResult));

        snapshot.files.push_back(std::move(file));
    }

    if (job.isCancelled()) {
        return Cancelled{snapshot.coverage.diagnostics};
    }

    job.report("collect-files", total, total);

    if (job.isCancelled()) {
        return Cancelled{snapshot.coverage.diagnostics};
    }

    return snapshot;

}

Result<SourceSnapshot> collect(
    const SourceRequest& request,
    const JobContext& job
)
{
    if(job.isCancelled()) {
        return Cancelled{};
    }

    if(request.selection.relativePaths.empty()) {
        DirectoryProvider directory;
        SourceProvider& provider = directory;
        return provider.collect(request, job);
    }

    FileListProvider files;
    SourceProvider& provider = files;

    return provider.collect(request, job);
}

}
