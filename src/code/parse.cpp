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
#include <string>

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

    if (range.endByte > range.startByte && end.column == 0) {
        range.endLine = end.row;
    }

    return range;
}

TSNode findDeclaratorName(TSNode node) {
    if (ts_node_is_null(node)) {
        return {};
    }

    const std::string_view kind = ts_node_type(node);

    if (kind == "identifier" ||
        kind == "field_identifier" ||
        kind == "type_identifier" ||
        kind == "qualified_identifier" ||
        kind == "destructor_name" ||
        kind == "operator_name" ||
        kind == "template_function" ||
        kind == "template_method") {
        return node;
    }

    const auto nested = ts_node_child_by_field_name(
        node,
        "declarator",
        10
    );

    if (!ts_node_is_null(nested)) {
        return findDeclaratorName(nested);
    }

    if (kind == "reference_declarator" ||
        kind == "parenthesized_declarator" ||
        kind == "attributed_declarator") {
        const auto count = ts_node_named_child_count(node);

        for (std::uint32_t index = 0; index < count; ++index) {
            const auto child = ts_node_named_child(node, index);
            const auto name = findDeclaratorName(child);

            if (!ts_node_is_null(name)) {
                return name;
            }
        }
    }

    return {};
}

struct FunctionName {
    std::string name;
    std::string ownerName;
    bool globallyQualified = false;
};

std::optional<FunctionName> extractQualifiedName(
    TSNode node,
    std::string_view source
)
{
    if (ts_node_is_null(node)) {
        return std::nullopt;
    }

    FunctionName result;
    bool first = true;

    while (std::string_view(ts_node_type(node)) == "qualified_identifier") {
        const auto scope = ts_node_child_by_field_name(node, "scope", 5);

        if (first) {
            result.globallyQualified = ts_node_is_null(scope);
            first = false;
        }

        if (!ts_node_is_null(scope)) {
            const auto scopeText = nodeText(scope, source);

            if (!scopeText || scopeText->empty()) {
                return std::nullopt;
            }

            if (!result.ownerName.empty()) {
                result.ownerName += "::";
            }

            result.ownerName += *scopeText;
        }

        node = ts_node_child_by_field_name(node, "name", 4);

        if (ts_node_is_null(node)) {
            return std::nullopt;
        }
    }

    const std::string_view kind = ts_node_type(node);

    if (kind != "identifier" &&
        kind != "field_identifier" &&
        kind != "type_identifier" &&
        kind != "destructor_name" &&
        kind != "operator_name" &&
        kind != "template_function" &&
        kind != "template_method") {
        return std::nullopt;
    }

    const auto text = nodeText(node, source);

    if (!text || text->empty()) {
        return std::nullopt;
    }

    result.name = std::string(*text);
    return result;
}

Result<std::shared_ptr<FunctionEntity>> extractFunction(
    TSNode node,
    const SourceFile& file
)
{
    if (ts_node_is_null(node) ||
        std::string_view(ts_node_type(node)) != "function_definition") {
        return Error{
            ErrorCode::invalid_input,
            "Expected a function definition node",
            file.relativePath
        };
    }

    if (!file.bytes || !nodeText(node, *file.bytes)) {
        return Error{
            ErrorCode::invalid_input,
            "Function range is outside source bytes",
            file.relativePath
        };
    }

    const auto declarator = ts_node_child_by_field_name(node, "declarator", 10);

    const auto nameNode = findDeclaratorName(declarator);
    const auto functionName = extractQualifiedName(nameNode, *file.bytes);

    if (!functionName) {
        return Error{
            ErrorCode::parse_incomplete,
            "Cannot extract function name",
            file.relativePath
        };
    }

    const auto body = ts_node_child_by_field_name(node, "body", 4);

    auto function = std::make_shared<FunctionEntity>();
    function->name = functionName->name;
    function->ownerName = functionName->ownerName;
    function->relativePath = file.relativePath;
    function->range = nodeRange(node);

    if (!ts_node_is_null(body)) {
        function->bodyRange = nodeRange(body);
        function->controlTree.range = *function->bodyRange;
    }

    if (ts_node_has_error(node)) {
        function->validity = Validity::unavailable;
        function->reason = "Function contains syntax errors";
    }
    else if (ts_node_is_null(body)) {
        function->validity = Validity::not_applicable;
        function->reason = "Function has no body";
    }

    return function;
}

} // namespace


} // namespace xray::code
