#include "code/api.hpp"
#include "code/types.hpp"
#include "common/job.hpp"
#include "common/result.hpp"

#include <cstddef>
#include <tree_sitter/api.h>

#include <memory>
#include <cstdint>
#include <limits>
#include <optional>
#include <string_view>

extern "C" const TSLanguage* tree_sitter_cpp();

namespace xray::code {

namespace {

using ParserPtr = std::unique_ptr<TSParser, decltype(&ts_parser_delete)>;
using TreePtr = std::unique_ptr<TSTree, decltype(&ts_tree_delete)>;

Result<ParserPtr> createParser() {
    ParserPtr parser(ts_parser_new(), ts_parser_delete);

    if(!parser) {
        return Error{ErrorCode::parse_incomplete, "Cannot create tree-sitter parser"};
    }

    if (!ts_parser_set_language(parser.get(), tree_sitter_cpp())) {
        return Error{
            ErrorCode::parse_incomplete,
            "Tree-sitter runtime and C++ grammar are incompatible"
        };
    }

    return parser;
}

Result<TreePtr> parseTree(
    TSParser& parser,
    const SourceFile& file,
    const JobContext& job
)
{
    if (job.isCancelled()) {
        return Cancelled{};
    }

    if (!file.bytes) {
        return Error{
            ErrorCode::invalid_input,
            "Source file has no byte buffer",
            file.relativePath
        };
    }

    const auto& bytes = *file.bytes;

    if (bytes.size() > std::numeric_limits<std::uint32_t>::max()) {
        return Error{
            ErrorCode::invalid_input,
            "Source file exceeds Tree-sitter byte limit",
            file.relativePath
        };
    }

    TreePtr tree(
        ts_parser_parse_string(
            &parser,
            nullptr,
            bytes.data(),
            static_cast<std::uint32_t>(bytes.size())
        ),
        ts_tree_delete
    );

    if (job.isCancelled()) {
        return Cancelled{};
    }

    if (!tree) {
        return Error{
            ErrorCode::parse_incomplete,
            "Tree-sitter did not produce a syntax tree",
            file.relativePath
        };
    }

    return tree;
}

std::optional<std::string_view> nodeText(
    TSNode node,
    std::string_view source
)
{
    if (ts_node_is_null(node)) {
        return std::nullopt;
    }

    const auto start = ts_node_start_byte(node);
    const auto end = ts_node_end_byte(node);

    if (start > end || end > source.size()) {
        return std::nullopt;
    }

    return source.substr(start, end-start);
}

SourceRange nodeRange(TSNode node) {
    const auto start = ts_node_start_point(node);
    const auto end = ts_node_end_point(node);

    SourceRange range;
    range.startByte = ts_node_start_byte(node);
    range.endByte = ts_node_end_byte(node);
    range.startLine = start.row + 1;
    range.endLine = end.row + 1;


}

} // namespace


} // namespace xray::code
