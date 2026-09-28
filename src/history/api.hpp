#pragma once

#include "history/types.hpp"
#include "common/job.hpp"
#include "common/result.hpp"
#include "code/types.hpp"
#include <memory>

namespace xray::history {

    Result<HistoryResult> query(const RepositorySpec& spec,
        const HistoryRequest& request,
        const JobContext& ctx);

    Result<code::SourceSnapshot> loadSnapshot(const RepositorySpec& spec,
        const Oid& oid,
        const code::FileSelection& selection,
        const JobContext& ctx);

    class HistoryService {
    public:
        virtual ~HistoryService() = default;

        [[nodiscard]] virtual Result<HistoryResult> fetchHistory(
            RepositorySpec spec,
            HistoryRequest request,
            const JobContext& ctx) = 0;
    };

    [[nodiscard]] std::unique_ptr<HistoryService> createHistoryService();

} // namespace xray::history