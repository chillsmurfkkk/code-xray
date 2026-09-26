find_package(Qt6 6.11.1 EXACT CONFIG REQUIRED COMPONENTS Widgets)
find_package(unofficial-tree-sitter CONFIG REQUIRED)
find_package(libgit2 CONFIG REQUIRED)
find_package(nlohmann_json 3.12.0 EXACT CONFIG REQUIRED)
find_package(inja 3.5.0 EXACT CONFIG REQUIRED)

include(FetchContent)
FetchContent_Declare(tree_sitter_cpp
    GIT_REPOSITORY https://github.com/tree-sitter/tree-sitter-cpp.git
    GIT_TAG f41e1a044c8a84ea9fa8577fdd2eab92ec96de02
    SOURCE_SUBDIR unused-e0-subdirectory)
FetchContent_MakeAvailable(tree_sitter_cpp)
add_library(xray_cpp_grammar STATIC
    "${tree_sitter_cpp_SOURCE_DIR}/src/parser.c"
    "${tree_sitter_cpp_SOURCE_DIR}/src/scanner.c")
target_include_directories(xray_cpp_grammar PRIVATE "${tree_sitter_cpp_SOURCE_DIR}/src")
set_target_properties(xray_cpp_grammar PROPERTIES C_STANDARD 11 C_STANDARD_REQUIRED YES)
if(MSVC)
    target_compile_options(xray_cpp_grammar PRIVATE /utf-8 /bigobj)
endif()
