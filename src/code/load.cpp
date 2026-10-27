#include "code/api.hpp"

#include <memory>
#include <utility>
#include <variant>

namespace xray::code {

Result<ParsedSnapshot> collectAndParse(
    const SourceRequest& request,
    const ParseOptions& options,
    const JobContext& context
) {
    auto collected = collect(request, context);

    if (const auto* error = std::get_if<Error>(&collected)) {
        return *error;
    }

    if (const auto* cancelled = std::get_if<Cancelled>(&collected)) {
        return *cancelled;
    }

    auto source = std::make_shared<const SourceSnapshot>(
        std::move(std::get<SourceSnapshot>(collected))
    );

    return parse(std::move(source), options, context);
}

} // namespace xray::code
