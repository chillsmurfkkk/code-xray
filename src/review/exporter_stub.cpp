#include "review/api.hpp"

namespace xray::review {

Result<ExportReceipt> exportReport(const ComparisonReport&,
                                   const ExportOptions&,
                                   const JobContext&)
{
    return Error{ErrorCode::invalid_input, "exportReport is not available in contracts-only build mode."};
}

} // namespace xray::review
