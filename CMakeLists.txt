cmake_minimum_required(VERSION 3.25)

project(GitFlux
    VERSION 0.1.0
    DESCRIPTION "Git Flux — Modern local-first desktop Git client"
    LANGUAGES C CXX
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
# 1. CORE (ZERO Qt — ADR-002)
# ==============================================================================
add_library(gf_domain INTERFACE)
target_include_directories(gf_domain INTERFACE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
)

add_library(gf_application STATIC
    src/core/modules/git/bl/porcelain_status_parser.cpp
    src/core/modules/git/bl/unified_diff_parser.cpp
    src/core/modules/git/bl/operation_coordinator.cpp
    src/core/modules/git/bl/git_working_tree_service.cpp
    src/core/modules/git/bl/git_staging_service.cpp
    src/core/modules/git/bl/git_diff_service.cpp
    src/core/modules/git/bl/git_branch_service.cpp
    src/core/modules/git/bl/commit_log_parser.cpp
    src/core/modules/git/bl/git_history_service.cpp
    src/core/modules/git/bl/git_remote_merge_service.cpp
    src/core/modules/repositories/bl/repository_service.cpp
    src/core/modules/groups/bl/group_service.cpp
    src/core/modules/workspaces/bl/workspace_service.cpp
)
target_include_directories(gf_application PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
)
target_link_libraries(gf_application PUBLIC gf_domain)
gf_apply_compiler_warnings(gf_application)

# ==============================================================================
# 2. INFRASTRUCTURE — Persistence (SQLite 3 — Zero-Qt)
# ==============================================================================
add_library(gf_sqlite3 STATIC
    third_party/sqlite/sqlite3.c
)
target_include_directories(gf_sqlite3 PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/third_party/sqlite"
)
target_compile_definitions(gf_sqlite3 PRIVATE
    SQLITE_ENABLE_JSON1
    SQLITE_ENABLE_RTREE
    SQLITE_THREADSAFE=1
)
target_compile_options(gf_sqlite3 PRIVATE
    $<$<C_COMPILER_ID:MSVC>:/w>
    $<$<NOT:$<C_COMPILER_ID:MSVC>>:-w>
)
if(NOT MSVC)
    target_link_libraries(gf_sqlite3 PRIVATE ${CMAKE_DL_LIBS} pthread)
endif()

add_library(gf_persistence_sqlite STATIC
    src/infrastructure/persistence/sqlite/connection/sqlite_connection.cpp
    src/infrastructure/persistence/sqlite/connection/sqlite_statement.cpp
    src/infrastructure/persistence/sqlite/migrations/schema_migrator.cpp
    src/infrastructure/persistence/repositories/sqlite_repository_store.cpp
    src/infrastructure/persistence/groups/sqlite_group_store.cpp
    src/infrastructure/persistence/workspaces/sqlite_workspace_store.cpp
)
target_include_directories(gf_persistence_sqlite PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
)
target_link_libraries(gf_persistence_sqlite PUBLIC
    gf_domain
    gf_sqlite3
)
gf_apply_compiler_warnings(gf_persistence_sqlite)

# ==============================================================================
# 3. INFRASTRUCTURE — Git CLI Adapter (Native OS vs Qt 6)
# ==============================================================================
add_library(gf_git_cli_native STATIC
    src/infrastructure/git/git_discovery.cpp
    src/infrastructure/git/native/native_git_executor.cpp
    src/infrastructure/git/native/native_git_batch_session.cpp
    src/infrastructure/git/native/native_repository_git_ops.cpp
)
target_include_directories(gf_git_cli_native PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
)
target_link_libraries(gf_git_cli_native PUBLIC gf_application)
gf_apply_compiler_warnings(gf_git_cli_native)

if(GF_BUILD_DESKTOP)
    set(CMAKE_AUTOMOC ON)
    set(CMAKE_AUTORCC ON)

    if(UNIX AND NOT APPLE)
        if(EXISTS "$ENV{HOME}/.local/qt6-declarative/usr/lib/x86_64-linux-gnu/cmake/Qt6Quick")
            list(PREPEND CMAKE_PREFIX_PATH
                "$ENV{HOME}/.local/qt6-declarative/usr"
                "$ENV{HOME}/.local/qt6-declarative/usr/lib/x86_64-linux-gnu/cmake"
            )
            set(Qt6Quick_DIR "$ENV{HOME}/.local/qt6-declarative/usr/lib/x86_64-linux-gnu/cmake/Qt6Quick" CACHE PATH "Path to Qt6Quick")
            set(Qt6Qml_DIR "$ENV{HOME}/.local/qt6-declarative/usr/lib/x86_64-linux-gnu/cmake/Qt6Qml" CACHE PATH "Path to Qt6Qml")
            set(Qt6QmlTools_DIR "$ENV{HOME}/.local/qt6-declarative/usr/lib/x86_64-linux-gnu/cmake/Qt6QmlTools" CACHE PATH "Path to Qt6QmlTools")
            set(Qt6QuickControls2_DIR "$ENV{HOME}/.local/qt6-declarative/usr/lib/x86_64-linux-gnu/cmake/Qt6QuickControls2" CACHE PATH "Path to Qt6QuickControls2")
        endif()
    endif()

    find_package(Qt6 REQUIRED COMPONENTS Core Gui Quick Qml QuickControls2)
    if(GF_BUILD_TESTING)
        find_package(Qt6 REQUIRED COMPONENTS Test)
    endif()
    qt_policy(SET QTP0001 NEW)
    qt_policy(SET QTP0004 OLD)

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

    # MVVM ViewModels e Desktop Composition Root
    add_library(gf_desktop_viewmodels STATIC
        src/desktop/viewmodels/repository_list_model.cpp
        src/desktop/viewmodels/group_list_model.cpp
        src/desktop/viewmodels/workspace_list_model.cpp
        src/desktop/viewmodels/working_tree_file_list_model.cpp
        src/desktop/viewmodels/diff_line_list_model.cpp
        src/desktop/viewmodels/diff_viewmodel.cpp
        src/desktop/viewmodels/commit_box_viewmodel.cpp
        src/desktop/viewmodels/working_tree_viewmodel.cpp
        src/desktop/viewmodels/branch_list_model.cpp
        src/desktop/viewmodels/commit_history_list_model.cpp
        src/desktop/viewmodels/branch_manager_viewmodel.cpp
        src/desktop/viewmodels/workspace_session_viewmodel.cpp
        src/desktop/viewmodels/home_hub_viewmodel.cpp
        src/desktop/viewmodels/quick_open_viewmodel.cpp
        src/desktop/bootstrap/app_bootstrap.cpp
    )
    target_include_directories(gf_desktop_viewmodels PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/src"
    )
    target_link_libraries(gf_desktop_viewmodels PUBLIC
        gf_application
        gf_persistence_sqlite
        gf_git_cli_native
        Qt6::Core
        Qt6::Gui
        Qt6::Quick
        Qt6::Qml
        Qt6::QuickControls2
    )
    gf_apply_compiler_warnings(gf_desktop_viewmodels)

    # Applicazione Desktop Eseguibile (Qt Quick 6)
    qt_add_executable(git_flux
        src/desktop/main.cpp
    )
    qt_add_qml_module(git_flux
        URI "GitFlux.Desktop"
        VERSION 1.0
        RESOURCE_PREFIX "/qt/qml"
        QML_FILES
            src/desktop/ui/qml/main.qml
            src/desktop/ui/qml/themes/FluxTheme.qml
            src/desktop/ui/qml/views/HomeHubView.qml
            src/desktop/ui/qml/views/QuickOpenModal.qml
            src/desktop/ui/qml/views/WorkspaceSessionView.qml
            src/desktop/ui/qml/views/WorkingTreePanel.qml
            src/desktop/ui/qml/views/CommitBox.qml
            src/desktop/ui/qml/views/DiffViewerPanel.qml
            src/desktop/ui/qml/views/BranchSwitcherDropdown.qml
            src/desktop/ui/qml/views/BranchManagerModal.qml
            src/desktop/ui/qml/views/CommitHistoryPanel.qml
    )
    target_include_directories(git_flux PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src"
    )
    target_link_libraries(git_flux PRIVATE
        gf_desktop_viewmodels
        Qt6::Core
        Qt6::Gui
        Qt6::Quick
        Qt6::Qml
        Qt6::QuickControls2
    )
    gf_apply_compiler_warnings(git_flux)
    gf_deploy_runtime_dlls(git_flux)
endif()

# ==============================================================================
# 4. TOOLS — Deterministic Synthetic Git Fixture Generator
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



# ==============================================================================
# 5. TESTS & BENCHMARKS (Catch2 v3)
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
        tests/unit/domain_entities_unit_tests.cpp
        tests/unit/services_unit_tests.cpp
        tests/unit/working_tree_unit_tests.cpp
        tests/unit/staging_unit_tests.cpp
        tests/unit/diff_unit_tests.cpp
        tests/unit/branch_unit_tests.cpp
        tests/unit/history_unit_tests.cpp
        tests/unit/remote_merge_unit_tests.cpp
    )
    target_link_libraries(gf_unit_tests PRIVATE
        gf_git_cli_native
        gf_persistence_sqlite
        Catch2::Catch2WithMain
    )
    gf_apply_compiler_warnings(gf_unit_tests)
    add_test(NAME gf_unit_tests COMMAND gf_unit_tests)

    # Integration test (Git reale su repo temporanei + SQLite persistence)
    add_executable(gf_integration_tests
        tests/integration/suite_a_git_integration_tests.cpp
        tests/integration/sqlite_stores_integration_tests.cpp
        tests/integration/working_tree_integration_tests.cpp
        tests/integration/staging_integration_tests.cpp
        tests/integration/diff_integration_tests.cpp
        tests/integration/branch_integration_tests.cpp
        tests/integration/history_integration_tests.cpp
        tests/integration/remote_merge_integration_tests.cpp
    )
    target_link_libraries(gf_integration_tests PRIVATE
        gf_git_cli_native
        gf_persistence_sqlite
        gf_fixture_gen_lib
        Catch2::Catch2WithMain
    )
    if(GF_BUILD_DESKTOP)
        target_link_libraries(gf_integration_tests PRIVATE gf_git_cli_qt)
        target_compile_definitions(gf_integration_tests PRIVATE GF_HAS_QT_ADAPTER=1)
        gf_deploy_runtime_dlls(gf_integration_tests)

        # Unit test per MVVM (ViewModels & Models con Catch2 + QSignalSpy)
        add_executable(gf_viewmodels_tests
            tests/unit/viewmodels_unit_tests.cpp
        )
        target_link_libraries(gf_viewmodels_tests PRIVATE
            gf_desktop_viewmodels
            Qt6::Test
            Catch2::Catch2WithMain
        )
        gf_apply_compiler_warnings(gf_viewmodels_tests)
        gf_deploy_runtime_dlls(gf_viewmodels_tests)
        add_test(NAME gf_viewmodels_tests COMMAND gf_viewmodels_tests)
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
