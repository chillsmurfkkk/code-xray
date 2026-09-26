#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <stop_token>
#include <string>
#include <utility>

namespace xray {
using JobId = std::uint64_t;
struct Progress {
    JobId jobId = 0;
    std::string stage;
    std::size_t completed = 0;
    std::optional<std::size_t> total;
};

// Callbacks run on the worker thread. app queues delivery to the GUI thread.
struct JobContext {
    JobId jobId = 0;
    std::stop_token stopToken;
    std::function<void(const Progress&)> onProgress;

    [[nodiscard]] bool isCancelled() const noexcept { return stopToken.stop_requested(); }
    void report(std::string stage, std::size_t completed,
                std::optional<std::size_t> total = std::nullopt) const {
        if (onProgress) {
            onProgress(Progress{jobId, std::move(stage), completed, total});
        }
    }
};
} // namespace xray
