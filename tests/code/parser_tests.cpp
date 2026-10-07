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
    check(unit.functions.size() == 2, "Expected both out-of-class definitions");
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
        {"duplicate keys retain separate entities", duplicateKeysRetainSeparateEntities}
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
