#include "code/api.hpp"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <future>
#include <iostream>
#include <stdexcept>
#include <stop_token>
#include <variant>

namespace {
void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

class TemporaryDirectory {
public:
    TemporaryDirectory() {
        const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
        path = std::filesystem::temp_directory_path() / ("code-xray-load-" + std::to_string(stamp));
        require(std::filesystem::create_directory(path), "Could not create the source fixture");
    }
    ~TemporaryDirectory() {
        std::error_code error;
        std::filesystem::remove(path / "example.cpp", error);
        std::filesystem::remove(path, error);
    }
    TemporaryDirectory(const TemporaryDirectory&) = delete;
    TemporaryDirectory& operator=(const TemporaryDirectory&) = delete;
    std::filesystem::path path;
};
}

int main() {
    try {
        using namespace xray;
        TemporaryDirectory directory;
        const std::string source = "int square(int x) { return x * x; }\n";
        {
            std::ofstream file(directory.path / "example.cpp", std::ios::binary);
            file << source;
            require(static_cast<bool>(file), "Could not write the source fixture");
        }

        code::SourceRequest request;
        request.root = directory.path;
        auto success = std::async(std::launch::async, [&] {
            return code::collectAndParse(request, {}, {});
        }).get();
        const auto* snapshot = std::get_if<code::ParsedSnapshot>(&success);
        require(snapshot && snapshot->source && snapshot->units.size() == 1,
                "A valid directory must produce a parsed snapshot with its source");
        require(snapshot->units.front().root && snapshot->units.front().root->sourceBytes
                    && *snapshot->units.front().root->sourceBytes == source,
                "The parsed result must retain the collected bytes");
        require(snapshot->units.front().functions.size() == 1
                    && snapshot->units.front().functions.front()->name == "square",
                "The parsed function must be available to the GUI");

        auto missing = request;
        missing.root /= "missing";
        auto missingResult = code::collectAndParse(missing, {}, {});
        const auto* missingError = std::get_if<Error>(&missingResult);
        require(missingError && missingError->code == ErrorCode::not_found,
                "Collection errors must be returned to the caller");

        code::ParseOptions unsupported;
        unsupported.structureVersion = "unsupported";
        auto parseErrorResult = code::collectAndParse(request, unsupported, {});
        const auto* parseError = std::get_if<Error>(&parseErrorResult);
        require(parseError && parseError->code == ErrorCode::invalid_input,
                "Parser errors must be returned to the caller");

        for (const auto* stage : {"scan-directory", "parse-files"}) {
            std::stop_source stop;
            JobContext job;
            job.stopToken = stop.get_token();
            bool stageReached = false;
            job.onProgress = [&](const Progress& progress) {
                if (progress.stage == stage) {
                    stageReached = true;
                    stop.request_stop();
                }
            };
            auto cancelled = std::async(std::launch::async, [&] {
                return code::collectAndParse(request, {}, job);
            }).get();
            require(stageReached && std::holds_alternative<Cancelled>(cancelled),
                    "Cancellation during collection or parsing must not return a snapshot");
        }

        std::cout << "Source loading, errors and cancellation: passed\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
