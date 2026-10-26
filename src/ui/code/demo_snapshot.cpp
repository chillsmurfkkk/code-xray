#include "demo_snapshot.hpp"

#include <string>

namespace xray::ui {

std::shared_ptr<const xray::code::ParsedSnapshot> makeDemoSnapshot() {
    using namespace xray::code;

    const std::string path = "demo/example.cpp";

    auto bytes = std::make_shared<const std::string>(
        "int square(int value) {\n"
        "    return value * value;\n"
        "}"
    );

    auto source = std::make_shared<SourceSnapshot>();
    source->id = "demo:structure";
    source->files.push_back({path, bytes, "demo:square-v1"});

    auto function = std::make_shared<FunctionEntity>();
    function->id = "demo:function:square";
    function->name = "square";
    function->relativePath = path;
    function->parameterCount = 1;
    function->signatureTokens = {
        "int", "square", "(", "int", "value", ")"
    };
    function->identityKey =
        "16:demo/example.cpp6:square3:int6:square1:(3:int5:value1:)";

    function->range = {0, bytes->size(), 1, 3};
    function->bodyRange = SourceRange{
        bytes->find('{'), bytes->size(), 1, 3
    };
    function->controlTree.range = *function->bodyRange;

    auto file = std::make_shared<FileEntity>();
    file->id = "demo:file:example";
    file->name = "example.cpp";
    file->relativePath = path;
    file->contentId = source->files.front().contentId;
    file->sourceBytes = bytes;
    file->range = function->range;
    file->children.push_back(function);

    auto parsed = std::make_shared<ParsedSnapshot>();
    parsed->source = source;
    parsed->units.push_back({
        path,
        file,
        {function},
        {},
        {}
    });

    return parsed;
}

} // namespace xray::ui
