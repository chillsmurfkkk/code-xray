#pragma once

#include "code/types.hpp"
#include "common/job.hpp"

namespace xray::code {
// Synchronous worker operations. Definitions are delivered by A after E0.
Result<SourceSnapshot> collect(const SourceRequest&, const JobContext&);
Result<ParsedSnapshot> parse(std::shared_ptr<const SourceSnapshot>, const ParseOptions&, const JobContext&);

class SourceProvider {
public:
    virtual ~SourceProvider() = default;
    virtual Result<SourceSnapshot> collect(const SourceRequest&, const JobContext&) = 0;
};
class DirectoryProvider : public SourceProvider {
public:
    Result<SourceSnapshot> collect(const SourceRequest&, const JobContext&) override;
};
class FileListProvider : public SourceProvider {
public:
    Result<SourceSnapshot> collect(const SourceRequest&, const JobContext&) override;
};
} // namespace xray::code
