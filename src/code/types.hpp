#pragma once

#include "common/result.hpp"

#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace xray::code {
using EntityId = std::string;

// Byte offsets refer to original UTF-8 bytes, including BOM and CRLF.
// endLine is the line of the LAST token, not the exclusive byte endpoint.
struct SourceRange {
    std::uint64_t startByte = 0;
    std::uint64_t endByte = 0;
    std::uint32_t startLine = 1;
    std::uint32_t endLine = 1;
};

struct FileSelection {
    std::vector<std::string> extensions{".cpp", ".cc", ".cxx", ".h", ".hpp", ".hxx"};
    std::vector<std::string> excludedDirectories{".git", "build", "vendor", "third_party"};
    std::vector<std::string> relativePaths; // Empty means all matching paths.
    bool operator==(const FileSelection&) const = default;
};

struct SourceRequest {
    std::filesystem::path root;
    FileSelection selection;
};

enum class SourceKind { local, git };
struct SourceFile {
    std::string relativePath;
    std::shared_ptr<const std::string> bytes;
    std::string contentId;
};
struct SourceSnapshot {
    std::string id;
    SourceKind kind = SourceKind::local;
    std::optional<std::string> commitOid;
    FileSelection selection;
    std::vector<SourceFile> files;
    Coverage coverage;
};

enum class Validity { valid, syntactic_only, not_applicable, unavailable };
enum class EntityKind { file, function, type };
enum class ControlKind {
    block, if_statement, else_if, for_loop, range_for, while_loop, do_loop,
    switch_statement, case_label, default_label, try_statement, catch_clause,
    conditional_expression, lambda, local_type
};
struct ControlNode {
    ControlKind kind = ControlKind::block;
    SourceRange range;
    std::vector<ControlNode> children;
};
struct ParseDiagnostic {
    std::string relativePath;
    std::optional<SourceRange> range;
    Error error;
};

struct CodeEntity {
    explicit CodeEntity(EntityKind entityKind) : kind(entityKind) {}
    virtual ~CodeEntity() = default;
    EntityId id;
    std::string name;
    const EntityKind kind;
    SourceRange range;
    std::vector<std::shared_ptr<const CodeEntity>> children;
    [[nodiscard]] virtual std::string describe() const = 0;
};
struct FileEntity : CodeEntity {
    FileEntity() : CodeEntity(EntityKind::file) {}
    std::string relativePath;
    std::string contentId;
    std::string languageId = "cpp";
    std::shared_ptr<const std::string> sourceBytes;
    Validity parseState = Validity::valid;
    std::string describe() const override { return relativePath; }
};
struct FunctionEntity : CodeEntity {
    FunctionEntity() : CodeEntity(EntityKind::function) {}
    std::string relativePath;
    std::string identityKey;
    std::vector<std::string> signatureTokens;
    std::optional<SourceRange> bodyRange;
    std::size_t parameterCount = 0;
    std::string qualifiers;
    std::string ownerName;
    Validity validity = Validity::valid;
    std::string reason;
    ControlNode controlTree;
    std::string describe() const override { return ownerName.empty() ? name : ownerName + "::" + name; }
};
struct TypeEntity : CodeEntity {
    TypeEntity() : CodeEntity(EntityKind::type) {}
    std::string qualifiedName;
    std::string describe() const override { return qualifiedName; }
};

struct ParseOptions {
    std::string grammarVersion = "tree-sitter-cpp@f41e1a044c8a84ea9fa8577fdd2eab92ec96de02";
    std::string structureVersion = "cpp-structure-v1";
    bool operator==(const ParseOptions&) const = default;
};
struct ParsedUnit {
    std::string relativePath;
    std::shared_ptr<const FileEntity> root;
    // Index of the SAME immutable function objects reachable from root.
    std::vector<std::shared_ptr<const FunctionEntity>> functions;
    std::vector<ParseDiagnostic> diagnostics;
    Coverage coverage;
};
struct ParsedSnapshot {
    std::shared_ptr<const SourceSnapshot> source;
    ParseOptions options;
    std::vector<ParsedUnit> units;
    Coverage coverage;
};
} // namespace xray::code
