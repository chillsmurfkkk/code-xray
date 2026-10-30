cmake_minimum_required(VERSION 3.25)
if(APPLE AND NOT CMAKE_OSX_SYSROOT)
    execute_process(COMMAND xcrun --sdk macosx --show-sdk-path
        OUTPUT_VARIABLE _MACOSX_SDK_PATH
        OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
    if(_MACOSX_SDK_PATH)
        set(CMAKE_OSX_SYSROOT "${_MACOSX_SDK_PATH}" CACHE PATH "macOS SDK root")
    endif()
endif()

project(CodeXray VERSION 0.0.1 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
if(WIN32)
    if(NOT MSVC OR NOT CMAKE_CXX_COMPILER_ARCHITECTURE_ID STREQUAL "x64")
        message(FATAL_ERROR "E0 supports Windows x64 with MSVC only; use the documented presets.")
    endif()
else()
    message(STATUS "Configuring on non-Windows platform: ${CMAKE_SYSTEM_NAME} (${CMAKE_CXX_COMPILER_ID})")
endif()

option(CODE_XRAY_BUILD_APP "Build the Qt application and dependency smoke test" ON)
option(CODE_XRAY_BUILD_PROFILES "Build JSON profile persistence and tests" ON)
option(CODE_XRAY_BUILD_EXPORT "Build report exporter (HTML/JSON via inja/nlohmann_json) and export tests" ON)
include(CTest)

add_library(xray_common INTERFACE)
target_include_directories(xray_common INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_compile_features(xray_common INTERFACE cxx_std_20)

if(MSVC)
    target_compile_options(xray_common INTERFACE /W4 /permissive- /utf-8)
else()
    target_compile_options(xray_common INTERFACE -Wall -Wextra -Wpedantic)
endif()

add_library(xray_code STATIC
    src/code/source.cpp
)
target_link_libraries(xray_code PUBLIC xray_common)

add_library(xray_analysis STATIC
    src/analysis/metrics.cpp
    src/analysis/rules.cpp
)
target_include_directories(xray_analysis PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_compile_features(xray_analysis PUBLIC cxx_std_20)
target_link_libraries(xray_analysis PUBLIC xray_code)

if(CODE_XRAY_BUILD_PROFILES)
    find_package(nlohmann_json 3.12.0 EXACT CONFIG REQUIRED)
    target_sources(xray_analysis PRIVATE src/analysis/profiles.cpp src/analysis/engine.cpp)
    target_link_libraries(xray_analysis PRIVATE nlohmann_json::nlohmann_json)
endif()

option(CODE_XRAY_BUILD_HISTORY "Build libgit2-backed history module" ON)

if(CODE_XRAY_BUILD_HISTORY)
    find_package(libgit2 CONFIG REQUIRED)
    add_library(xray_history STATIC src/history/api.cpp)
    target_link_libraries(xray_history PUBLIC xray_code PRIVATE libgit2::libgit2package)
    target_compile_definitions(xray_history PUBLIC CODE_XRAY_HAS_HISTORY=1)
else()
    add_library(xray_history INTERFACE)
    target_link_libraries(xray_history INTERFACE xray_code)
endif()
add_library(xray_review STATIC
    src/review/compare.cpp
    src/review/trend.cpp
)
target_link_libraries(xray_review PUBLIC xray_analysis)

if(CODE_XRAY_BUILD_EXPORT)
    find_package(nlohmann_json CONFIG REQUIRED)
    find_package(inja CONFIG REQUIRED)
    target_sources(xray_review PRIVATE src/review/exporter.cpp)
    target_link_libraries(xray_review PUBLIC
        nlohmann_json::nlohmann_json
        pantor::inja
    )
    target_compile_definitions(xray_review PUBLIC CODE_XRAY_HAS_EXPORTER=1)
else()
    target_sources(xray_review PRIVATE src/review/exporter_stub.cpp)
endif()

if(BUILD_TESTING)
    add_executable(review_tests tests/review/review_tests.cpp)
    target_link_libraries(review_tests PRIVATE xray_review)
    add_test(NAME review COMMAND review_tests)

    add_executable(compare_tests tests/review/compare_tests.cpp)
    target_link_libraries(compare_tests PRIVATE xray_review)
    add_test(NAME compare COMMAND compare_tests)

    add_executable(trend_tests tests/review/trend_tests.cpp)
    target_link_libraries(trend_tests PRIVATE xray_review)
    add_test(NAME trend COMMAND trend_tests)

    if(CODE_XRAY_BUILD_EXPORT)
        add_executable(export_tests tests/review/export_tests.cpp)
        target_link_libraries(export_tests PRIVATE xray_review)
        add_test(NAME export COMMAND export_tests)
    endif()

    add_executable(history_test tests/history_test.cpp)
    target_link_libraries(history_test PRIVATE xray_history)
    add_test(NAME history_test COMMAND history_test)

    if(CODE_XRAY_BUILD_HISTORY)
        add_executable(git_backend_tests tests/history/git_backend_tests.cpp)
        target_link_libraries(git_backend_tests PRIVATE xray_history)
        add_test(NAME git_backend COMMAND git_backend_tests)
        set_tests_properties(git_backend PROPERTIES TIMEOUT 30)
    endif()

    add_executable(contract_tests tests/contracts.cpp)
    target_link_libraries(contract_tests PRIVATE xray_history xray_review)
    add_test(NAME contracts COMMAND contract_tests)

    add_executable(analysis_tests tests/analysis_tests.cpp)
    target_link_libraries(analysis_tests PRIVATE xray_analysis)
    add_test(NAME analysis COMMAND analysis_tests)

    if(CODE_XRAY_BUILD_PROFILES)
        add_executable(engine_tests tests/analysis/engine_tests.cpp)
        target_link_libraries(engine_tests PRIVATE xray_review)
        add_test(NAME analysis_engine COMMAND engine_tests)
        add_executable(analysis_demo tests/analysis/demo.cpp)
        target_link_libraries(analysis_demo PRIVATE xray_analysis)
        add_executable(profile_tests tests/analysis/profile_tests.cpp)
        target_link_libraries(profile_tests PRIVATE xray_analysis nlohmann_json::nlohmann_json)
        add_test(NAME profiles COMMAND profile_tests)
        file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/analysis_profiles_hpp.cpp" "#include \"analysis/profiles.hpp\"\n")
        add_library(header_analysis_profiles_hpp OBJECT "${CMAKE_CURRENT_BINARY_DIR}/analysis_profiles_hpp.cpp")
        target_link_libraries(header_analysis_profiles_hpp PRIVATE xray_common)
    endif()

    add_executable(source_provider_test tests/source_provider_test.cpp)
    target_link_libraries(source_provider_test PRIVATE xray_code)
    add_test(NAME source_provider COMMAND source_provider_test)

    set(contract_headers
        common/job.hpp common/result.hpp
        code/types.hpp code/api.hpp
        analysis/types.hpp analysis/api.hpp analysis/engine.hpp analysis/aggregation.hpp
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

    target_sources(xray_code PRIVATE
        src/code/parse.cpp
        src/code/load.cpp
    )
    target_link_libraries(xray_code PRIVATE
        xray_cpp_grammar
        unofficial::tree-sitter::tree-sitter
    )

    if(NOT CODE_XRAY_BUILD_PROFILES)
        message(FATAL_ERROR "Analysis pages require CODE_XRAY_BUILD_PROFILES=ON")
    endif()

    # Shared GUI components can be reused by the app and its behavior checks.
    add_library(xray_ui STATIC
        src/app/analysis_controller.cpp src/app/analysis_controller.hpp
        src/ui/analysis/session.cpp src/ui/analysis/session.hpp
        src/ui/analysis/profiles_page.cpp src/ui/analysis/profiles_page.hpp
        src/ui/analysis/metrics_page.cpp src/ui/analysis/metrics_page.hpp
        src/ui/analysis/rules_page.cpp src/ui/analysis/rules_page.hpp
        src/ui/analysis/findings_page.cpp src/ui/analysis/findings_page.hpp
        src/ui/analysis/workspace.cpp src/ui/analysis/workspace.hpp
        src/ui/shared/theme.cpp
        src/ui/shared/theme.hpp
        src/ui/shared/popup_style.hpp
        src/ui/shared/icons.cpp
        src/ui/shared/icons.hpp
        src/ui/shared/navigation.cpp
        src/ui/shared/navigation.hpp
        src/ui/code/structure_page.cpp
        src/ui/code/structure_page.hpp
        src/ui/code/demo_snapshot.cpp
        src/ui/code/demo_snapshot.hpp
        src/ui/code/code_view.cpp
        src/ui/code/code_view.hpp
        src/ui/code/cpp_syntax_highlighter.cpp
        src/ui/code/cpp_syntax_highlighter.hpp
        src/ui/review/ComparisonPage.cpp
        src/ui/review/ComparisonPage.hpp
        src/ui/review/TrendPage.cpp
        src/ui/review/TrendPage.hpp
        src/ui/review/ChangeCardPage.cpp
        src/ui/review/ChangeCardPage.hpp
        src/ui/review/ExportPage.cpp
        src/ui/review/ExportPage.hpp
        src/ui/review/review_table_delegate.hpp
        src/ui/history/CommitsPage.cpp
        src/ui/history/CommitsPage.hpp
        src/ui/history/HistoryFiltersPage.cpp
        src/ui/history/HistoryFiltersPage.hpp
        src/ui/history/CodeChurnPage.cpp
        src/ui/history/CodeChurnPage.hpp
        src/ui/history/AncestryPage.cpp
        src/ui/history/AncestryPage.hpp
        src/ui/history/history_fixtures.hpp
    )
    set_target_properties(xray_ui PROPERTIES AUTOMOC ON)
    target_link_libraries(xray_ui PUBLIC Qt6::Widgets xray_common xray_history xray_review)

    qt_add_executable(code-xray WIN32 src/app/main.cpp src/app/main_window.cpp src/app/main_window.hpp src/app/windows_uia_filter.hpp)
    target_link_libraries(code-xray PRIVATE xray_ui xray_history xray_review)

    if(WIN32)
        add_custom_command(TARGET code-xray POST_BUILD
            COMMAND "$<TARGET_FILE:Qt6::windeployqt>" --no-translations --no-compiler-runtime "$<TARGET_FILE:code-xray>"
            VERBATIM)
    endif()

    if(BUILD_TESTING)
        find_package(Qt6 CONFIG REQUIRED COMPONENTS Test)
        add_executable(analysis_ui_tests tests/analysis/ui_tests.cpp src/app/main_window.cpp)
        set_target_properties(analysis_ui_tests PROPERTIES AUTOMOC ON)
        target_link_libraries(analysis_ui_tests PRIVATE xray_ui xray_code Qt6::Test)
        add_test(NAME analysis_ui COMMAND analysis_ui_tests)
        set_tests_properties(analysis_ui PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen" TIMEOUT 30)

        add_executable(history_ui_tests tests/history/ui_tests.cpp src/app/main_window.cpp)
        set_target_properties(history_ui_tests PROPERTIES AUTOMOC ON)
        target_link_libraries(history_ui_tests PRIVATE xray_ui xray_history xray_code Qt6::Test)
        add_test(NAME history_ui COMMAND history_ui_tests)
        set_tests_properties(history_ui PROPERTIES TIMEOUT 30)

        add_executable(gui_design_tests tests/ui/design_tests.cpp src/app/main_window.cpp)
        target_link_libraries(gui_design_tests PRIVATE xray_ui xray_code)
        add_dependencies(gui_design_tests code-xray)
        add_test(NAME gui_design COMMAND gui_design_tests "${CMAKE_CURRENT_BINARY_DIR}/gui-design")
        set_tests_properties(gui_design PROPERTIES TIMEOUT 30)

        add_executable(gui_snapshot_tests tests/ui/snapshot_tests.cpp)
        target_link_libraries(gui_snapshot_tests PRIVATE xray_ui xray_code)
        add_dependencies(gui_snapshot_tests code-xray)
        add_test(NAME gui_snapshot COMMAND gui_snapshot_tests "${CMAKE_CURRENT_BINARY_DIR}/gui-snapshots")
        set_tests_properties(gui_snapshot PROPERTIES TIMEOUT 30)

        add_executable(gui_syntax_tests tests/ui/syntax_tests.cpp)
        target_link_libraries(gui_syntax_tests PRIVATE xray_ui)
        add_dependencies(gui_syntax_tests code-xray)
        add_test(NAME gui_syntax COMMAND gui_syntax_tests "${CMAKE_CURRENT_BINARY_DIR}/gui-syntax")
        set_tests_properties(gui_syntax PROPERTIES TIMEOUT 30)

        add_executable(gui_popup_tests tests/ui/popup_tests.cpp)
        target_link_libraries(gui_popup_tests PRIVATE xray_ui xray_code)
        add_dependencies(gui_popup_tests code-xray)
        add_test(NAME gui_popup COMMAND gui_popup_tests "${CMAKE_CURRENT_BINARY_DIR}/gui-popups")
        set_tests_properties(gui_popup PROPERTIES TIMEOUT 30)

        add_executable(parser_tests tests/code/parser_tests.cpp)
        target_link_libraries(parser_tests PRIVATE xray_code)
        add_test(NAME parser COMMAND parser_tests)

        add_executable(load_tests tests/code/load_tests.cpp)
        target_link_libraries(load_tests PRIVATE xray_code)
        add_test(NAME source_load COMMAND load_tests)

        add_executable(dependency_smoke tests/dependencies.cpp)
        target_link_libraries(dependency_smoke PRIVATE xray_common xray_cpp_grammar
            unofficial::tree-sitter::tree-sitter libgit2::libgit2package
            nlohmann_json::nlohmann_json pantor::inja)
        add_test(NAME dependencies COMMAND dependency_smoke)
        add_test(NAME gui_startup COMMAND code-xray --smoke-test)
        add_test(NAME gui_demo_startup COMMAND code-xray --demo --smoke-test)
        set_tests_properties(gui_startup gui_demo_startup PROPERTIES TIMEOUT 15)

        if(WIN32)
            add_executable(windows_uia_filter_tests tests/ui/windows_uia_filter_tests.cpp)
            target_link_libraries(windows_uia_filter_tests PRIVATE xray_common Qt6::Core)
            add_dependencies(windows_uia_filter_tests code-xray)
            add_test(NAME windows_uia_filter COMMAND windows_uia_filter_tests)
            add_test(NAME gui_startup_no_uia COMMAND code-xray --no-uia --smoke-test)
            set_tests_properties(windows_uia_filter PROPERTIES TIMEOUT 30)
            set_tests_properties(gui_startup_no_uia PROPERTIES TIMEOUT 15)
        endif()
    endif()
endif()
