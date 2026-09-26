#pragma once

#include "common/job.hpp"
#include "history/types.hpp"

namespace xray::history {
Result<HistoryResult> query(const RepositorySpec&, const HistoryRequest&, const JobContext&);
Result<code::SourceSnapshot> loadSnapshot(const RepositorySpec&, const Oid&,
                                         const code::FileSelection&, const JobContext&);
} // namespace xray::history
