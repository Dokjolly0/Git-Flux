cmake_minimum_required(VERSION 3.25)

project(GitFlux
    VERSION 0.1.0
    DESCRIPTION "Git Flux — Modern local-first desktop Git client"
    LANGUAGES CXX
)

option(GF_BUILD_DESKTOP "Abilita i target che dipendono da Qt 6 (adapter Qt e UI desktop)" OFF)
option(GF_BUILD_TESTING "Compila unit e integration test" ON)
option(GF_BUILD_BENCHMARKS "Compila le suite di benchmark di Fase 0" ON)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Opzioni di compilazione comuni per un target Git Flux
function(gf_apply_compiler_warnings target_name)
    target_compile_options(${target_name} PRIVATE
        $<$<CXX_COMPILER_ID:MSVC>:/utf-8 /W4 /permissive- /EHsc>
        $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall -Wextra -Wpedantic>
    )
    target_compile_definitions(${target_name} PRIVATE
        $<$<PLATFORM_ID:Windows>:WIN32_LEAN_AND_MEAN NOMINMAX UNICODE _UNICODE>
    )
endfunction()

# ==============================================================================
# 1. CORE (ZERO Qt — ADR-002)
# ==============================================================================
add_library(gf_domain INTERFACE)
target_include_directories(gf_domain INTERFACE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
)

add_library(gf_application STATIC
    src/core/modules/git/bl/porcelain_status_parser.cpp
    src/core/modules/git/bl/operation_coordinator.cpp
)
target_include_directories(gf_application PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
)
target_link_libraries(gf_application PUBLIC gf_domain)
gf_apply_compiler_warnings(gf_application)

# ==============================================================================
# 2. INFRASTRUCTURE — Git CLI Adapter (Native OS vs Qt 6)
# ==============================================================================
add_library(gf_git_cli_native STATIC
    src/infrastructure/git/git_discovery.cpp
    src/infrastructure/git/native/native_git_executor.cpp
    src/infrastructure/git/native/native_git_batch_session.cpp
)
target_include_directories(gf_git_cli_native PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
)
target_link_libraries(gf_git_cli_native PUBLIC gf_application)
gf_apply_compiler_warnings(gf_git_cli_native)

if(GF_BUILD_DESKTOP)
    find_package(Qt6 REQUIRED COMPONENTS Core)

    add_library(gf_git_cli_qt STATIC
        src/infrastructure/git/qt/qt_git_executor.cpp
        src/infrastructure/git/qt/qt_git_batch_session.cpp
    )
    target_include_directories(gf_git_cli_qt PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/src"
    )
    target_link_libraries(gf_git_cli_qt PUBLIC
        gf_application
        Qt6::Core
    )
    gf_apply_compiler_warnings(gf_git_cli_qt)
endif()

# ==============================================================================
# 3. TOOLS — Deterministic Synthetic Git Fixture Generator
# ==============================================================================
add_library(gf_fixture_gen_lib STATIC
    tools/fixture_gen/fixture_generator.cpp
)
target_include_directories(gf_fixture_gen_lib PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/tools/fixture_gen"
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
)
target_link_libraries(gf_fixture_gen_lib PUBLIC gf_git_cli_native)
gf_apply_compiler_warnings(gf_fixture_gen_lib)

add_executable(gf_fixture_gen
    tools/fixture_gen/main.cpp
)
target_link_libraries(gf_fixture_gen PRIVATE gf_fixture_gen_lib)
gf_apply_compiler_warnings(gf_fixture_gen)

# Helper per copiare automaticamente le DLL di runtime (es. Qt6Core.dll su Windows)
function(gf_deploy_runtime_dlls target_name)
    if(WIN32 AND GF_BUILD_DESKTOP)
        add_custom_command(TARGET ${target_name} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                $<TARGET_RUNTIME_DLLS:${target_name}>
                $<TARGET_FILE_DIR:${target_name}>
            COMMAND_EXPAND_LISTS
        )
    endif()
endfunction()

# ==============================================================================
# 4. TESTS & BENCHMARKS (Catch2 v3)
# ==============================================================================
if(GF_BUILD_TESTING OR GF_BUILD_BENCHMARKS)
    include(CTest)
    find_package(Catch2 3 CONFIG QUIET)
    if(NOT Catch2_FOUND)
        include(FetchContent)
        message(STATUS "Catch2 v3 non trovato nel sistema: recupero tramite FetchContent...")
        FetchContent_Declare(
            Catch2
            GIT_REPOSITORY https://github.com/catchorg/Catch2.git
            GIT_TAG        v3.7.1
            GIT_SHALLOW    TRUE
        )
        FetchContent_MakeAvailable(Catch2)
    endif()
endif()

if(GF_BUILD_TESTING)
    # Unit test del Core (sempre ZERO Qt anche se GF_BUILD_DESKTOP=ON)
    add_executable(gf_unit_tests
        tests/unit/core_git_unit_tests.cpp
    )
    target_link_libraries(gf_unit_tests PRIVATE
        gf_git_cli_native
        Catch2::Catch2WithMain
    )
    gf_apply_compiler_warnings(gf_unit_tests)
    add_test(NAME gf_unit_tests COMMAND gf_unit_tests)

    # Integration test Suite A (Git reale su repository temporanei)
    add_executable(gf_integration_tests
        tests/integration/suite_a_git_integration_tests.cpp
    )
    target_link_libraries(gf_integration_tests PRIVATE
        gf_git_cli_native
        gf_fixture_gen_lib
        Catch2::Catch2WithMain
    )
    if(GF_BUILD_DESKTOP)
        target_link_libraries(gf_integration_tests PRIVATE gf_git_cli_qt)
        target_compile_definitions(gf_integration_tests PRIVATE GF_HAS_QT_ADAPTER=1)
        gf_deploy_runtime_dlls(gf_integration_tests)
    endif()
    gf_apply_compiler_warnings(gf_integration_tests)
    add_test(NAME gf_integration_tests COMMAND gf_integration_tests)
endif()

if(GF_BUILD_BENCHMARKS)
    add_executable(gf_bench_suite_a
        tests/performance/suite_a_git_bench.cpp
    )
    target_link_libraries(gf_bench_suite_a PRIVATE
        gf_git_cli_native
        gf_fixture_gen_lib
        Catch2::Catch2WithMain
    )
    if(GF_BUILD_DESKTOP)
        target_link_libraries(gf_bench_suite_a PRIVATE gf_git_cli_qt)
        target_compile_definitions(gf_bench_suite_a PRIVATE GF_HAS_QT_ADAPTER=1)
        gf_deploy_runtime_dlls(gf_bench_suite_a)
    endif()
    gf_apply_compiler_warnings(gf_bench_suite_a)
endif()
