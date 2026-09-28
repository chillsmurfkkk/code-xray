#pragma once

#include "history/types.hpp"
#include "common/job.hpp"
#include "common/result.hpp"
#include "code/types.hpp"

namespace xray::history {

    Result<HistoryResult> query(const RepositorySpec& spec,
        const HistoryRequest& request,
        const JobContext& ctx);

    Result<code::SourceSnapshot> loadSnapshot(const RepositorySpec& spec,
        const Oid& oid,
        const code::FileSelection& selection,
        const JobContext& ctx);

} // namespace xray::history