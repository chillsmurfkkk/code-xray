#pragma once

#include "history/types.hpp"
#include "common/job.hpp"
#include "common/result.hpp"
#include "code/types.hpp"

namespace xray::history {


    Result<HistoryResult> query(const RepositorySpec& spec,
        const HistoryQuery& querySpec,
        const RevisionSelection& selection,
        const JobContext& ctx);

    Result<code::SourceSnapshot> loadSnapshot(const RepositorySpec& spec,
        const Oid& oid,
        const code::FileSelection& selection,
        const JobContext& ctx);

} // namespace xray::history