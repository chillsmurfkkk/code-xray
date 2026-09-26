#include <git2.h>
#include <inja/inja.hpp>
#include <nlohmann/json.hpp>
#include <tree_sitter/api.h>

#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>

extern "C" const TSLanguage* tree_sitter_cpp();

int main() {
    try {
        std::unique_ptr<TSParser, decltype(&ts_parser_delete)> parser(ts_parser_new(), ts_parser_delete);
        if (!parser || !ts_parser_set_language(parser.get(), tree_sitter_cpp())) {
            throw std::runtime_error("Tree-sitter runtime / C++ grammar ABI mismatch");
        }
        const std::string source = "int f() { return 1; }";
        std::unique_ptr<TSTree, decltype(&ts_tree_delete)> tree(
            ts_parser_parse_string(parser.get(), nullptr, source.data(), static_cast<uint32_t>(source.size())),
            ts_tree_delete);
        if (!tree || ts_node_has_error(ts_tree_root_node(tree.get()))) {
            throw std::runtime_error("C++ grammar failed to parse fixture");
        }
        if (git_libgit2_init() < 0) {
            throw std::runtime_error("libgit2 initialization failed");
        }
        git_oid oid{};
        const int oidStatus = git_oid_fromstr(&oid, "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa");
        git_libgit2_shutdown();
        if (oidStatus != 0) {
            throw std::runtime_error("libgit2 failed to parse OID");
        }
        const auto data = nlohmann::json::parse(R"({"name":"Code Xray","unknown":null})");
        if (inja::render("Hello {{ name }}", data) != "Hello Code Xray"
            || !nlohmann::json::parse(data.dump())["unknown"].is_null()) {
            throw std::runtime_error("inja / JSON integration failed");
        }
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
