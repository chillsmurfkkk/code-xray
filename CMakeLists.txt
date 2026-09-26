cmake_minimum_required(VERSION 3.25)
project(CodeXray VERSION 0.0.1 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
if(NOT WIN32 OR NOT MSVC OR NOT CMAKE_CXX_COMPILER_ARCHITECTURE_ID STREQUAL "x64")
    message(FATAL_ERROR "E0 supports Windows x64 with MSVC only; use the documented presets.")
endif()

option(CODE_XRAY_BUILD_APP "Build the Qt application and dependency smoke test" ON)
include(CTest)

add_library(xray_common INTERFACE)
target_include_directories(xray_common INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_compile_features(xray_common INTERFACE cxx_std_20)
if(MSVC)
    target_compile_options(xray_common INTERFACE /W4 /permissive- /utf-8)
endif()
add_library(xray_code INTERFACE)
target_link_libraries(xray_code INTERFACE xray_common)
add_library(xray_analysis INTERFACE)
target_link_libraries(xray_analysis INTERFACE xray_code)
add_library(xray_history INTERFACE)
target_link_libraries(xray_history INTERFACE xray_code)
add_library(xray_review INTERFACE)
target_link_libraries(xray_review INTERFACE xray_analysis)

if(BUILD_TESTING)
    add_executable(contract_tests tests/contracts.cpp)
    target_link_libraries(contract_tests PRIVATE xray_history xray_review)
    add_test(NAME contracts COMMAND contract_tests)
    # Each public header must compile without another header included first.
    set(contract_headers
        common/job.hpp common/result.hpp
        code/types.hpp code/api.hpp
        analysis/types.hpp analysis/api.hpp
        history/types.hpp history/api.hpp
        review/types.hpp review/api.hpp)
    foreach(relative_header IN LISTS contract_headers)
        string(MAKE_C_IDENTIFIER "${relative_header}" header_id)
        file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/${header_id}.cpp" "#include \"${relative_header}\"\n")
        add_library("header_${header_id}" OBJECT "${CMAKE_CURRENT_BINARY_DIR}/${header_id}.cpp")
        target_link_libraries("header_${header_id}" PRIVATE xray_common)
    endforeach()
endif()

if(CODE_XRAY_BUILD_APP)
    include(cmake/Dependencies.cmake)
    qt_add_executable(code-xray WIN32 src/app/main.cpp)
    target_link_libraries(code-xray PRIVATE Qt6::Widgets xray_history xray_review)
    add_custom_command(TARGET code-xray POST_BUILD
        COMMAND "$<TARGET_FILE:Qt6::windeployqt>" --no-translations --no-compiler-runtime "$<TARGET_FILE:code-xray>"
        VERBATIM)
    if(BUILD_TESTING)
        add_executable(dependency_smoke tests/dependencies.cpp)
        target_link_libraries(dependency_smoke PRIVATE xray_common xray_cpp_grammar
            unofficial::tree-sitter::tree-sitter libgit2::libgit2package
            nlohmann_json::nlohmann_json pantor::inja)
        add_test(NAME dependencies COMMAND dependency_smoke)
        add_test(NAME gui_startup COMMAND code-xray --smoke-test)
        set_tests_properties(gui_startup PROPERTIES TIMEOUT 15)
    endif()
endif()
