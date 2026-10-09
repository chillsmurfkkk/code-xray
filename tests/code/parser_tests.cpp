#include "code/api.hpp"

#include <cstddef>
#include <exception>
#include <initializer_list>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <stop_token>
#include <string>
#include <string_view>
#include <unordered_set>
#include <utility>
#include <variant>
#include <vector>

namespace {
using namespace xray;
using namespace xray::code;

void check(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

using InputFile = std::pair<std::string, std::string>;

std::shared_ptr<const SourceSnapshot> source(
    std::initializer_list<InputFile> files
) {
    auto snapshot = std::make_shared<SourceSnapshot>();
    snapshot->id = "parser-test-source";
    for (const auto& [path, bytes] : files) {
        snapshot->files.push_back(SourceFile{
            path, std::make_shared<const std::string>(bytes), "test:" + path
        });
    }
    return snapshot;
}

ParsedSnapshot parseSuccessfully(
    std::shared_ptr<const SourceSnapshot> input,
    const JobContext& job = {}
) {
    auto result = parse(std::move(input), ParseOptions{}, job);
    if (const auto* error = std::get_if<Error>(&result)) {
        throw std::runtime_error("Unexpected parse error: " + error->message);
    }
    check(!std::holds_alternative<Cancelled>(result), "Unexpected cancellation");
    return std::move(std::get<ParsedSnapshot>(result));
}

const FunctionEntity& functionNamed(
    const ParsedUnit& unit, std::string_view name
) {
    for (const auto& function : unit.functions) {
        if (function->name == name) {
            return *function;
        }
    }
    throw std::runtime_error("Function not found: " + std::string(name));
}

bool reachable(const CodeEntity& root, const CodeEntity* target) {
    if (&root == target) {
        return true;
    }
    for (const auto& child : root.children) {
        if (reachable(*child, target)) {
            return true;
        }
    }
    return false;
}

std::string_view textAt(const std::string& bytes, const SourceRange& range) {
    check(range.startByte <= range.endByte, "Reversed byte range");
    check(range.endByte <= bytes.size(), "Range exceeds original bytes");
    return std::string_view(bytes).substr(
        static_cast<std::size_t>(range.startByte),
        static_cast<std::size_t>(range.endByte - range.startByte)
    );
}

void emptyInputs() {
    const auto empty = parseSuccessfully(source({}));
    check(empty.units.empty(), "Empty snapshot must produce no units");
    check(empty.coverage.completeness == Completeness::complete,
          "Empty snapshot must be complete");

    const auto file = parseSuccessfully(source({{"empty.cpp", ""}}));
    check(file.units.size() == 1, "Empty file must still have a unit");
    const auto& unit = file.units.front();
    check(unit.root != nullptr, "Empty file must have a root");
    check(unit.functions.empty() && unit.root->children.empty(),
          "Empty file must have no entities");
    check(unit.root->range.startByte == 0 && unit.root->range.endByte == 0,
          "Empty file must have an empty range");
    check(unit.diagnostics.empty(), "Empty file must not have diagnostics");
}

void invalidInputs() {
    auto result = parse(nullptr, ParseOptions{}, JobContext{});
    const auto* error = std::get_if<Error>(&result);
    check(error && error->code == ErrorCode::invalid_input,
          "Null snapshot must be invalid input");

    auto invalid = std::make_shared<SourceSnapshot>();
    invalid->files.push_back(SourceFile{"missing.cpp", nullptr, "test"});
    result = parse(invalid, ParseOptions{}, JobContext{});
    error = std::get_if<Error>(&result);
    check(error && error->code == ErrorCode::invalid_input &&
              error->path == "missing.cpp",
          "Missing byte buffer must return an error with the file path");

    for (const bool changeGrammar : {true, false}) {
        ParseOptions options;
        if (changeGrammar) {
            options.grammarVersion = "unsupported-grammar";
        } else {
            options.structureVersion = "unsupported-structure";
        }
        result = parse(source({}), options, JobContext{});
        error = std::get_if<Error>(&result);
        check(error && error->code == ErrorCode::invalid_input,
              "Unsupported versions must not be recorded as successful parsing");
    }
}

void originalBytesAndRanges() {
    for (const std::string newline : {std::string("\n"), std::string("\r\n")}) {
        const std::string definition =
            "int sum(" + newline +
            "    int a, int b) {" + newline +
            "    return a + b;" + newline + "}";
        const std::string bytes =
            std::string("\xEF\xBB\xBF") + "// Привіт" + newline +
            definition + newline;
        auto input = source({{"src/sum.cpp", bytes}});
        const auto parsed = parseSuccessfully(input);
        check(parsed.units.size() == 1, "Expected one parsed file");
        const auto& unit = parsed.units.front();
        check(unit.functions.size() == 1, "Expected one function");
        const auto& function = *unit.functions.front();
        check(function.name == "sum" && function.relativePath == "src/sum.cpp",
              "Function name and path must come from the input");
        check(textAt(bytes, function.range) == definition,
              "Function range must select exactly the definition");
        check(function.range.startLine == 2 && function.range.endLine == 5,
              "Function line numbers must be one-based and inclusive");
        check(function.range.startByte == bytes.find("int sum("),
              "Byte offsets must include BOM and UTF-8 comment bytes");
        check(function.bodyRange.has_value(), "Definition must have a body range");
        check(textAt(bytes, *function.bodyRange) ==
                  "{" + newline + "    return a + b;" + newline + "}",
              "Body range must select braces and their contents");
        check(function.bodyRange->startLine == 3 && function.bodyRange->endLine == 5,
              "Body line range must match the original source");
        check(textAt(bytes, unit.root->range) == bytes,
              "File range must include all original bytes");
        check(unit.root->range.endLine == 5,
              "Trailing newline must not add a token line");
        check(unit.root->sourceBytes == input->files.front().bytes &&
                  *unit.root->sourceBytes == bytes,
              "BOM and line endings must be retained without normalization");
        check(unit.root->contentId == input->files.front().contentId,
              "File content identity must be retained");
        check(unit.coverage.completeness == Completeness::complete &&
                  function.validity == Validity::valid,
              "Valid source must have complete parsing coverage");
    }
}

void nestedStructureAndSharedIndex() {
    const auto parsed = parseSuccessfully(source({{"nested.cpp", R"cpp(
namespace outer {
namespace inner {
struct Widget {
    int run() { return 1; }
    class Inner {
        int nested() { return 2; }
    };
};
int freeFunction() { return 3; }
}
}
)cpp"}}));
    const auto& unit = parsed.units.front();
    check(unit.functions.size() == 3, "Expected two methods and one free function");
    check(unit.root->children.size() == 2, "Namespaces must not add entity nodes");
    const auto widget = std::dynamic_pointer_cast<const TypeEntity>(
        unit.root->children.front());
    check(widget && widget->qualifiedName == "outer::inner::Widget",
          "Type must include enclosing namespaces");
    check(widget->children.size() == 2, "Widget must contain a method and a nested type");
    const auto inner = std::dynamic_pointer_cast<const TypeEntity>(widget->children[1]);
    check(inner && inner->qualifiedName == "outer::inner::Widget::Inner",
          "Nested type must include its enclosing class");
    check(functionNamed(unit, "run").ownerName == "outer::inner::Widget",
          "Method must inherit its class scope");
    check(functionNamed(unit, "nested").ownerName == "outer::inner::Widget::Inner",
          "Nested method must inherit the complete lexical scope");
    check(functionNamed(unit, "freeFunction").ownerName == "outer::inner",
          "Free function must inherit its namespace scope");
    for (const auto& function : unit.functions) {
        check(reachable(*unit.root, function.get()),
              "Function index must point to the SAME objects in the entity tree");
        const CodeEntity& entity = *function;
        check(!entity.describe().empty(), "Entity must remain usable through the base interface");
    }
}

void explicitlyQualifiedMethods() {
    const auto parsed = parseSuccessfully(source({{"methods.cpp", R"cpp(
namespace app {
struct Widget { int run(); };
int Widget::run() { return 1; }
}
int app::Widget::run() { return 2; }
)cpp"}}));
    const auto& unit = parsed.units.front();
    check(unit.functions.size() == 3, "Expected the method declaration and both out-of-class definitions");
    check(unit.functions[0]->validity == Validity::not_applicable && !unit.functions[0]->bodyRange &&
          unit.functions[1]->validity == Validity::valid && unit.functions[1]->bodyRange &&
          unit.functions[2]->validity == Validity::valid && unit.functions[2]->bodyRange,
          "The in-class declaration must remain distinct from both usable definitions");
    for (const auto& function : unit.functions) {
        check(function->name == "run", "Qualification must be separated from the name");
        check(function->ownerName == "app::Widget",
              "Explicit Widget/app::Widget scope must not be lost");
        check(function->describe() == "app::Widget::run",
              "Description must include the full function scope");
    }
}

void declaratorShapes() {
    const auto parsed = parseSuccessfully(source({{"declarators.cpp", R"cpp(
int* pointer() { return nullptr; }
int& reference(int& value) { return value; }
int (parenthesized)() { return 1; }
struct Widget {
    Widget() {}
    ~Widget() {}
    int operator()() { return 2; }
};
)cpp"}}));
    const auto& unit = parsed.units.front();
    check(unit.functions.size() == 6, "Expected all six declarator shapes");
    for (const auto name : {"pointer", "reference", "parenthesized", "Widget",
                            "~Widget", "operator()"}) {
        const auto& function = functionNamed(unit, name);
        check(function.bodyRange.has_value() && function.validity == Validity::valid,
              "Declarator shape must not hide a valid function body");
    }
}

void templateFunction() {
    const auto parsed = parseSuccessfully(source({{"template.cpp",
        "template <typename T>\nT identity(T value) { return value; }\n"}}));
    const auto& unit = parsed.units.front();
    check(unit.functions.size() == 1, "Template function must be found exactly once");
    const auto& function = functionNamed(unit, "identity");
    check(function.bodyRange.has_value() && function.validity == Validity::valid,
          "Template function must have a valid body");
    check(textAt(*unit.root->sourceBytes, *function.bodyRange) == "{ return value; }",
          "Template body must refer to the original source");
}

void syntaxErrorsPreserveValidFunctions() {
    const auto parsed = parseSuccessfully(source({{"broken.cpp",
        "int broken() { return 1 + ; }\nint good() { return 2; }\n"}}));
    const auto& unit = parsed.units.front();
    check(parsed.coverage.completeness == Completeness::partial &&
              unit.coverage.completeness == Completeness::partial,
          "Syntax errors must make both coverages partial");
    check(unit.root->parseState == Validity::unavailable,
          "File must not be advertised as completely valid");
    check(!unit.diagnostics.empty() && !parsed.coverage.diagnostics.empty(),
          "Syntax errors must be retained in file and snapshot diagnostics");
    for (const auto& diagnostic : unit.diagnostics) {
        check(diagnostic.relativePath == "broken.cpp" &&
                  diagnostic.error.code == ErrorCode::parse_incomplete,
              "Parse diagnostics must identify the affected file");
    }
    const auto& broken = functionNamed(unit, "broken");
    check(broken.validity == Validity::unavailable && !broken.reason.empty(),
          "Function intersecting syntax errors must be unavailable with a reason");
    check(functionNamed(unit, "good").validity == Validity::valid,
          "An unrelated valid function must remain available");
}

void inheritedCoverageAndMultipleFiles() {
    auto input = std::make_shared<SourceSnapshot>(*source({
        {"first.cpp", "int first() { return 1; }"},
        {"second.cpp", "int second() { return 2; }"}
    }));
    input->coverage = {Completeness::partial, 2,
        {Error{ErrorCode::read_error, "Earlier read failed", "skipped.cpp"}}};
    const auto parsed = parseSuccessfully(input);
    check(parsed.units.size() == 2 && parsed.units[0].relativePath == "first.cpp" &&
              parsed.units[1].relativePath == "second.cpp",
          "Every file must be parsed in input order");
    check(functionNamed(parsed.units[0], "first").name == "first" &&
              functionNamed(parsed.units[1], "second").name == "second",
          "Reusing the parser must not mix files");
    check(parsed.coverage.completeness == Completeness::partial &&
              parsed.coverage.skippedElements == 2 &&
              parsed.coverage.diagnostics.size() == 1 &&
              parsed.coverage.diagnostics.front().path == "skipped.cpp",
          "Source coverage must be inherited exactly once");
    check(parsed.units[0].coverage.completeness == Completeness::complete,
          "A source-level read failure must not mark an unrelated unit invalid");
}

void sourceLifetime() {
    auto input = source({{"not-on-disk.cpp", "int kept() { return 7; }"}});
    std::weak_ptr<const SourceSnapshot> weak = input;
    auto parsed = parseSuccessfully(input);
    check(parsed.source == input, "Parsed snapshot must retain the original source object");
    input.reset();
    check(!weak.expired(), "Parsed snapshot must keep its source alive");
    const auto& unit = parsed.units.front();
    check(textAt(*unit.root->sourceBytes, unit.functions.front()->range) ==
              "int kept() { return 7; }",
          "Entities must remain usable after the Tree-sitter tree was freed");
    parsed = ParsedSnapshot{};
    check(weak.expired(), "Releasing the parsed snapshot must release its source ownership");
}

void progress() {
    std::vector<Progress> events;
    JobContext job{73, {}, [&](const Progress& event) { events.push_back(event); }};
    const auto parsed = parseSuccessfully(source({
        {"a.cpp", "int a() { return 1; }"},
        {"b.cpp", "int b() { return 2; }"}
    }), job);
    check(parsed.units.size() == 2 && events.size() == 3,
          "Progress must report the start and both completed files");
    for (std::size_t index = 0; index < events.size(); ++index) {
        check(events[index].jobId == 73 && events[index].stage == "parse-files" &&
                  events[index].completed == index && events[index].total == 2,
              "Progress must carry job ID, stage, count and total");
    }
}

void cancellationBeforeStart() {
    std::stop_source stop;
    stop.request_stop();
    std::size_t reports = 0;
    JobContext job{1, stop.get_token(), [&](const Progress&) { ++reports; }};
    const auto result = parse(source({{"a.cpp", "int a() {}"}}), ParseOptions{}, job);
    check(std::holds_alternative<Cancelled>(result) && reports == 0,
          "Pre-cancelled job must not start parsing or report progress");
}

void cancellationFromProgress() {
    // Cancel at initial progress, between files, and at the final progress callback.
    for (const std::size_t completed : {std::size_t{0}, std::size_t{1}, std::size_t{2}}) {
        std::stop_source stop;
        std::vector<std::size_t> observed;
        JobContext job{2, stop.get_token(), [&](const Progress& event) {
            observed.push_back(event.completed);
            if (event.completed == completed) {
                stop.request_stop();
            }
        }};
        const auto result = parse(source({
            {"a.cpp", "int a() {}"}, {"b.cpp", "int b() {}"}
        }), ParseOptions{}, job);
        check(std::holds_alternative<Cancelled>(result),
              "Progress cancellation must return Cancelled, never a snapshot");
        check(!observed.empty() && observed.back() == completed,
              "No further files may complete after a progress cancellation");
    }
}

void cancellationKeepsDiagnostics() {
    auto input = std::make_shared<SourceSnapshot>(*source({
        {"broken.cpp", "int broken() { return 1 + ; }"},
        {"later.cpp", "int later() {}"}
    }));
    input->coverage = {Completeness::partial, 1,
        {Error{ErrorCode::read_error, "Earlier read failed", "skipped.cpp"}}};
    std::stop_source stop;
    JobContext job{3, stop.get_token(), [&](const Progress& event) {
        if (event.completed == 1) {
            stop.request_stop();
        }
    }};
    const auto result = parse(input, ParseOptions{}, job);
    const auto* cancelled = std::get_if<Cancelled>(&result);
    check(cancelled != nullptr, "Expected cancellation between files");
    bool retainedReadError = false;
    bool retainedParseError = false;
    for (const auto& diagnostic : cancelled->diagnostics) {
        retainedReadError |= diagnostic.path == "skipped.cpp";
        retainedParseError |= diagnostic.path == "broken.cpp";
    }
    check(retainedReadError && retainedParseError,
          "Cancellation must retain both inherited and collected diagnostics");
}

void declaratorTokensIgnoreFormatting() {
    const auto compact = parseSuccessfully(source({{"tokens.cpp",
        "int* find(const int& value, int count = 2) noexcept { return nullptr; }"}}));
    const auto formatted = parseSuccessfully(source({{"tokens.cpp",
        "int * /* result */ find ( const int & value, // argument\n"
        "int count /* default */ = 2 ) noexcept { return nullptr; }"}}));
    const std::vector<std::string> expected{
        "*", "find", "(", "const", "int", "&", "value", ",",
        "int", "count", "=", "2", ")", "noexcept"
    };
    check(functionNamed(compact.units.front(), "find").signatureTokens == expected,
          "Declarator tokens must retain punctuation, parameters and qualifiers in order");
    check(functionNamed(formatted.units.front(), "find").signatureTokens == expected,
          "Whitespace and comments between tokens must not alter the declarator");
}

void literalContentsRemainSignificant() {
    const auto first = parseSuccessfully(source({{"literal.cpp",
        "void log(const char* text = \"a b\") {}"}}));
    const auto second = parseSuccessfully(source({{"literal.cpp",
        "void log(const char* text = \"ab\") {}"}}));
    const auto& withSpace = functionNamed(first.units.front(), "log");
    const auto& withoutSpace = functionNamed(second.units.front(), "log");
    check(withSpace.signatureTokens != withoutSpace.signatureTokens,
          "Spaces inside a string literal are source content, not formatting");
    check(withSpace.identityKey != withoutSpace.identityKey,
          "Changing a default argument literal must change the identity key");
}

void identityIgnoresBodyAndLineShifts() {
    const auto before = parseSuccessfully(source({{"stable.cpp",
        "namespace app { int run(int value) { return value; } }"}}));
    const auto after = parseSuccessfully(source({{"stable.cpp",
        "// Added header\n\nnamespace app {\n"
        "int run /* renamed body only */ ( int value ) {\n"
        "    const int doubled = value * 2;\n    return doubled;\n}\n}"}}));
    const auto& oldFunction = functionNamed(before.units.front(), "run");
    const auto& newFunction = functionNamed(after.units.front(), "run");
    check(!oldFunction.identityKey.empty(), "Every extracted function needs an identity key");
    check(oldFunction.range.startLine != newFunction.range.startLine,
          "Test must move the function to another source line");
    check(oldFunction.identityKey == newFunction.identityKey,
          "Body edits, shifted lines and declarator formatting must preserve identity");
}

void identityUsesLengthPrefixedUtf8Fields() {
    const auto parsed = parseSuccessfully(source({{"src/код:main.cpp",
        "namespace app { int f() { return 1; } }"}}));
    const auto& function = functionNamed(parsed.units.front(), "f");
    // The path has 19 UTF-8 bytes; app::f has 6. Colons are part of field values.
    check(function.identityKey == "19:src/код:main.cpp6:app::f1:f1:(1:)",
          "Key must length-prefix path, qualified name and each declarator token in UTF-8 bytes");
}

void identitiesDistinguishPathsScopesAndOverloads() {
    const auto parsed = parseSuccessfully(source({
        {"a.cpp", R"cpp(
namespace left {
int run(int value) { return value; }
int run(double value) { return 0; }
}
namespace right { int run(int value) { return value; } }
struct Box {
    int get() { return 0; }
    int get() const { return 0; }
};
)cpp"},
        {"b.cpp", "namespace left { int run(int value) { return value; } }"}
    }));
    std::unordered_set<std::string> keys;
    for (const auto& unit : parsed.units) {
        // Check each source definition once, so duplicate index entries have their own test.
        std::unordered_set<const FunctionEntity*> seen;
        for (const auto& function : unit.functions) {
            if (!seen.insert(function.get()).second) {
                continue;
            }
            check(!function->identityKey.empty() && keys.insert(function->identityKey).second,
                  "Different paths, owners, parameter types or const qualifiers need distinct keys");
        }
    }
    check(keys.size() == 6, "Expected six distinct function identities");
}

void formattedNamespacePreservesIdentity() {
    const auto compact = parseSuccessfully(source({{"namespace.cpp",
        "namespace app::detail { int run() { return 1; } }"}}));
    const auto formatted = parseSuccessfully(source({{"namespace.cpp",
        "namespace app /* scope */ :: detail { int run() { return 1; } }"}}));
    const auto& first = functionNamed(compact.units.front(), "run");
    const auto& second = functionNamed(formatted.units.front(), "run");
    check(first.ownerName == "app::detail" && second.ownerName == first.ownerName,
          "Namespace names must exclude whitespace and comments around ::");
    check(first.identityKey == second.identityKey,
          "Formatting a nested namespace must not change a function identity");
}

void formattedTemplateOwnerPreservesIdentity() {
    const auto compact = parseSuccessfully(source({{"owner.cpp",
        "template<> int Box<int>::run() { return 1; }"}}));
    const auto formatted = parseSuccessfully(source({{"owner.cpp",
        "template <> int Box< int >::run() { return 1; }"}}));
    const auto& first = functionNamed(compact.units.front(), "run");
    const auto& second = functionNamed(formatted.units.front(), "run");
    check(first.ownerName == "Box<int>" && second.ownerName == first.ownerName,
          "Template owner names must be independent of spacing inside < >");
    check(first.identityKey == second.identityKey,
          "Formatting a template owner must not change a function identity");
}

void collectEntityIds(const CodeEntity& entity, std::vector<EntityId>& ids) {
    ids.push_back(entity.id);
    for (const auto& child : entity.children) {
        check(child != nullptr, "Entity tree must not contain null children");
        collectEntityIds(*child, ids);
    }
}

void entityIdsAreUniqueAndRepeatable() {
    const auto input = source({
        {"a.cpp", "struct Outer { struct Inner { int run() { return 1; } }; };"},
        {"empty.cpp", ""},
        {"b.cpp", "struct Other { int run() { return 2; } }; int free() { return 0; }"}
    });
    const auto first = parseSuccessfully(input);
    const auto second = parseSuccessfully(input);
    std::vector<EntityId> firstIds;
    std::vector<EntityId> secondIds;
    for (const auto& unit : first.units) {
        collectEntityIds(*unit.root, firstIds);
        std::unordered_set<const FunctionEntity*> indexed;
        for (const auto& function : unit.functions) {
            check(indexed.insert(function.get()).second,
                  "A function object must appear in the function index exactly once");
            check(reachable(*unit.root, function.get()),
                  "Indexed functions must share the entity IDs of the tree objects");
        }
    }
    for (const auto& unit : second.units) {
        collectEntityIds(*unit.root, secondIds);
    }
    std::unordered_set<EntityId> unique;
    for (const auto& id : firstIds) {
        check(!id.empty(), "Files, types and functions all need nonempty IDs");
        check(unique.insert(id).second, "Entity IDs must be unique across the entire snapshot");
    }
    check(firstIds.size() == 9, "Expected three files, three types and three functions");
    check(firstIds == secondIds,
          "Parsing the same input twice must assign the same IDs in tree traversal order");
}

void duplicateKeysRetainSeparateEntities() {
    // Tree-sitter parses syntax; duplicate semantic definitions are deliberately retained.
    const auto parsed = parseSuccessfully(source({{"duplicates.cpp",
        "int run() { return 1; }\nint run() { return 2; }"}}));
    const auto& unit = parsed.units.front();
    check(unit.functions.size() == 2 && unit.root->children.size() == 2,
          "Each of two definitions must appear exactly once in the index and tree");
    const auto& first = *unit.functions[0];
    const auto& second = *unit.functions[1];
    check(&first != &second && !first.id.empty() && !second.id.empty() && first.id != second.id,
          "Duplicate keys must belong to separate entities with distinct IDs");
    check(!first.identityKey.empty() && first.identityKey == second.identityKey,
          "Duplicate identity keys must remain visible to the matching module");
    check(first.range.startByte != second.range.startByte,
          "Duplicate definitions must retain their own source ranges");
}

std::size_t countControlKind(const ControlNode& node, ControlKind kind) {
    std::size_t count = node.kind == kind ? 1 : 0;
    for (const auto& child : node.children) {
        count += countControlKind(child, kind);
    }
    return count;
}

void controlTreePreservesBranchLevels() {
    const auto parsed = parseSuccessfully(source({{"controls.cpp", R"cpp(
void run(int value) {
    if (value > 0) {
        while (value > 1) { --value; }
    } else /* comment */ if (value < 0) {
        ++value;
    } else if (value == 0) {
        return;
    } else {
        if (value == 1) { return; }
    }
    try {
        run(value);
    } catch (int error) {
        if (error) { return; }
    } catch (...) {
        return;
    }
}
)cpp"}}));
    const auto& function = functionNamed(parsed.units.front(), "run");
    check(function.validity == Validity::valid, "Control fixture must parse without errors");
    const auto& tree = function.controlTree;
    check(tree.kind == ControlKind::block && tree.children.size() == 6,
          "If, two else-if branches, try and two catches must share the root block");
    const ControlKind expected[] = {
        ControlKind::if_statement, ControlKind::else_if, ControlKind::else_if,
        ControlKind::try_statement, ControlKind::catch_clause, ControlKind::catch_clause
    };
    for (std::size_t index = 0; index < tree.children.size(); ++index) {
        check(tree.children[index].kind == expected[index],
              "Sibling branches must preserve source order and kind");
    }
    const auto& first = tree.children[0];
    check(first.children.size() == 1 && first.children[0].kind == ControlKind::block,
          "The first if must retain its body block");
    check(first.children[0].children.size() == 1 &&
          first.children[0].children[0].kind == ControlKind::while_loop,
          "The while must remain nested in the first branch");
    const auto& lastBranch = tree.children[2];
    check(lastBranch.children.size() == 2 &&
          lastBranch.children[1].kind == ControlKind::block &&
          lastBranch.children[1].children.size() == 1 &&
          lastBranch.children[1].children[0].kind == ControlKind::if_statement,
          "An if inside a braced else must remain nested, not become an else-if");
    check(countControlKind(tree, ControlKind::else_if) == 2 &&
          countControlKind(tree, ControlKind::if_statement) == 3 &&
          countControlKind(tree, ControlKind::catch_clause) == 2,
          "Deferred branches must appear exactly once");
}

void controlTreeRecognizesLoopsAndLabels() {
    const auto parsed = parseSuccessfully(source({{"loops.cpp", R"cpp(
void run(int value, int* values) {
    for (int i = 0; i < value; ++i) {}
    for (int item : values) {}
    while (value) { --value; }
    do { ++value; } while (value < 2);
    switch (value) {
        case 0: break;
        case 1: if (value) { break; } break;
        default: break;
    }
}
)cpp"}}));
    // The parser is syntactic: the range-for operand need not be semantically iterable.
    const auto& function = functionNamed(parsed.units.front(), "run");
    check(function.validity == Validity::valid, "Loop fixture must parse without errors");
    const auto& tree = function.controlTree;
    check(tree.children.size() == 5, "Each loop and switch must appear once at body level");
    const ControlKind expected[] = {
        ControlKind::for_loop, ControlKind::range_for, ControlKind::while_loop,
        ControlKind::do_loop, ControlKind::switch_statement
    };
    for (std::size_t index = 0; index < tree.children.size(); ++index) {
        check(tree.children[index].kind == expected[index], "Loop kinds must remain distinct");
    }
    const auto& switchNode = tree.children[4];
    check(switchNode.children.size() == 1 &&
          switchNode.children[0].kind == ControlKind::block,
          "Switch must retain its body block");
    const auto& labels = switchNode.children[0].children;
    check(labels.size() == 3 && labels[0].kind == ControlKind::case_label &&
          labels[1].kind == ControlKind::case_label &&
          labels[2].kind == ControlKind::default_label,
          "Case and default labels must be distinguished in source order");
    check(countControlKind(labels[1], ControlKind::if_statement) == 1,
          "A case must retain control statements inside it");
}

void controlTreeFindsExpressionsThroughWrappers() {
    const auto parsed = parseSuccessfully(source({{"expressions.cpp", R"cpp(
int choose(int value) {
    int result = value ? 1 : 2;
    consume(value ? 3 : 4);
    if (value ? true : false) { result += 1; }
    return value ? result : (result ? 5 : 6);
}
void empty() {}
)cpp"}}));
    const auto& unit = parsed.units.front();
    const auto& tree = functionNamed(unit, "choose").controlTree;
    check(tree.children.size() == 4 &&
          tree.children[0].kind == ControlKind::conditional_expression &&
          tree.children[1].kind == ControlKind::conditional_expression &&
          tree.children[2].kind == ControlKind::if_statement &&
          tree.children[3].kind == ControlKind::conditional_expression,
          "Declarations, calls and returns must not hide conditional expressions");
    check(countControlKind(tree, ControlKind::conditional_expression) == 5,
          "Condition expressions and nested ternaries must all remain reachable");
    check(countControlKind(tree.children[3], ControlKind::conditional_expression) == 2,
          "A nested ternary must remain inside its outer expression");
    const auto& empty = functionNamed(unit, "empty");
    check(empty.controlTree.kind == ControlKind::block && empty.controlTree.children.empty(),
          "An empty body must have one empty root, without a duplicate block");
}

void controlTreeExcludesInnerBodies() {
    const auto parsed = parseSuccessfully(source({{"inner.cpp", R"cpp(
void outer(int value) {
    auto callback = [] { if (true) { while (true) {} } };
    class Local { void method() { for (;;) { if (true) {} } } };
    struct Other { void method() { do {} while (true); } };
    union Storage { int number; double fraction; };
    if (value) { return; }
}
)cpp"}}));
    const auto& tree = functionNamed(parsed.units.front(), "outer").controlTree;
    check(tree.children.size() == 5 && tree.children[0].kind == ControlKind::lambda &&
          tree.children[1].kind == ControlKind::local_type &&
          tree.children[2].kind == ControlKind::local_type &&
          tree.children[3].kind == ControlKind::local_type &&
          tree.children[4].kind == ControlKind::if_statement,
          "Inner bodies must leave explicit boundaries in source order");
    for (std::size_t index = 0; index < 4; ++index) {
        check(tree.children[index].children.empty(), "Inner-body boundaries must be leaves");
    }
    check(countControlKind(tree, ControlKind::if_statement) == 1 &&
          countControlKind(tree, ControlKind::while_loop) == 0 &&
          countControlKind(tree, ControlKind::for_loop) == 0 &&
          countControlKind(tree, ControlKind::do_loop) == 0,
          "Inner-function control statements must not leak into the outer function");
}

void controlTreeKeepsOriginalRanges() {
    const std::string bytes = "\xEF\xBB\xBF// \xD0\xBA\xD0\xBE\xD0\xB4\r\n"
        "int run(int value) {\r\n"
        "    if (value) { return value ? 1 : 2; }\r\n"
        "    return 0;\r\n"
        "}\r\n";
    const auto parsed = parseSuccessfully(source({{"ranges.cpp", bytes}}));
    const auto& function = functionNamed(parsed.units.front(), "run");
    const auto& tree = function.controlTree;
    check(function.bodyRange && tree.range.startByte == function.bodyRange->startByte &&
          tree.range.endByte == function.bodyRange->endByte &&
          tree.range.startLine == 2 && tree.range.endLine == 5,
          "Root range must match the original function body, including CRLF coordinates");
    check(tree.children.size() == 1, "Returns must not introduce control nodes");
    const auto& branch = tree.children[0];
    check(textAt(bytes, branch.range) == "if (value) { return value ? 1 : 2; }" &&
          branch.range.startByte == bytes.find("if (value)") &&
          branch.range.startLine == 3 && branch.range.endLine == 3,
          "If offsets must refer to original bytes after BOM and multibyte text");
    check(branch.children.size() == 1 && branch.children[0].children.size() == 1,
          "The body block must retain its return expression");
    const auto& expression = branch.children[0].children[0];
    check(expression.kind == ControlKind::conditional_expression &&
          textAt(bytes, expression.range) == "value ? 1 : 2",
          "Expression ranges must remain usable after the Tree-sitter tree is destroyed");
}

void syntaxDiagnosticsHaveConcreteRanges() {
    const std::string bytes = "\xEF\xBB\xBF// \xD0\xBA\xD0\xBE\xD0\xB4\r\n"
        "void first() { @@@; }\r\n"
        "void second() { @@@; }\r\n"
        "int good() { return 2; }\r\n";
    const auto parsed = parseSuccessfully(source({{"errors.cpp", bytes}}));
    const auto& unit = parsed.units.front();
    check(unit.diagnostics.size() >= 2,
          "Both invalid fragments must have concrete syntax diagnostics");
    check(unit.coverage.diagnostics.size() == unit.diagnostics.size() &&
          parsed.coverage.diagnostics.size() == unit.diagnostics.size(),
          "Concrete syntax errors must propagate to unit and snapshot coverage once");
    check(unit.coverage.completeness == Completeness::partial &&
          parsed.coverage.completeness == Completeness::partial &&
          unit.root->parseState == Validity::unavailable,
          "Concrete diagnostics must retain partial-result status");
    check(unit.coverage.skippedElements == 0,
          "Syntax diagnostics must not count retained functions as skipped elements");
    const auto firstOffset = bytes.find("@@@");
    const auto secondOffset = bytes.find("@@@", firstOffset + 3);
    bool firstReported = false;
    bool secondReported = false;
    std::uint64_t previousStart = 0;
    for (std::size_t index = 0; index < unit.diagnostics.size(); ++index) {
        const auto& diagnostic = unit.diagnostics[index];
        check(diagnostic.range.has_value(), "Every syntax error must have a concrete range");
        check(diagnostic.relativePath == "errors.cpp" &&
              diagnostic.error.path == "errors.cpp" &&
              diagnostic.error.code == ErrorCode::parse_incomplete,
              "Syntax diagnostics must preserve path and error code");
        const auto& range = *diagnostic.range;
        const bool inFirst = range.startByte >= firstOffset && range.endByte <= firstOffset + 3;
        const bool inSecond = range.startByte >= secondOffset && range.endByte <= secondOffset + 3;
        check((inFirst || inSecond) && range.startByte >= previousStart &&
              textAt(bytes, range).find('@') != std::string_view::npos &&
              range.startLine == (inFirst ? 2u : 3u) && range.endLine == range.startLine,
              "Errors must be ordered and refer to original UTF-8/BOM/CRLF coordinates");
        firstReported = firstReported || inFirst;
        secondReported = secondReported || inSecond;
        previousStart = range.startByte;
        check(unit.coverage.diagnostics[index].message == diagnostic.error.message &&
              parsed.coverage.diagnostics[index].message == diagnostic.error.message,
              "Detailed and coverage diagnostics must describe the same error");
    }
    check(firstReported && secondReported, "Neither invalid fragment may be silently omitted");
    check(functionNamed(unit, "first").validity == Validity::unavailable &&
          functionNamed(unit, "second").validity == Validity::unavailable &&
          functionNamed(unit, "good").validity == Validity::valid,
          "Errors in two functions must not invalidate a separate correct function");
}

void missingSyntaxTokenHasPointRange() {
    const std::string bytes =
        "int broken() { return 1 }\n"
        "int good() { return 2; }\n";
    const auto parsed = parseSuccessfully(source({{"missing.cpp", bytes}}));
    const auto& unit = parsed.units.front();
    check(unit.diagnostics.size() == 1, "A missing semicolon must yield one diagnostic");
    const auto& diagnostic = unit.diagnostics.front();
    check(diagnostic.error.message == "Missing syntax token: ;" && diagnostic.range,
          "Missing-token diagnostics must identify the expected punctuation token");
    const auto& range = *diagnostic.range;
    const auto expressionEnd = bytes.find("return 1") + std::string_view("return 1").size();
    check(range.startByte == range.endByte && range.startByte >= expressionEnd &&
          range.endByte <= bytes.find('}') && range.startLine == 1 && range.endLine == 1,
          "A missing token must identify a zero-width insertion point before the closing brace");
    check(textAt(bytes, range).empty(), "Missing tokens must not claim existing source bytes");
    check(unit.coverage.diagnostics.size() == 1 && parsed.coverage.diagnostics.size() == 1 &&
          parsed.coverage.completeness == Completeness::partial,
          "Missing-token errors must propagate into partial coverage");
    check(functionNamed(unit, "broken").validity == Validity::unavailable &&
          functionNamed(unit, "good").validity == Validity::valid,
          "A missing token must only invalidate the affected function");
}

void validSyntaxHasNoDiagnostics() {
    const auto parsed = parseSuccessfully(source({{"valid.cpp",
        "int good() { return 2; }\n"}}));
    const auto& unit = parsed.units.front();
    check(unit.diagnostics.empty() && unit.coverage.diagnostics.empty() &&
          parsed.coverage.diagnostics.empty(),
          "Valid syntax must not produce recovery diagnostics");
    check(unit.coverage.completeness == Completeness::complete &&
          parsed.coverage.completeness == Completeness::complete &&
          unit.root->parseState == Validity::valid,
          "Valid syntax must retain complete coverage and file status");
}

void eligibilityUsesConcreteDiagnosticReasons() {
    const auto parsed = parseSuccessfully(source({
        {"affected.cpp", "int broken() { return 1 }int good() { return 2; }"},
        {"separate.cpp", "int broken() { return 1; }"}
    }));
    const auto& affected = parsed.units[0];
    const auto& broken = functionNamed(affected, "broken");
    check(broken.validity == Validity::unavailable &&
          broken.reason == "Missing syntax token: ;",
          "A missing-token diagnostic must supply the affected function's concrete reason");
    check(affected.diagnostics.size() == 1 &&
          affected.diagnostics[0].error.message == broken.reason,
          "Function eligibility must agree with the collected diagnostic");
    const auto& good = functionNamed(affected, "good");
    check(good.validity == Validity::valid && good.reason.empty(),
          "A neighboring function must remain valid without inheriting another function's reason");
    const auto& separate = functionNamed(parsed.units[1], "broken");
    check(separate.validity == Validity::valid && separate.reason.empty() &&
          parsed.units[1].coverage.completeness == Completeness::complete,
          "Errors must not affect a same-named function at similar offsets in a different file");
}

void eligibilityExcludesErrorsBetweenFunctions() {
    const auto parsed = parseSuccessfully(source({{"between.cpp",
        "int before() { return 1; }@@@int after() { return 2; }"}}));
    const auto& unit = parsed.units.front();
    const auto& before = functionNamed(unit, "before");
    const auto& after = functionNamed(unit, "after");
    check(unit.coverage.completeness == Completeness::partial && !unit.diagnostics.empty(),
          "Invalid file-level text must remain visible in coverage");
    for (const auto& diagnostic : unit.diagnostics) {
        check(diagnostic.range && diagnostic.range->startByte >= before.range.endByte &&
              diagnostic.range->endByte <= after.range.startByte,
              "The fixture's error ranges must lie between the two function ranges");
    }
    check(before.validity == Validity::valid && after.validity == Validity::valid &&
          before.reason.empty() && after.reason.empty(),
          "Nonempty errors touching function endpoints must not invalidate either function");
}

void eligibilityIncludesMissingClosingBraceAtEndpoint() {
    const std::string bytes = "int unfinished() { return 1;";
    const auto parsed = parseSuccessfully(source({{"unfinished.cpp", bytes}}));
    const auto& unit = parsed.units.front();
    const auto& function = functionNamed(unit, "unfinished");
    check(unit.diagnostics.size() == 1 && unit.diagnostics[0].range,
          "A missing closing brace must have a concrete diagnostic");
    const auto& diagnostic = unit.diagnostics[0];
    check(diagnostic.error.message == "Missing syntax token: }" &&
          diagnostic.range->startByte == bytes.size() &&
          diagnostic.range->endByte == bytes.size() && function.range.endByte == bytes.size(),
          "The missing brace must be represented as an insertion point at the function endpoint");
    check(function.validity == Validity::unavailable && function.reason == diagnostic.error.message,
          "A missing token at the endpoint must make the function unavailable with a concrete reason");
}

void conditionalAncestorsMarkAllBranches() {
    const auto parsed = parseSuccessfully(source({{"conditional.cpp", R"cpp(
#if 0
namespace app {
class Worker { public: void run() {} };
}
#elif OTHER
void alternative() {}
#else
void fallback() {}
#endif
#ifdef FEATURE
void enabled() {}
#endif
#ifndef DISABLED
void defaultEnabled() {}
#endif
void ordinary() {}
)cpp"}}));
    const auto& unit = parsed.units.front();
    check(unit.functions.size() == 6, "All syntactic branches must remain visible, including #if 0");
    for (const auto name : {"run", "alternative", "fallback", "enabled", "defaultEnabled"}) {
        const auto& function = functionNamed(unit, name);
        check(function.validity == Validity::syntactic_only && !function.reason.empty(),
              "Conditional ancestors must mark every branch, including methods inside namespaces and classes");
    }
    const auto& ordinary = functionNamed(unit, "ordinary");
    check(ordinary.validity == Validity::valid && ordinary.reason.empty(),
          "Conditional status must not leak past #endif");
    check(unit.diagnostics.empty() && parsed.coverage.completeness == Completeness::complete,
          "Unevaluated conditional compilation is a precision limitation, not a syntax failure");
}

void conditionalBodyPreservesControlTree() {
    const auto parsed = parseSuccessfully(source({{"conditional-body.cpp", R"cpp(
int choose(int value) {
#if FEATURE
    if (value) { return 1; }
#elif OTHER
    while (value) { --value; }
#else
    return value ? 2 : 3;
#endif
    return 0;
}
#define FEATURE 1
#include "unused.hpp"
int ordinary() { return 4; }
)cpp"}}));
    const auto& unit = parsed.units.front();
    const auto& function = functionNamed(unit, "choose");
    check(function.validity == Validity::syntactic_only && !function.reason.empty(),
          "A conditional inside the body must mark a function with no conditional ancestors");
    check(countControlKind(function.controlTree, ControlKind::if_statement) == 1 &&
          countControlKind(function.controlTree, ControlKind::while_loop) == 1 &&
          countControlKind(function.controlTree, ControlKind::conditional_expression) == 1,
          "The syntactic tree must retain control constructs from all unevaluated branches");
    check(functionNamed(unit, "ordinary").validity == Validity::valid,
          "Unrelated #define and #include directives must not mark ordinary functions as conditional");
    check(unit.diagnostics.empty() && unit.coverage.completeness == Completeness::complete,
          "A valid conditional body must retain complete syntactic coverage");
}

void conditionalSyntaxErrorsRemainUnavailable() {
    const auto parsed = parseSuccessfully(source({{"conditional-errors.cpp", R"cpp(
#ifdef FEATURE
int broken() { return 1 }
int good() { return 2; }
#endif
int brokenInside() {
#if FEATURE
    return 3
#endif
}
int ordinary() { return 4; }
)cpp"}}));
    const auto& unit = parsed.units.front();
    for (const auto name : {"broken", "brokenInside"}) {
        const auto& function = functionNamed(unit, name);
        check(function.validity == Validity::unavailable &&
              function.reason == "Missing syntax token: ;",
              "Syntax errors must outrank conditional precision and retain their concrete reason");
    }
    check(functionNamed(unit, "good").validity == Validity::syntactic_only &&
          functionNamed(unit, "ordinary").validity == Validity::valid,
          "Neighboring conditional and ordinary functions must retain their own eligibility");
    check(parsed.coverage.completeness == Completeness::partial,
          "Syntax errors inside conditional branches must still make coverage partial");
}

void declarationsHaveNoApplicableBody() {
    const std::string bytes = "int calculate(int value);\n"
        "int first(), second(int), value;\n"
        "int calculate(int value) { return value; }\n";
    const auto parsed = parseSuccessfully(source({{"declarations.cpp", bytes}}));
    const auto& unit = parsed.units.front();
    check(unit.functions.size() == 4 && unit.root->children.size() == 4,
          "Every function declarator must appear once, while ordinary variables are excluded");
    for (std::size_t index = 0; index < 3; ++index) {
        const auto& function = *unit.functions[index];
        check(function.validity == Validity::not_applicable && !function.reason.empty() &&
              !function.bodyRange && function.controlTree.children.empty(),
              "Declarations must have no body or applicable metrics");
        check(reachable(*unit.root, &function),
              "Declarations in the function index must be the same entities stored in the outline");
    }
    check(unit.functions[0]->name == "calculate" && unit.functions[1]->name == "first" &&
          unit.functions[2]->name == "second" && unit.functions[3]->name == "calculate",
          "Multiple declarators must retain their own names and source order");
    check(unit.functions[3]->validity == Validity::valid && unit.functions[3]->bodyRange,
          "Definition extraction must still retain a usable function body");
    check(unit.functions[0]->identityKey == unit.functions[3]->identityKey &&
          unit.functions[0]->id != unit.functions[3]->id,
          "Declaration and definition share an identity key but remain separate snapshot entities");
    check(textAt(bytes, unit.functions[0]->range) == "int calculate(int value);",
          "Declaration ranges must include the terminating semicolon");
    check(unit.diagnostics.empty() && unit.coverage.completeness == Completeness::complete,
          "Correct declarations must not make coverage partial");
}

void declarationsDistinguishFunctionsFromPointers() {
    const auto parsed = parseSuccessfully(source({{"declarators.cpp", R"cpp(
int* pointerResult(int);
int& referenceResult();
int (parenthesized)(int);
int (*factory())(int);
int (*callback)(int);
int (&reference)(int);
int (*callbacks[2])(int);
int initialized = 1;
)cpp"}}));
    const auto& unit = parsed.units.front();
    check(unit.functions.size() == 4,
          "Only function declarations, not pointer/reference/array variables, belong in the index");
    for (const auto name : {"pointerResult", "referenceResult", "parenthesized", "factory"}) {
        check(functionNamed(unit, name).validity == Validity::not_applicable,
              "Functions with wrapped declarators must remain recognizable declarations");
    }
    check(unit.diagnostics.empty(), "Valid declarator shapes must not generate diagnostics");
}

void methodDeclarationsPreserveTypeStructure() {
    const auto parsed = parseSuccessfully(source({{"methods.hpp", R"cpp(
namespace app {
struct Widget {
    Widget();
    ~Widget();
    void run() const;
    virtual int value() = 0;
    int operator+(int) const;
    void (*callback)(int);
    int count;
} instance;
}
)cpp"}}));
    const auto& unit = parsed.units.front();
    check(unit.root->children.size() == 1 && unit.root->children[0]->kind == EntityKind::type,
          "An inline type definition must still be extracted from a variable declaration");
    const auto& type = *unit.root->children[0];
    check(unit.functions.size() == 5 && type.children.size() == 5,
          "Constructors, destructors, methods and operators must be retained without data members");
    for (const auto& function : unit.functions) {
        check(function->ownerName == "app::Widget" &&
              function->validity == Validity::not_applicable && !function->bodyRange &&
              reachable(type, function.get()),
              "Method declarations must retain lexical ownership and the shared function index");
    }
    check(functionNamed(unit, "Widget").name == "Widget" &&
          functionNamed(unit, "~Widget").name == "~Widget" &&
          functionNamed(unit, "operator+").name == "operator+",
          "Special method names must use the existing name extraction");
    check(unit.diagnostics.empty(), "Valid method declarations must parse without diagnostics");
}

void conditionalDeclarationsRemainNotApplicable() {
    const auto parsed = parseSuccessfully(source({{"conditional-declarations.hpp", R"cpp(
#ifdef FEATURE
int declared(int);
template<class T> T convert(T value);
#endif
int ordinary();
)cpp"}}));
    const auto& unit = parsed.units.front();
    check(unit.functions.size() == 3, "Conditional and template declarations must all be visible");
    for (const auto& function : unit.functions) {
        check(function->validity == Validity::not_applicable && !function->bodyRange,
              "The absence of a body must outrank conditional precision for metric eligibility");
    }
    check(unit.diagnostics.empty() && parsed.coverage.completeness == Completeness::complete,
          "Correct conditional declarations must retain complete syntactic coverage");
}

struct TestCase {
    const char* name;
    void (*run)();
};
} // namespace

int main() {
    const TestCase cases[] = {
        {"empty inputs", emptyInputs},
        {"invalid inputs and versions", invalidInputs},
        {"original UTF-8/BOM bytes and LF/CRLF ranges", originalBytesAndRanges},
        {"nested structure and shared function index", nestedStructureAndSharedIndex},
        {"explicitly qualified methods", explicitlyQualifiedMethods},
        {"pointer/reference/parenthesized/operator declarators", declaratorShapes},
        {"template function", templateFunction},
        {"syntax errors preserve valid functions", syntaxErrorsPreserveValidFunctions},
        {"inherited coverage and multiple files", inheritedCoverageAndMultipleFiles},
        {"source lifetime", sourceLifetime},
        {"progress", progress},
        {"cancellation before start", cancellationBeforeStart},
        {"cancellation from progress", cancellationFromProgress},
        {"cancellation retains diagnostics", cancellationKeepsDiagnostics},
        {"declarator tokens ignore formatting", declaratorTokensIgnoreFormatting},
        {"literal contents remain significant", literalContentsRemainSignificant},
        {"identity ignores body edits and line shifts", identityIgnoresBodyAndLineShifts},
        {"identity uses length-prefixed UTF-8 fields", identityUsesLengthPrefixedUtf8Fields},
        {"identities distinguish paths, scopes and overloads", identitiesDistinguishPathsScopesAndOverloads},
        {"formatted namespace preserves identity", formattedNamespacePreservesIdentity},
        {"formatted template owner preserves identity", formattedTemplateOwnerPreservesIdentity},
        {"entity IDs are unique and repeatable", entityIdsAreUniqueAndRepeatable},
        {"duplicate keys retain separate entities", duplicateKeysRetainSeparateEntities},
        {"control tree branch levels", controlTreePreservesBranchLevels},
        {"control tree loops and labels", controlTreeRecognizesLoopsAndLabels},
        {"control tree expressions through wrappers", controlTreeFindsExpressionsThroughWrappers},
        {"control tree inner-body boundaries", controlTreeExcludesInnerBodies},
        {"control tree original ranges", controlTreeKeepsOriginalRanges},
        {"syntax diagnostics have concrete ranges", syntaxDiagnosticsHaveConcreteRanges},
        {"missing syntax token has a point range", missingSyntaxTokenHasPointRange},
        {"valid syntax has no diagnostics", validSyntaxHasNoDiagnostics},
        {"eligibility uses concrete diagnostic reasons", eligibilityUsesConcreteDiagnosticReasons},
        {"eligibility excludes errors between functions", eligibilityExcludesErrorsBetweenFunctions},
        {"eligibility includes missing closing brace at endpoint", eligibilityIncludesMissingClosingBraceAtEndpoint},
        {"conditional ancestors mark all branches", conditionalAncestorsMarkAllBranches},
        {"conditional body preserves control tree", conditionalBodyPreservesControlTree},
        {"conditional syntax errors remain unavailable", conditionalSyntaxErrorsRemainUnavailable},
        {"declarations have no applicable body", declarationsHaveNoApplicableBody},
        {"declarations distinguish functions from pointers", declarationsDistinguishFunctionsFromPointers},
        {"method declarations preserve type structure", methodDeclarationsPreserveTypeStructure},
        {"conditional declarations remain not applicable", conditionalDeclarationsRemainNotApplicable}
    };
    int failures = 0;
    for (const auto& test : cases) {
        try {
            test.run();
            std::cout << "PASS: " << test.name << '\n';
        } catch (const std::exception& error) {
            ++failures;
            std::cerr << "FAIL: " << test.name << ": " << error.what() << '\n';
        }
    }
    return failures == 0 ? 0 : 1;
}
