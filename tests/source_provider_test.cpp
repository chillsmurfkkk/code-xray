#include "code/api.hpp"
#include "common/job.hpp"

#include <chrono>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <system_error>
#include <variant>
#include <fstream>
#include <string_view>
#include <stop_token>
#include <algorithm>

namespace {

void check(bool condition, const char* message)
{
    if (!condition) {
        throw std::runtime_error(message);
    }
}

class TempDirectory {
public:
    TempDirectory()
    {
        const auto stamp =
            std::chrono::steady_clock::now()
                .time_since_epoch().count();

        path = std::filesystem::temp_directory_path() /
            ("code-xray-source-test-" + std::to_string(stamp));

        check(
            std::filesystem::create_directory(path),
            "Cannot create a fresh test directory"
        );
    }

    ~TempDirectory()
    {
        std::error_code ec;
        std::filesystem::remove_all(path, ec);
    }

    TempDirectory(const TempDirectory&) = delete;
    TempDirectory& operator=(const TempDirectory&) = delete;

    std::filesystem::path path;
};

void writeFile(
    const std::filesystem::path& path,
    std::string_view bytes
)
{
    std::filesystem::create_directories(path.parent_path());

    std::ofstream output(path, std::ios::binary);
    check(output.is_open(), "Cannot create test file");

    output.write(
        bytes.data(),
        static_cast<std::streamsize>(bytes.size())
    );

    output.close();
    check(static_cast<bool>(output), "Cannot write test file");
}

void emptyDirectoryIsSuccessful()
{
    TempDirectory directory;

    xray::code::SourceRequest request;
    request.root = directory.path;

    auto result = xray::code::collect(request, xray::JobContext{});

    const auto* snapshot =
        std::get_if<xray::code::SourceSnapshot>(&result);

    check(snapshot != nullptr, "Empty directory must be a success");
    check(snapshot->files.empty(), "Snapshot must contain no files");
    check(
        snapshot->coverage.completeness == xray::Completeness::complete,
        "Empty directory must have complete coverage"
    );
    check(
        snapshot->coverage.skippedElements == 0,
        "Empty directory must have no skipped entries"
    );
    check(
        snapshot->coverage.diagnostics.empty(),
        "Empty directory must have no diagnostics"
    );
}

void sourceBytesArePreserved()
{
    TempDirectory directory;

    const std::string expected =
        "\xEF\xBB\xBF"
        "// Привіт\r\n"
        "int main() { return 0; }\r\n";

    const auto filePath = directory.path / "src" / "main.cpp";
    writeFile(filePath, expected);

    xray::code::SourceRequest request;
    request.root = directory.path;

    auto result = xray::code::collect(request, xray::JobContext{});

    const auto* snapshot =
        std::get_if<xray::code::SourceSnapshot>(&result);

    check(snapshot != nullptr, "UTF-8 file must be accepted");
    check(snapshot->files.size() == 1, "Expected exactly one file");

    const auto& file = snapshot->files.front();

    check(
        file.relativePath == "src/main.cpp",
        "Source path must be relative with forward slashes"
    );
    check(file.bytes != nullptr, "Source bytes must be retained");
    check(*file.bytes == expected, "BOM and CRLF must be preserved");

    check(
        snapshot->selection == request.selection,
        "Snapshot must retain the original selection"
    );
    check(
        snapshot->coverage.completeness == xray::Completeness::complete,
        "Valid file must have complete coverage"
    );
    check(
        snapshot->coverage.skippedElements == 0 &&
        snapshot->coverage.diagnostics.empty(),
        "Valid file must have no skips or diagnostics"
    );

    check(
        std::filesystem::remove(filePath),
        "Cannot remove test source file"
    );
    check(
        *file.bytes == expected,
        "Snapshot bytes must survive source file removal"
    );
}

void invalidContentIsSkipped(
    std::string_view invalidBytes,
    xray::ErrorCode expectedCode
)
{
    TempDirectory directory;

    const std::string validBytes = "int value = 42;\n";

    writeFile(directory.path / "good.cpp", validBytes);
    writeFile(directory.path / "bad.cpp", invalidBytes);

    xray::code::SourceRequest request;
    request.root = directory.path;

    auto result = xray::code::collect(request, xray::JobContext{});

    const auto* snapshot =
        std::get_if<xray::code::SourceSnapshot>(&result);

    check(snapshot != nullptr, "Skipped file must allow partial success");
    check(snapshot->files.size() == 1, "Only the valid file must remain");

    const auto& file = snapshot->files.front();

    check(file.relativePath == "good.cpp", "Wrong file was retained");
    check(
        file.bytes && *file.bytes == validBytes,
        "Valid file bytes must be preserved"
    );

    check(
        snapshot->coverage.completeness == xray::Completeness::partial,
        "Rejected content must produce partial coverage"
    );
    check(
        snapshot->coverage.skippedElements == 1,
        "Exactly one file must be skipped"
    );
    check(
        snapshot->coverage.diagnostics.size() == 1,
        "Skipped file must have one diagnostic"
    );

    const auto& diagnostic = snapshot->coverage.diagnostics.front();

    check(diagnostic.code == expectedCode, "Wrong diagnostic code");
    check(
        diagnostic.path && *diagnostic.path == "bad.cpp",
        "Diagnostic must identify the skipped file"
    );
    check(!diagnostic.message.empty(), "Skip reason must be provided");
}

void explicitSelectionDeduplicatesAndFilters()
{
    TempDirectory directory;

    writeFile(directory.path / "src" / "main.cpp", "int value;\n");
    writeFile(directory.path / "src" / "other.hpp", "int other;\n");
    writeFile(directory.path / "notes.txt", "Project notes\n");
    writeFile(directory.path / "build" / "generated.cpp", "int generated;\n");

    xray::code::SourceRequest request;
    request.root = directory.path;
    request.selection.relativePaths = {
        "src/main.cpp",
        "src/./main.cpp",
        "src/main.cpp",
        "notes.txt",
        "build/generated.cpp"
    };

    auto result = xray::code::collect(request, xray::JobContext{});

    const auto* snapshot =
        std::get_if<xray::code::SourceSnapshot>(&result);

    check(snapshot != nullptr, "Explicit selection must succeed");
    check(
        snapshot->files.size() == 1,
        "Duplicates and filtered files must not enter the snapshot"
    );
    check(
        snapshot->files.front().relativePath == "src/main.cpp",
        "Selected path must be normalized"
    );
    check(
        snapshot->selection == request.selection,
        "Original selection must be retained"
    );
    check(
        snapshot->coverage.completeness == xray::Completeness::complete,
        "Selection filters must not make coverage partial"
    );
    check(
        snapshot->coverage.skippedElements == 2,
        "Only the two filtered files must count as skipped"
    );
    check(
        snapshot->coverage.diagnostics.size() == 2,
        "Each filtered file must have a diagnostic"
    );
}

void invalidSelectedPathIsSkipped(
    std::string_view selectedPath,
    xray::ErrorCode expectedCode
)
{
    TempDirectory directory;
    writeFile(directory.path / "good.cpp", "int value;\n");

    xray::code::SourceRequest request;
    request.root = directory.path;
    request.selection.relativePaths = {
        "good.cpp",
        std::string(selectedPath)
    };

    auto result = xray::code::collect(request, xray::JobContext{});

    const auto* snapshot =
        std::get_if<xray::code::SourceSnapshot>(&result);

    check(snapshot != nullptr, "Invalid path must allow partial success");
    check(snapshot->files.size() == 1, "Valid file must remain");
    check(
        snapshot->files.front().relativePath == "good.cpp",
        "Wrong file was retained"
    );
    check(
        snapshot->coverage.completeness == xray::Completeness::partial,
        "Invalid selected path must make coverage partial"
    );
    check(
        snapshot->coverage.skippedElements == 1 &&
        snapshot->coverage.diagnostics.size() == 1,
        "Invalid path must produce exactly one skip and diagnostic"
    );

    const auto& diagnostic = snapshot->coverage.diagnostics.front();

    check(diagnostic.code == expectedCode, "Wrong path error code");
    check(
        diagnostic.path && *diagnostic.path == selectedPath,
        "Diagnostic must identify the rejected path"
    );
}

void cancellationNeverReturnsSnapshot()
{
    TempDirectory directory;
    writeFile(directory.path / "good.cpp", "int good;\n");
    writeFile(directory.path / "later.cpp", "int later;\n");

    xray::code::SourceRequest request;
    request.root = directory.path;

    std::stop_source beforeStart;
    beforeStart.request_stop();

    xray::JobContext cancelledJob;
    cancelledJob.stopToken = beforeStart.get_token();

    auto beforeResult = xray::code::collect(request, cancelledJob);

    check(
        std::holds_alternative<xray::Cancelled>(beforeResult),
        "Cancellation before start must return Cancelled"
    );

    request.selection.relativePaths = {
        "missing.cpp",
        "good.cpp",
        "later.cpp"
    };

    std::stop_source duringCollection;

    xray::JobContext activeJob;
    activeJob.stopToken = duringCollection.get_token();
    activeJob.onProgress = [&](const xray::Progress& progress) {
        if (progress.stage == "collect-files" &&
            progress.completed == 2) {
            duringCollection.request_stop();
        }
    };

    auto duringResult = xray::code::collect(request, activeJob);

    check(
        duringCollection.stop_requested(),
        "Test must reach the cancellation point"
    );

    const auto* cancelled =
        std::get_if<xray::Cancelled>(&duringResult);

    check(cancelled != nullptr, "Cancellation must discard the snapshot");
    check(
        cancelled->diagnostics.size() == 1,
        "Cancellation must retain earlier diagnostics"
    );

    const auto& diagnostic = cancelled->diagnostics.front();

    check(
        diagnostic.code == xray::ErrorCode::not_found &&
        diagnostic.path && *diagnostic.path == "missing.cpp",
        "Earlier missing-file diagnostic must be preserved"
    );
}

void snapshotAndContentIdsBehaveCorrectly()
{
    TempDirectory directory;
    const auto path = directory.path / "main.cpp";

    writeFile(path, "int first;\n");

    xray::code::SourceRequest request;
    request.root = directory.path;

    auto collectSnapshot = [&] {
        auto result = xray::code::collect(request, xray::JobContext{});

        const auto* snapshot =
            std::get_if<xray::code::SourceSnapshot>(&result);

        check(snapshot != nullptr, "Collection must succeed");
        check(snapshot->files.size() == 1, "Expected one source file");

        return *snapshot;
    };

    const auto first = collectSnapshot();
    const auto second = collectSnapshot();

    check(!first.id.empty(), "Snapshot ID must be populated");
    check(!second.id.empty(), "Repeated snapshot ID must be populated");
    check(first.id != second.id, "Separate collections must have different IDs");

    const auto& firstContentId = first.files.front().contentId;

    check(!firstContentId.empty(), "Content ID must be populated");
    check(
        firstContentId == second.files.front().contentId,
        "Unchanged bytes must retain the same content ID"
    );

    writeFile(path, "int second;\n");
    const auto changed = collectSnapshot();

    check(
        firstContentId != changed.files.front().contentId,
        "These different contents must have different content IDs"
    );
}

void nestedGitTreesAreSkipped()
{
    TempDirectory directory;

    std::filesystem::create_directory(directory.path / ".git");

    writeFile(directory.path / "good.cpp", "int good;\n");
    writeFile(
        directory.path / "module" / ".git",
        "gitdir: ../.git/modules/module\n"
    );
    writeFile(
        directory.path / "module" / "src" / "hidden.cpp",
        "int hidden;\n"
    );

    const bool modes[] = {false, true};

    for (bool explicitList : modes) {
        xray::code::SourceRequest request;
        request.root = directory.path;

        if (explicitList) {
            request.selection.relativePaths = {
                "good.cpp",
                "module/src/hidden.cpp"
            };
        }

        auto result = xray::code::collect(request, xray::JobContext{});

        const auto* snapshot =
            std::get_if<xray::code::SourceSnapshot>(&result);

        check(snapshot != nullptr, "Nested Git tree must allow partial success");
        check(snapshot->files.size() == 1, "Nested source must be skipped");
        check(
            snapshot->files.front().relativePath == "good.cpp",
            "Root repository source must remain"
        );
        check(
            snapshot->coverage.completeness == xray::Completeness::partial,
            "Unsupported nested Git tree must make coverage partial"
        );

        const std::size_t expectedSkips = explicitList ? 1 : 2;

        check(
            snapshot->coverage.skippedElements == expectedSkips &&
            snapshot->coverage.diagnostics.size() == expectedSkips,
            "Unexpected skips or diagnostics"
        );

        const std::string expectedPath =
            explicitList ? "module/src/hidden.cpp" : "module";

        const auto& diagnostics = snapshot->coverage.diagnostics;

        const auto found = std::find_if(
            diagnostics.begin(),
            diagnostics.end(),
            [&](const xray::Error& error) {
                return error.path && *error.path == expectedPath;
            }
        );

        check(found != diagnostics.end(), "Nested Git diagnostic is missing");
        check(
            found->code == xray::ErrorCode::invalid_input,
            "Nested Git tree must have an unsupported-input diagnostic"
        );
    }
}

} // namespace

int main()
{
    try {
        emptyDirectoryIsSuccessful();
        sourceBytesArePreserved();
        explicitSelectionDeduplicatesAndFilters();
        cancellationNeverReturnsSnapshot();
        snapshotAndContentIdsBehaveCorrectly();
        nestedGitTreesAreSkipped();

        invalidSelectedPathIsSkipped(
            "\xFF.cpp",
            xray::ErrorCode::invalid_input
        );

        invalidSelectedPathIsSkipped(
            std::string_view{"good.cpp\0ignored.cpp", 20},
            xray::ErrorCode::invalid_input
        );

        invalidSelectedPathIsSkipped(
            "missing.cpp",
            xray::ErrorCode::not_found
        );

        invalidSelectedPathIsSkipped(
            "../outside.cpp",
            xray::ErrorCode::invalid_input
        );

        // Незавершена трьохбайтова послідовність UTF-8.
        invalidContentIsSkipped(
            "\xE2\x82",
            xray::ErrorCode::unsupported_encoding
        );

        // UTF-16 LE із BOM та символом A.
        invalidContentIsSkipped(
            std::string_view{"\xFF\xFE\x41\x00", 4},
            xray::ErrorCode::unsupported_encoding
        );

        // Вміст із нульовим байтом.
        invalidContentIsSkipped(
            std::string_view{"a\0b", 3},
            xray::ErrorCode::invalid_input
        );
    }
    catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }

    return 0;
}
