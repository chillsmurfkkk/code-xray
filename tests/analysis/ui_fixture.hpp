#pragma once
#include "analysis/types.hpp"
namespace analysis_test {
// Hand-built parser-output fixture, never presented as a real parsed project.
inline std::shared_ptr<const xray::code::ParsedSnapshot> fixture() {
    namespace c = xray::code;
    auto source = std::make_shared<c::SourceSnapshot>();
    source->id = "B-control-fixture-v2";
    auto parsed = std::make_shared<c::ParsedSnapshot>();
    parsed->source = source;
    const std::array<int, 5> lines{61, 2, 5, 1, 1};
    for (int group = 0; group < 2; ++group) {
        c::ParsedUnit unit;
        unit.relativePath = group == 0 ? "src/example.cpp" : "include/example.hpp";
        auto root = std::make_shared<c::FileEntity>();
        root->relativePath = unit.relativePath;
        root->id = "file" + std::to_string(group);
        std::string bytes;
        std::uint32_t line = 1;
        for (int i = group == 0 ? 0 : 3; i < (group == 0 ? 3 : 5); ++i) {
            auto fn = std::make_shared<c::FunctionEntity>();
            fn->id = "fn" + std::to_string(i);
            fn->name = fn->id;
            fn->relativePath = unit.relativePath;
            const auto start = bytes.size();
            if (i == 3)
                bytes += "void fn3( {";
            else if (i == 4)
                bytes += "void fn4();";
            else {
                bytes += "void " + fn->name + "() {\n";
                for (int j = 0; j < lines[i] - 2; ++j)
                    bytes += "    // control fixture\n";
                bytes += "}";
            }
            fn->range = {start, bytes.size(), line, line + static_cast<std::uint32_t>(lines[i]) - 1};
            fn->bodyRange = fn->range;
            bytes += '\n';
            line += lines[i];
            if (i == 2) {
                fn->validity = c::Validity::syntactic_only;
                fn->reason = "Control fixture: syntax only";
            }
            if (i == 3) {
                fn->validity = c::Validity::unavailable;
                fn->reason = "Control fixture: parse error";
            }
            if (i == 4) {
                fn->validity = c::Validity::not_applicable;
                fn->bodyRange.reset();
            }
            root->children.push_back(fn);
            unit.functions.push_back(fn);
        }
        auto retained = std::make_shared<const std::string>(std::move(bytes));
        root->sourceBytes = retained;
        root->range = {0, retained->size(), 1, line - 1};
        unit.root = root;
        source->files.push_back({unit.relativePath, retained, "control"});
        parsed->units.push_back(std::move(unit));
    }
    return parsed;
}
} // namespace analysis_test
