#include "code/api.hpp"
#include "code/types.hpp"
#include "common/job.hpp"
#include "common/result.hpp"

#include <algorithm>
#include <cstddef>
#include <ios>
#include <tree_sitter/api.h>

#include <memory>
#include <cstdint>
#include <limits>
#include <optional>
#include <string_view>
#include <string>
#include <variant>
#include <utility>
#include <vector>

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

std::optional<ControlKind> controlKindFor(std::string_view nodeType) {
    if (nodeType == "compound_statement") {
        return ControlKind::block;
    }
    if (nodeType == "if_statement") {
        return ControlKind::if_statement;
    }
    if (nodeType == "for_statement") {
        return ControlKind::for_loop;
    }
    if (nodeType == "for_range_loop") {
        return ControlKind::range_for;
    }
    if (nodeType == "while_statement") {
        return ControlKind::while_loop;
    }
    if (nodeType == "do_statement") {
        return ControlKind::do_loop;
    }
    if (nodeType == "switch_statement") {
        return ControlKind::switch_statement;
    }
    if (nodeType == "try_statement") {
        return ControlKind::try_statement;
    }
    if (nodeType == "catch_clause") {
        return ControlKind::catch_clause;
    }
    if (nodeType == "conditional_expression") {
        return ControlKind::conditional_expression;
    }
    if (nodeType == "lambda_expression") {
        return ControlKind::lambda;
    }
    if (nodeType == "class_specifier" ||
        nodeType == "struct_specifier" ||
        nodeType == "union_specifier") {
        return ControlKind::local_type;
    }

    return std::nullopt;
}

std::optional<ControlKind> controlKindFor(TSNode node) {
    if (ts_node_is_null(node)) {
        return std::nullopt;
    }

    const std::string_view nodeType = ts_node_type(node);

    if (nodeType == "if_statement") {
        const auto parent = ts_node_parent(node);

        if (!ts_node_is_null(parent) &&
            std::string_view(ts_node_type(parent)) == "else_clause") {
            return ControlKind::else_if;
        }
    }

    if (nodeType == "case_statement") {
        const auto count = ts_node_child_count(node);

        for (std::uint32_t index = 0; index < count; ++index) {
            const auto child = ts_node_child(node, index);
            const std::string_view childType = ts_node_type(child);

            if (childType == "case") {
                return ControlKind::case_label;
            }
            if (childType == "default") {
                return ControlKind::default_label;
            }
        }

        return std::nullopt;
    }

    return controlKindFor(nodeType);
}

bool appendControlNodes(
    TSNode node,
    std::vector<ControlNode>& destination,
    const JobContext& job
)
{
    if (job.isCancelled()) {
        return false;
    }

    if (ts_node_is_null(node)) {
        return true;
    }

    const auto kind = controlKindFor(node);

    if (!kind) {
        const auto count = ts_node_named_child_count(node);

        for (std::uint32_t index = 0; index < count; ++index) {
            const auto child = ts_node_named_child(node, index);

            if (!appendControlNodes(child, destination, job)) {
                return false;
            }
        }

        return !job.isCancelled();
    }

    ControlNode control;
    control.kind = *kind;
    control.range = nodeRange(node);

    std::vector<TSNode> siblings;

    if (*kind != ControlKind::lambda && *kind != ControlKind::local_type) {
        const auto count = ts_node_named_child_count(node);

        for (std::uint32_t index = 0; index < count; ++index) {
            const auto child = ts_node_named_child(node, index);
            const std::string_view childType = ts_node_type(child);

            if ((*kind == ControlKind::if_statement ||
                *kind == ControlKind::else_if) &&
                childType == "else_clause") {

                TSNode elseIf{};

                const auto alternativeCount = ts_node_named_child_count(child);

                for (std::uint32_t alternativeIndex = 0;
                    alternativeIndex < alternativeCount;
                    ++alternativeIndex) {

                    const auto alternative = ts_node_named_child(child, alternativeIndex);

                    if (std::string_view(ts_node_type(alternative)) == "if_statement") {
                        elseIf = alternative;
                        break;
                    }
                }

                if (!ts_node_is_null(elseIf)) {
                    siblings.push_back(elseIf);
                    continue;
                }
            }

            if (*kind == ControlKind::try_statement && childType == "catch_clause") {
                siblings.push_back(child);
                continue;
            }

            if (!appendControlNodes(child, control.children, job)) {
                return false;
            }
        }
    }

    if (job.isCancelled()) {
        return false;
    }

    destination.push_back(std::move(control));

    for (const auto sibling : siblings) {
        if (!appendControlNodes(sibling, destination, job)) {
            return false;
        }
    }

    return !job.isCancelled();
}

bool collectParseDiagnostics(
    TSNode root,
    const SourceFile& file,
    ParsedUnit& unit,
    const JobContext& job
)
{
    std::vector<TSNode> pending{root};

    while (!pending.empty()) {
        if(job.isCancelled()) {
            return false;
        }

        const auto node = pending.back();
        pending.pop_back();

        if (ts_node_is_null(node)) {
            continue;
        }

        const bool missing = ts_node_is_missing(node);
        const bool errorNode = ts_node_is_error(node);

        if (missing || errorNode) {
            std::string message;

            if (missing) {
                message = "Missing syntax token: ";
                message += ts_node_type(node);
            } else {
                message = "Unrecognized syntax";
            }

            Error error{
                ErrorCode::parse_incomplete,
                std::move(message),
                file.relativePath
            };

            unit.diagnostics.push_back(ParseDiagnostic{
                file.relativePath,
                nodeRange(node),
                error
            });

            unit.coverage.diagnostics.push_back(std::move(error));
            unit.coverage.completeness = Completeness::partial;
        }

        const auto count = ts_node_child_count(node);

        for (std::uint32_t index = count; index > 0; --index) {
            pending.push_back(ts_node_child(node, index-1));
        }
    }

    return !job.isCancelled();
}

void markAnalysisEligibility(
    FunctionEntity& function,
    const std::vector<ParseDiagnostic>& diagnostics
)
{
    for (const auto& diagnostic : diagnostics) {
        if (diagnostic.relativePath != function.relativePath || !diagnostic.range) {
            continue;
        }

        const auto& problem = *diagnostic.range;
        const auto& range = function.range;

        bool intersects;

        if (problem.startByte == problem.endByte) {
            intersects = range.startByte <= problem.startByte &&
                problem.startByte <= range.endByte;
        } else {
            intersects = problem.startByte < range.endByte &&
                range.startByte < problem.endByte;
        }

        if (intersects) {
            function.validity = Validity::unavailable;
            function.reason = diagnostic.error.message;
            return;
        }
    }

    if (function.validity == Validity::unavailable) {
        return;
    }

    if (!function.bodyRange) {
        function.validity = Validity::not_applicable;
        function.reason = "Function has no body";
    }
}

std::optional<std::vector<std::string>> normalizeDeclarator(
    TSNode declarator,
    std::string_view source
)
{
    if (ts_node_is_null(declarator) || ts_node_has_error(declarator)) {
        return std::nullopt;
    }

    std::vector<std::string> tokens;
    std::vector<TSNode> pending{declarator};

    while (!pending.empty()) {
        const auto node = pending.back();
        pending.pop_back();

        if (ts_node_is_missing(node)) {
            return std::nullopt;
        }

        if (std::string_view(ts_node_type(node)) == "comment") {
            continue;
        }

        const auto count = ts_node_child_count(node);

        if (count > 0) {
            for (std::uint32_t index = count; index > 0; --index) {
                pending.push_back(ts_node_child(node, index - 1));
            }

            continue;
        }

        const auto text = nodeText(node, source);

        if (!text || text->empty()) {
            return std::nullopt;
        }

        tokens.emplace_back(*text);
    }

    if (tokens.empty()) {
        return std::nullopt;
    }

    return tokens;
}

std::optional<std::string> normalizeName(
    TSNode node,
    std::string_view source
) {
    const auto tokens = normalizeDeclarator(node, source);

    if (!tokens) {
        return std::nullopt;
    }

    const auto isWordByte = [](unsigned char byte) {
        return (byte >= 'a' && byte <= 'z') ||
               (byte >= 'A' && byte <= 'Z') ||
               (byte >= '0' && byte <= '9') ||
               byte == '_' ||
               byte >= 0x80;
    };

    std::string name;

    for (const auto& token : *tokens) {
        if (!name.empty() &&
            isWordByte(name.back()) &&
            isWordByte(token.front())) {
            name += ' ';
        }

        name += token;
    }

    return name;
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

TSNode findDeclaredFunction(TSNode declarator) {
    auto name = findDeclaratorName(declarator);

    while (!ts_node_is_null(name) &&
           std::string_view(ts_node_type(name)) == "qualified_identifier") {
        name = ts_node_child_by_field_name(name, "name", 4);
    }

    if (ts_node_is_null(name)) {
        return {};
    }

    auto current = name;

    while (!ts_node_is_null(current)) {
        const std::string_view kind = ts_node_type(current);

        if (kind == "function_declarator") {
            return current;
        }

        if (kind == "pointer_declarator" ||
            kind == "pointer_type_declarator" ||
            kind == "reference_declarator" ||
            kind == "array_declarator") {
            return {};
        }

        if (ts_node_eq(current, declarator)) {
            break;
        }

        current = ts_node_parent(current);
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
            const auto scopeText = normalizeName(scope, source);

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
        kind != "template_method" &&
        kind != "template_type") {
        return std::nullopt;
    }

    const auto text = normalizeName(node, source);

    if (!text || text->empty()) {
        return std::nullopt;
    }

    result.name = std::string(*text);
    return result;
}

std::string joinScope(
    std::string_view outer,
    std::string_view inner
) {
    if (outer.empty()) {
        return std::string(inner);
    }

    if (inner.empty()) {
        return std::string(outer);
    }

    return std::string(outer) + "::" + std::string(inner);
}

std::string makeIdentityKey(const FunctionEntity& function) {
    std::string key;

    auto appendField = [&key](std::string_view value) {
        key += std::to_string(value.size());
        key += ':';
        key.append(value);
    };

    appendField(function.relativePath);
    appendField(joinScope(function.ownerName, function.name));

    for (const auto& token : function.signatureTokens) {
        appendField(token);
    }

    return key;
}

EntityId makeEntityId(std::uint64_t& nextId) {
    return "entity:" + std::to_string(nextId++);
}

bool hasConditionalAncestor(TSNode node) {
    auto parent = ts_node_parent(node);

    while (!ts_node_is_null(parent)) {
        const std::string_view kind = ts_node_type(parent);

        if (kind == "preproc_if" ||
            kind == "preproc_ifdef" ||
            kind == "preproc_elif" ||
            kind == "preproc_elifdef" ||
            kind == "preproc_else") {
            return true;
        }

        parent = ts_node_parent(parent);
    }

    return false;
}

Result<bool> hasConditionalDescendant(
    TSNode root,
    const JobContext& job
)
{
    std::vector<TSNode> pending{root};

    while (!pending.empty()) {
        if (job.isCancelled()) {
            return Cancelled{};
        }

        const auto node = pending.back();
        pending.pop_back();

        if (ts_node_is_null(node)) {
            continue;
        }

        const std::string_view kind = ts_node_type(node);

        if (kind == "preproc_if" ||
            kind == "preproc_ifdef" ||
            kind == "preproc_elif" ||
            kind == "preproc_elifdef" ||
            kind == "preproc_else") {
            return true;
        }

        const auto count = ts_node_named_child_count(node);

        for (std::uint32_t index = count; index > 0; --index) {
            pending.push_back(ts_node_named_child(node, index - 1));
        }
    }

    if (job.isCancelled()) {
        return Cancelled{};
    }

    return false;
}

Result<std::shared_ptr<FunctionEntity>> extractFunction(
    TSNode node,
    TSNode declarator,
    const SourceFile& file,
    std::string_view lexicalScope,
    const JobContext& job
)
{
    if (ts_node_is_null(node) || ts_node_is_null(declarator)) {
        return Error{
            ErrorCode::invalid_input,
            "Expected a function definition node and declarator",
            file.relativePath
        };
    }

    const std::string_view kind = ts_node_type(node);

    if (kind != "function_definition" &&
        kind != "declaration" &&
        kind != "field_declaration") {
        return Error{
            ErrorCode::invalid_input,
            "Expected a function definition or declaration",
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

    const auto nameNode = findDeclaratorName(declarator);
    const auto functionName = extractQualifiedName(nameNode, *file.bytes);

    if (!functionName) {
        return Error{
            ErrorCode::parse_incomplete,
            "Cannot extract function name",
            file.relativePath
        };
    }

    auto signature = normalizeDeclarator(declarator, *file.bytes);

    if (!signature) {
        return Error{
            ErrorCode::parse_incomplete,
            "Cannot extract reliable declarator tokens",
            file.relativePath
        };
    }

    const auto body = ts_node_child_by_field_name(node, "body", 4);

    auto function = std::make_shared<FunctionEntity>();
    function->name = functionName->name;
    function->ownerName = functionName->globallyQualified ? functionName->ownerName : joinScope(lexicalScope, functionName->ownerName);
    function->relativePath = file.relativePath;
    function->range = nodeRange(node);
    function->signatureTokens = std::move(*signature);
    function->identityKey = makeIdentityKey(*function);

    if (!ts_node_is_null(body)) {
        function->bodyRange = nodeRange(body);
        function->controlTree.kind = ControlKind::block;
        function->controlTree.range = *function->bodyRange;

        if (std::string_view(ts_node_type(body)) == "compound_statement") {
            const auto count = ts_node_named_child_count(body);

            for (std::uint32_t index = 0; index < count; ++index) {
                const auto child = ts_node_named_child(body, index);

                if (!appendControlNodes(
                        child, function->controlTree.children, job)) {
                    return Cancelled{};
                }
            }
        } else {
            if (!appendControlNodes(
                    body, function->controlTree.children, job)) {
                return Cancelled{};
            }
        }
    }

    if (ts_node_has_error(node)) {
        function->validity = Validity::unavailable;
        function->reason = "Function contains syntax errors";
    }
    else if (ts_node_is_null(body)) {
        function->validity = Validity::not_applicable;
        function->reason = "Function has no body";
    }
    else {
        auto conditionalResult = hasConditionalDescendant(body, job);

        if (std::holds_alternative<Cancelled>(conditionalResult)) {
            return Cancelled{};
        }

        if (hasConditionalAncestor(node) || std::get<bool>(conditionalResult)) {
            function->validity = Validity::syntactic_only;
            function->reason = "Conditional compilation is not evaluated";
        }
    }

    return function;
}

Result<std::shared_ptr<TypeEntity>> extractType(
    TSNode node,
    const SourceFile& file,
    std::string_view lexicalScope
)
{
    if (ts_node_is_null(node)) {
        return Error{
            ErrorCode::invalid_input,
            "Expected a type node",
            file.relativePath
        };
    }

    const std::string_view kind = ts_node_type(node);

    if (kind != "class_specifier" &&
        kind != "struct_specifier" &&
        kind != "union_specifier") {
        return Error{
            ErrorCode::invalid_input,
            "Expected a class, struct or union node",
            file.relativePath
        };
    }

    if (!file.bytes || !nodeText(node, *file.bytes)) {
        return Error{
            ErrorCode::invalid_input,
            "Type range is outside source bytes",
            file.relativePath
        };
    }

    const auto nameNode = ts_node_child_by_field_name(node, "name", 4);

    if (ts_node_is_null(nameNode)) {
        return Error{
            ErrorCode::parse_incomplete,
            "Anonymous type requires separate scope handling",
            file.relativePath
        };
    }

    const auto typeName = extractQualifiedName(nameNode, *file.bytes);

    if (!typeName) {
        return Error{
            ErrorCode::parse_incomplete,
            "Cannot extract type name",
            file.relativePath
        };
    }

    auto type = std::make_shared<TypeEntity>();
    type->name = typeName->name;
    type->range = nodeRange(node);

    const auto owner = typeName->globallyQualified ? typeName->ownerName : joinScope(lexicalScope, typeName->ownerName);

    type->qualifiedName = joinScope(owner, typeName->name);

    return type;
}

bool walkStructure(
    TSNode node,
    const SourceFile& file,
    CodeEntity& parent,
    std::string_view scope,
    ParsedUnit& unit,
    std::uint64_t& nextId,
    const JobContext& job
)
{
    if (job.isCancelled()) {
        return false;
    }

    if (ts_node_is_null(node)) {
        return true;
    }

    const std::string_view kind = ts_node_type(node);

    auto recordFailure = [&](const Error& error) {
        unit.diagnostics.push_back(ParseDiagnostic{file.relativePath, nodeRange(node), error});
        unit.coverage.diagnostics.push_back(error);
        unit.coverage.completeness = Completeness::partial;
        ++unit.coverage.skippedElements;
    };

    if (kind == "namespace_definition") {
        const auto nameNode = ts_node_child_by_field_name(node, "name", 4);
        const auto body = ts_node_child_by_field_name(node, "body", 4);

        std::string name = "(anonymous namespace)";

        if (!ts_node_is_null(nameNode)) {
            const auto text = normalizeName(nameNode, *file.bytes);

            if (!text || text->empty()) {
                recordFailure(Error{
                    ErrorCode::parse_incomplete,
                    "Cannot extract namespace name",
                    file.relativePath
                });
                return true;
            }

            name = std::string(*text);
        }

        const auto nestedScope = joinScope(scope, name);

        return walkStructure(body, file, parent, nestedScope, unit, nextId, job);
    }

    if (kind == "function_definition") {
        const auto declarator = ts_node_child_by_field_name(node, "declarator", 10);
        auto result = extractFunction(node, declarator, file, scope, job);

        if (const auto* error = std::get_if<Error>(&result)) {
            recordFailure(*error);
            return true;
        }

        if (std::holds_alternative<Cancelled>(result)) {
            return false;
        }

        auto function = std::get<std::shared_ptr<FunctionEntity>>(result);
        function->id = makeEntityId(nextId);

        markAnalysisEligibility(*function, unit.diagnostics);

        parent.children.push_back(function);
        unit.functions.push_back(function);
        return true;
    }

    if (kind == "declaration" || kind == "field_declaration") {
        const auto count = ts_node_child_count(node);

        for (std::uint32_t index = 0; index < count; ++index) {
            if (job.isCancelled()) {
                return false;
            }

            const auto* field = ts_node_field_name_for_child(node, index);

            if (!field || std::string_view(field) != "declarator") {
                continue;
            }

            const auto declarator = ts_node_child(node, index);

            if (ts_node_is_null(findDeclaredFunction(declarator))) {
                continue;
            }

            auto result = extractFunction(node, declarator, file, scope, job);

            if (const auto* error = std::get_if<Error>(&result)) {
                recordFailure(*error);
                continue;
            }

            if (std::holds_alternative<Cancelled>(result)) {
                return false;
            }

            auto function = std::get<std::shared_ptr<FunctionEntity>>(result);

            function->id = makeEntityId(nextId);

            markAnalysisEligibility(*function, unit.diagnostics);

            parent.children.push_back(function);
            unit.functions.push_back(function);
        }
    }

    if (kind == "class_specifier" ||
        kind == "struct_specifier" ||
        kind == "union_specifier") {
        auto result = extractType(node, file, scope);

        if (const auto* error = std::get_if<Error>(&result)) {
            recordFailure(*error);
            return true;
        }

        if (std::holds_alternative<Cancelled>(result)) {
            return false;
        }

        auto type = std::get<std::shared_ptr<TypeEntity>>(result);
        type->id = makeEntityId(nextId);
        const auto body = ts_node_child_by_field_name(
            node, "body", 4
        );

        if (!walkStructure(body, file, *type, type->qualifiedName, unit, nextId, job)) {
            return false;
        }

        parent.children.push_back(type);
        return true;
    }

    if (kind == "lambda_expression") {
        return true;
    }

    const auto count = ts_node_named_child_count(node);

    for (std::uint32_t index = 0; index < count; ++index) {
        const auto child = ts_node_named_child(node, index);

        if (!walkStructure(child, file, parent, scope, unit, nextId, job)) {
            return false;
        }
    }

    return !job.isCancelled();
}

Result<ParsedUnit> parseUnit(
    TSParser& parser,
    const SourceFile& file,
    std::uint64_t& nextId,
    const JobContext& job
)
{
    auto treeResult = parseTree(parser, file, job);

    if (const auto* error = std::get_if<Error>(&treeResult)) {
        return *error;
    }

    if (const auto* cancelled = std::get_if<Cancelled>(&treeResult)) {
        return *cancelled;
    }

    const auto& tree = std::get<TreePtr>(treeResult);
    const auto rootNode = ts_tree_root_node(tree.get());

    ParsedUnit unit;
    unit.relativePath = file.relativePath;

    auto root = std::make_shared<FileEntity>();
    root->id = makeEntityId(nextId);
    root->name = file.relativePath;
    root->relativePath = file.relativePath;
    root->contentId = file.contentId;
    root->sourceBytes = file.bytes;
    root->range = nodeRange(rootNode);
    root->range.startByte = 0;
    root->range.startLine = 1;
    root->range.endByte = file.bytes->size();

    if (ts_node_has_error(rootNode)) {
        if (!collectParseDiagnostics(rootNode, file, unit, job)) {
            return Cancelled{unit.coverage.diagnostics};
        }

        unit.coverage.completeness = Completeness::partial;
    }

    if (!walkStructure(rootNode, file, *root, "", unit, nextId, job)) {
        return Cancelled{unit.coverage.diagnostics};
    }

    if (unit.coverage.completeness == Completeness::partial) {
        root->parseState = Validity::unavailable;
    }

    unit.root = root;
    return unit;
}

} // namespace

Result<ParsedSnapshot> parse(
    std::shared_ptr<const SourceSnapshot> source,
    const ParseOptions& options,
    const JobContext& job
)
{
    if (job.isCancelled()) {
        return Cancelled{};
    }

    if (!source) {
        return Error{
            ErrorCode::invalid_input,
            "Source snapshot must not be null"
        };
    }

    if (options != ParseOptions{}) {
        return Error{
            ErrorCode::invalid_input,
            "Unsupported parser or structure version"
        };
    }

    auto parserResult = createParser();

    if (const auto* error = std::get_if<Error>(&parserResult)) {
        return *error;
    }

    if (const auto* cancelled = std::get_if<Cancelled>(&parserResult)) {
        return *cancelled;
    }

    const auto& parser = std::get<ParserPtr>(parserResult);

    ParsedSnapshot snapshot;
    snapshot.source = std::move(source);
    snapshot.options = options;
    snapshot.coverage = snapshot.source->coverage;

    const auto total = snapshot.source->files.size();
    snapshot.units.reserve(total);

    job.report("parse-files", 0, total);

    std::uint64_t nextId = 0;

    for (std::size_t index = 0; index < total; ++index) {
        if (job.isCancelled()) {
            return Cancelled{snapshot.coverage.diagnostics};
        }

        const auto& file = snapshot.source->files[index];
        auto unitResult = parseUnit(*parser, file, nextId, job);

        if (const auto* error = std::get_if<Error>(&unitResult)) {
            return *error;
        }

        if (const auto* cancelled = std::get_if<Cancelled>(&unitResult)) {
            auto diagnostics = snapshot.coverage.diagnostics;
            diagnostics.insert(
                diagnostics.end(),
                cancelled->diagnostics.begin(),
                cancelled->diagnostics.end()
            );

            return Cancelled{std::move(diagnostics)};
        }

        auto unit = std::move(std::get<ParsedUnit>(unitResult));

        if (unit.coverage.completeness == Completeness::partial) {
            snapshot.coverage.completeness = Completeness::partial;
        }

        snapshot.coverage.skippedElements += unit.coverage.skippedElements;

        snapshot.coverage.diagnostics.insert(
            snapshot.coverage.diagnostics.end(),
            unit.coverage.diagnostics.begin(),
            unit.coverage.diagnostics.end()
        );

        snapshot.units.push_back(std::move(unit));
        job.report("parse-files", index + 1, total);
    }

    if (job.isCancelled()) {
        return Cancelled{snapshot.coverage.diagnostics};
    }

    return snapshot;
}

} // namespace xray::code
