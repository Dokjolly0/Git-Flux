#include <catch2/catch_test_macros.hpp>

#include "core/modules/terminal/bl/git_catalog_service.hpp"
#include "desktop/viewmodels/terminal_session_viewmodel.hpp"
#include "infrastructure/git/git_discovery.hpp"
#include "infrastructure/git/native/native_git_executor.hpp"
#include "infrastructure/terminal/native_terminal_process_runner.hpp"

#include <QDir>
#include <QGuiApplication>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <filesystem>

using namespace gf::desktop;
using namespace gf::terminal::bl;
using namespace gf::terminal::bo;

namespace
{
    void ensureTerminalTestQtApp()
    {
        if (!QCoreApplication::instance())
        {
            static int s_argc = 1;
            static char s_appName[] = "gf_viewmodels_tests";
            static char* s_argv[] = {s_appName, nullptr};
            if (qEnvironmentVariableIsEmpty("QT_PLUGIN_PATH") &&
                std::filesystem::exists("C:/Qt/6.6.2/msvc2019_64/plugins"))
            {
                qputenv("QT_PLUGIN_PATH", "C:/Qt/6.6.2/msvc2019_64/plugins");
            }
            qputenv("QT_QPA_PLATFORM", "offscreen");
            new QGuiApplication(s_argc, s_argv);
        }
    }
} // namespace

TEST_CASE("TerminalSessionViewModel manages multi-tab per-repo state, cd, completion chaining and git execution", "[viewmodels][terminal]")
{
    ensureTerminalTestQtApp();
    const auto discovery = gf::infra::git::GitDiscovery::discover();
    REQUIRE(discovery.has_value());
    const auto gitExe = discovery->directBinaryPath.empty() ? discovery->discoveredInPath : discovery->directBinaryPath;

    gf::infra::git::NativeGitCommandExecutor gitExec(gitExe);
    gf::infra::terminal::NativeTerminalProcessRunner runner(gitExe);
    GitCatalogService catalog(&gitExec);
    catalog.discoverAllCommands();

    QTemporaryDir tempRoot;
    REQUIRE(tempRoot.isValid());
    const QString repoA = tempRoot.filePath(QStringLiteral("repoA"));
    const QString repoASub = repoA + QStringLiteral("/src");
    const QString repoB = tempRoot.filePath(QStringLiteral("repoB"));
    REQUIRE(QDir().mkpath(repoASub));
    REQUIRE(QDir().mkpath(repoB));

    TerminalSessionViewModel vm(&runner, &catalog, &gitExec);
    vm.setActiveRepository(QStringLiteral("id-a"), QStringLiteral("RepoA"), repoA, QStringLiteral("main"));
    vm.setIsVisible(true);

    SECTION("Auto-creates initial tab rooted at repo root and supports multiple tabs per repo")
    {
        REQUIRE(vm.tabs().size() == 1);
        REQUIRE(vm.activeTabIndex() == 0);
        REQUIRE(QDir::cleanPath(vm.activeWorkingDir()).compare(QDir::cleanPath(repoA), Qt::CaseInsensitive) == 0);

        const int secondIdx = vm.createTab();
        REQUIRE(secondIdx == 1);
        REQUIRE(vm.tabs().size() == 2);
        REQUIRE(vm.activeTabIndex() == 1);

        // Switching to RepoB gives RepoB its own independent tab rooted at repoB
        vm.setActiveRepository(QStringLiteral("id-b"), QStringLiteral("RepoB"), repoB, QStringLiteral("develop"));
        REQUIRE(vm.tabs().size() == 1);
        REQUIRE(QDir::cleanPath(vm.activeWorkingDir()).compare(QDir::cleanPath(repoB), Qt::CaseInsensitive) == 0);

        // Switching back to RepoA restores its 2 tabs
        vm.setActiveRepository(QStringLiteral("id-a"), QStringLiteral("RepoA"), repoA, QStringLiteral("main"));
        REQUIRE(vm.tabs().size() == 2);
        REQUIRE(vm.activeTabIndex() == 1);

        vm.closeTab(1);
        REQUIRE(vm.tabs().size() == 1);
        REQUIRE(vm.activeTabIndex() == 0);
    }

    SECTION("Built-in cd persists working directory per tab and updates relative path")
    {
        vm.executeCommand(QStringLiteral("cd src"));
        REQUIRE(vm.activeRelativeDir() == QStringLiteral("/src"));

        vm.executeCommand(QStringLiteral("cd .."));
        REQUIRE(vm.activeRelativeDir() == QStringLiteral("/"));
    }

    SECTION("Live git completion filters progressively and chains on acceptCompletion")
    {
        vm.updateCompletions(QStringLiteral("git c"), 5);
        REQUIRE(vm.hasCompletions());
        REQUIRE(vm.completionCount() >= 10);
        REQUIRE_FALSE(vm.ghostText().isEmpty());

        vm.updateCompletions(QStringLiteral("git checko"), 10);
        REQUIRE(vm.completionCount() >= 1);
        const QString firstDisplay = vm.completionModel()->data(
            vm.completionModel()->index(0, 0), TerminalCompletionListModel::DisplayTextRole).toString();
        REQUIRE(firstDisplay == QStringLiteral("checkout"));

        const QVariantMap accepted = vm.acceptCompletion(0, QStringLiteral("git checko"), 10);
        REQUIRE(accepted.value(QStringLiteral("accepted")).toBool());
        REQUIRE(accepted.value(QStringLiteral("text")).toString() == QStringLiteral("git checkout "));
        REQUIRE(accepted.value(QStringLiteral("cursorPos")).toInt() == 13);
    }

    SECTION("Executing git command streams output and emits gitCommandFinished")
    {
        QSignalSpy finishedSpy(&vm, &TerminalSessionViewModel::gitCommandFinished);
        vm.executeCommand(QStringLiteral("git --version"));
        REQUIRE(vm.waitForIdle(5000));
        REQUIRE(finishedSpy.count() == 1);
        REQUIRE(finishedSpy.at(0).at(1).toInt() == 0);
        REQUIRE(vm.copyActiveOutput().contains(QStringLiteral("git version")));
    }

    SECTION("updateActiveBranch does not emit activeTabChanged and only notifies when branch changes")
    {
        QSignalSpy tabSpy(&vm, &TerminalSessionViewModel::activeTabChanged);
        QSignalSpy branchSpy(&vm, &TerminalSessionViewModel::activeBranchChanged);

        // Same branch ("main") must not emit either signal (prevents 1.5s scroll snap-back!)
        vm.updateActiveBranch(repoA, QStringLiteral("main"));
        REQUIRE(tabSpy.count() == 0);
        REQUIRE(branchSpy.count() == 0);

        // Different branch emits only activeBranchChanged, never activeTabChanged
        vm.updateActiveBranch(repoA, QStringLiteral("develop"));
        REQUIRE(tabSpy.count() == 0);
        REQUIRE(branchSpy.count() == 1);
        REQUIRE(vm.activeBranch() == QStringLiteral("develop"));
    }

    SECTION("shouldPaginateCommand detects paged Git commands and respects --no-pager")
    {
        REQUIRE(TerminalSessionViewModel::shouldPaginateCommand("git log"));
        REQUIRE(TerminalSessionViewModel::shouldPaginateCommand("git log 7852ebbd8d382e076b2c8c5c96ff2153771204b5"));
        REQUIRE(TerminalSessionViewModel::shouldPaginateCommand("git diff"));
        REQUIRE(TerminalSessionViewModel::shouldPaginateCommand("git show HEAD"));
        REQUIRE(TerminalSessionViewModel::shouldPaginateCommand("git -p status"));
        REQUIRE_FALSE(TerminalSessionViewModel::shouldPaginateCommand("git --no-pager log"));
        REQUIRE_FALSE(TerminalSessionViewModel::shouldPaginateCommand("git -P log"));
        REQUIRE_FALSE(TerminalSessionViewModel::shouldPaginateCommand("git status"));
    }

    SECTION("Integrated less-FRX pager keeps short git log inline and opens long git log at top (newest commit first)")
    {
        const QString workspaceRepo = QString::fromStdString(std::filesystem::current_path().string());
        if (QDir(workspaceRepo + QStringLiteral("/.git")).exists())
        {
            vm.setActiveRepository(QStringLiteral("id-self"), QStringLiteral("GitFlux"), workspaceRepo, QStringLiteral("develop"));
            vm.waitForIdle(5000);

            // Short output (2 lines) stays inline without activating pager
            vm.executeCommand(QStringLiteral("git log -n 2 --oneline"));
            REQUIRE(vm.waitForIdle(5000));
            REQUIRE_FALSE(vm.pagerActive());

            // Long output (> 28 lines) opens integrated pager starting at line 0 (newest commit at top!)
            vm.executeCommand(QStringLiteral("git log -n 40"));
            REQUIRE(vm.waitForIdle(5000));
            REQUIRE(vm.pagerActive());
            REQUIRE(vm.pagerLineCount() > 28);
            const QString firstLine = vm.activePagerModel()->data(
                vm.activePagerModel()->index(0, 0), TerminalOutputListModel::PlainTextRole).toString();
            REQUIRE(firstLine.startsWith(QStringLiteral("commit ")));

            vm.closePager();
            REQUIRE_FALSE(vm.pagerActive());
            REQUIRE(vm.hasLastPagerOutput());
        }
    }
}
