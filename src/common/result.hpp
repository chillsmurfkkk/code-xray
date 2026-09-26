#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace xray {
enum class ErrorCode {
    invalid_input, not_found, unsupported_encoding, read_error, parse_incomplete,
    incompatible_analysis, ambiguous_match, export_error
};

struct Error {
    ErrorCode code = ErrorCode::invalid_input;
    std::string message;
    std::optional<std::string> path;
    std::optional<std::string> commit;
    std::optional<std::string> rule;
};

struct Cancelled {
    std::vector<Error> diagnostics;
};

// A partial success is T with partial Coverage; cancellation never contains T.
template <class T>
using Result = std::variant<T, Error, Cancelled>;

enum class Completeness { complete, partial };
struct Coverage {
    Completeness completeness = Completeness::complete;
    std::size_t skippedElements = 0;
    std::vector<Error> diagnostics;
};
} // namespace xray
