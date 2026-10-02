#include "code/api.hpp"
#include "code/types.hpp"
#include "common/job.hpp"
#include "common/result.hpp"
#include <algorithm>
#include <cstddef>
#include <fstream>
#include <string>
#include <filesystem>
#include <system_error>
#include <memory>
#include <utility>
#include <variant>

namespace xray::code {

namespace {

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

        if (std::filesystem::is_symlink(status)) {
            return Error{ErrorCode::invalid_input, "Symbolic links are not supported"};
        }
    }

    if (!std::filesystem::is_regular_file(status)) {
        return Error{ErrorCode::invalid_input, "Selected path is not a regular file"};
    }

    return current;
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

    return bytes;
}

} // namespace

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
    snapshot.kind = SourceKind::local;
    snapshot.selection = request.selection;

    auto recordSkipped = [&](Error error, const std::string& path) {
        error.path = path;
        snapshot.coverage.completeness = Completeness::partial;
        ++snapshot.coverage.skippedElements;
        snapshot.coverage.diagnostics.push_back(std::move(error));
    };

    for (const auto& selectedPath : request.selection.relativePaths) {
        if (job.isCancelled()) {
            return Cancelled{snapshot.coverage.diagnostics};
        }

        const std::u8string utf8Path(selectedPath.begin(), selectedPath.end());

        auto relativeResult = validateRelativePath(std::filesystem::path(utf8Path));

        if (const auto* error = std::get_if<Error>(&relativeResult)) {
            recordSkipped(*error, selectedPath);
            continue;
        }

        const auto& relativePath = std::get<std::filesystem::path>(relativeResult);

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
        const auto normalizedPath = relativePath.generic_u8string();
        file.relativePath.assign(normalizedPath.begin(), normalizedPath.end());

        file.bytes = std::make_shared<const std::string>(std::move(std::get<std::string>(bytesResult)));

        snapshot.files.push_back(std::move(file));
    }

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
        DirectoryProvider provider;
        return provider.collect(request, job);
    }

    FileListProvider provider;
    return provider.collect(request, job);
}

}
