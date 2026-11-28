#include <catch2/catch_test_macros.hpp>

#include "core/modules/terminal/bl/git_catalog_service.hpp"
#include "desktop/viewmodels/terminal_session_viewmodel.hpp"
#include "infrastructure/git/git_discovery.hpp"
#include "infrastructure/git/native/native_git_executor.hpp"
#include "infrastructure/terminal/native_terminal_process_runner.hpp"

#include <QClipboard>
#include <QDir>
#include <QFile>
#include <QGuiApplication>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
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
            QQuickStyle::setStyle(QStringLiteral("Basic"));
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

    SECTION("Prompt input draft and cursor position are isolated and persisted per tab and per repository")
    {
        const int tab1Id = vm.activeTabId();
        REQUIRE(tab1Id > 0);
        REQUIRE(vm.activeInputDraft().isEmpty());
        REQUIRE(vm.activeInputCursorPos() == 0);

        // Type partial command in Terminal 1 with cursor mid-string
        vm.setActiveInputDraft(QStringLiteral("git commit -m \"wip"), 10);
        REQUIRE(vm.activeInputDraft() == QStringLiteral("git commit -m \"wip"));
        REQUIRE(vm.activeInputCursorPos() == 10);

        // Creating Terminal 2 gives it an empty draft without losing Terminal 1's draft
        const int tab2Idx = vm.createTab();
        REQUIRE(tab2Idx == 1);
        const int tab2Id = vm.activeTabId();
        REQUIRE(tab2Id != tab1Id);
        REQUIRE(vm.activeInputDraft().isEmpty());
        REQUIRE(vm.activeInputCursorPos() == 0);

        // Type a different draft in Terminal 2
        vm.setActiveInputDraft(QStringLiteral("cd src && ls"), 6);
        REQUIRE(vm.activeInputDraft() == QStringLiteral("cd src && ls"));
        REQUIRE(vm.activeInputCursorPos() == 6);

        // Switching back to Terminal 1 restores Terminal 1's draft and cursor
        vm.setActiveTabIndex(0);
        REQUIRE(vm.activeTabId() == tab1Id);
        REQUIRE(vm.activeInputDraft() == QStringLiteral("git commit -m \"wip"));
        REQUIRE(vm.activeInputCursorPos() == 10);

        // Switching to RepoB gives RepoB its own independent draft
        vm.setActiveRepository(QStringLiteral("id-b"), QStringLiteral("RepoB"), repoB, QStringLiteral("develop"));
        REQUIRE(vm.activeTabId() != tab1Id);
        REQUIRE(vm.activeTabId() != tab2Id);
        REQUIRE(vm.activeInputDraft().isEmpty());
        vm.setActiveInputDraft(QStringLiteral("git status --short"));
        REQUIRE(vm.activeInputDraft() == QStringLiteral("git status --short"));
        REQUIRE(vm.activeInputCursorPos() == 18);

        // Switching back to RepoA restores Terminal 1's draft
        vm.setActiveRepository(QStringLiteral("id-a"), QStringLiteral("RepoA"), repoA, QStringLiteral("main"));
        REQUIRE(vm.activeTabId() == tab1Id);
        REQUIRE(vm.activeInputDraft() == QStringLiteral("git commit -m \"wip"));
        REQUIRE(vm.activeInputCursorPos() == 10);

        // Executing a command in Terminal 1 clears Terminal 1's draft without affecting Terminal 2
        vm.executeCommand(QStringLiteral("pwd"));
        REQUIRE(vm.activeInputDraft().isEmpty());
        REQUIRE(vm.activeInputCursorPos() == 0);

        // History navigation updates the active tab's draft
        REQUIRE(vm.historyPrevious(QStringLiteral("draft_before_hist")) == QStringLiteral("pwd"));
        REQUIRE(vm.activeInputDraft() == QStringLiteral("pwd"));
        REQUIRE(vm.historyNext() == QStringLiteral("draft_before_hist"));
        REQUIRE(vm.activeInputDraft() == QStringLiteral("draft_before_hist"));

        // Switching to Terminal 2 still has Terminal 2's untouched draft
        vm.setActiveTabIndex(1);
        REQUIRE(vm.activeTabId() == tab2Id);
        REQUIRE(vm.activeInputDraft() == QStringLiteral("cd src && ls"));
        REQUIRE(vm.activeInputCursorPos() == 6);

        // Closing Terminal 2 restores Terminal 1's draft
        vm.closeTab(1);
        REQUIRE(vm.activeTabId() == tab1Id);
        REQUIRE(vm.activeInputDraft() == QStringLiteral("draft_before_hist"));

        // Closing all terminals resets activeTabId and activeInputDraft
        vm.closeAllTerminals();
        REQUIRE(vm.activeTabId() == -1);
        REQUIRE(vm.activeInputDraft().isEmpty());
        REQUIRE(vm.activeInputCursorPos() == 0);
    }

    SECTION("Built-in cd persists working directory per tab and updates relative path")
    {
        vm.executeCommand(QStringLiteral("cd src"));
        REQUIRE(vm.activeRelativeDir() == QStringLiteral("/src"));

        vm.executeCommand(QStringLiteral("cd .."));
        REQUIRE(vm.activeRelativeDir() == QStringLiteral("/"));

        // Quoted directory name containing shell operator characters stays built-in cd
        const QString specialDir = repoA + QStringLiteral("/dir && name");
        REQUIRE(QDir().mkpath(specialDir));
        vm.executeCommand(QStringLiteral("cd \"dir && name\""));
        REQUIRE(vm.activeRelativeDir() == QStringLiteral("/dir && name"));

        vm.executeCommand(QStringLiteral("cd -"));
        REQUIRE(vm.activeRelativeDir() == QStringLiteral("/"));
    }

    SECTION("isBuiltinCdCommand distinguishes pure cd from shell-chained cd commands")
    {
        // Pure cd invocations
        REQUIRE(TerminalSessionViewModel::isBuiltinCdCommand("cd"));
        REQUIRE(TerminalSessionViewModel::isBuiltinCdCommand("CD"));
        REQUIRE(TerminalSessionViewModel::isBuiltinCdCommand("  cd   src  "));
        REQUIRE(TerminalSessionViewModel::isBuiltinCdCommand("cd\tsrc"));
        REQUIRE(TerminalSessionViewModel::isBuiltinCdCommand("cd scripts/"));
        REQUIRE(TerminalSessionViewModel::isBuiltinCdCommand("cd scripts\\windows"));
        REQUIRE(TerminalSessionViewModel::isBuiltinCdCommand("cd .."));
        REQUIRE(TerminalSessionViewModel::isBuiltinCdCommand("cd -"));
        REQUIRE(TerminalSessionViewModel::isBuiltinCdCommand("cd ~"));
        REQUIRE(TerminalSessionViewModel::isBuiltinCdCommand("cd \"dir && name\""));
        REQUIRE(TerminalSessionViewModel::isBuiltinCdCommand("cd 'dir; name'"));
        REQUIRE(TerminalSessionViewModel::isBuiltinCdCommand("cd \"scripts\\\""));

        // Chained / shell-operator commands must be delegated to the shell
        REQUIRE_FALSE(TerminalSessionViewModel::isBuiltinCdCommand("cd scripts/ && ls"));
        REQUIRE_FALSE(TerminalSessionViewModel::isBuiltinCdCommand("cd src && git status"));
        REQUIRE_FALSE(TerminalSessionViewModel::isBuiltinCdCommand("cd scripts; ls"));
        REQUIRE_FALSE(TerminalSessionViewModel::isBuiltinCdCommand("cd scripts;ls"));
        REQUIRE_FALSE(TerminalSessionViewModel::isBuiltinCdCommand("cd scripts || echo err"));
        REQUIRE_FALSE(TerminalSessionViewModel::isBuiltinCdCommand("cd scripts | cat"));
        REQUIRE_FALSE(TerminalSessionViewModel::isBuiltinCdCommand("cd \"dir name\" && ls"));
        REQUIRE_FALSE(TerminalSessionViewModel::isBuiltinCdCommand("cd \"scripts\\\" && ls"));
        REQUIRE_FALSE(TerminalSessionViewModel::isBuiltinCdCommand("cd scripts\\&&ls"));
        REQUIRE_FALSE(TerminalSessionViewModel::isBuiltinCdCommand("cd src & dir"));
        REQUIRE_FALSE(TerminalSessionViewModel::isBuiltinCdCommand("cd src > out.txt"));
        REQUIRE_FALSE(TerminalSessionViewModel::isBuiltinCdCommand("cd src\nls"));
        REQUIRE_FALSE(TerminalSessionViewModel::isBuiltinCdCommand("cd \"unclosed && ls"));
        REQUIRE_FALSE(TerminalSessionViewModel::isBuiltinCdCommand("cd; ls"));
        REQUIRE_FALSE(TerminalSessionViewModel::isBuiltinCdCommand("cd&&ls"));
        REQUIRE_FALSE(TerminalSessionViewModel::isBuiltinCdCommand("git status"));
    }

    SECTION("Chained cd commands (&&, ;, ||, |) are delegated to the shell instead of failing as built-in cd")
    {
        auto lastCommandExitCode = [&]() -> int
        {
            const auto* model = vm.activeOutputModel();
            for (int r = model->count() - 1; r >= 0; --r)
            {
                const QModelIndex idx = model->index(r, 0);
                if (model->data(idx, TerminalOutputListModel::LineKindRole).toString() == QStringLiteral("command"))
                {
                    return model->data(idx, TerminalOutputListModel::ExitCodeRole).toInt();
                }
            }
            return -1;
        };

        auto collectStreamText = [&](const QString& kind) -> QString
        {
            const auto* model = vm.activeOutputModel();
            QStringList lines;
            for (int r = 0; r < model->count(); ++r)
            {
                const QModelIndex idx = model->index(r, 0);
                if (model->data(idx, TerminalOutputListModel::LineKindRole).toString() == kind)
                {
                    lines.append(model->data(idx, TerminalOutputListModel::PlainTextRole).toString());
                }
            }
            return lines.join(QLatin1Char('\n'));
        };

        // Create a marker file inside repoA/src so listing it proves cd succeeded inside the shell
        {
            QFile marker(repoASub + QStringLiteral("/marker_from_src.txt"));
            REQUIRE(marker.open(QIODevice::WriteOnly));
            marker.write("ok\n");
            marker.close();
        }

        // 1. `cd src/ && ls` (reproduces `cd scripts/ && ls`) AND persists working directory to `/src`
        vm.clearActiveOutput();
        vm.executeCommand(QStringLiteral("cd src/ && ls"));
        REQUIRE(vm.waitForIdle(5000));
        REQUIRE(lastCommandExitCode() == 0);
        REQUIRE_FALSE(collectStreamText(QStringLiteral("stderr")).contains(QStringLiteral("cd: directory non trovata")));
        REQUIRE(collectStreamText(QStringLiteral("stdout")).contains(QStringLiteral("marker_from_src.txt")));
        REQUIRE(vm.activeRelativeDir() == QStringLiteral("/src"));

        // `pwd` right after must reflect `/src`, not the previous repo root!
        vm.clearActiveOutput();
        vm.executeCommand(QStringLiteral("pwd"));
        REQUIRE(collectStreamText(QStringLiteral("stdout")).endsWith(QStringLiteral("src"), Qt::CaseInsensitive));

        // `cd -` returns to repo root `/`
        vm.executeCommand(QStringLiteral("cd -"));
        REQUIRE(vm.activeRelativeDir() == QStringLiteral("/"));

        // 2. `cd src; ls` also persists working directory to `/src`
        vm.clearActiveOutput();
        vm.executeCommand(QStringLiteral("cd src; ls"));
        REQUIRE(vm.waitForIdle(5000));
        REQUIRE(lastCommandExitCode() == 0);
        REQUIRE_FALSE(collectStreamText(QStringLiteral("stderr")).contains(QStringLiteral("cd: directory non trovata")));
        REQUIRE(collectStreamText(QStringLiteral("stdout")).contains(QStringLiteral("marker_from_src.txt")));
        REQUIRE(vm.activeRelativeDir() == QStringLiteral("/src"));

        vm.executeCommand(QStringLiteral("cd .."));
        REQUIRE(vm.activeRelativeDir() == QStringLiteral("/"));

        // 3. `cd missing_dir_xyz || echo fallback_ok` keeps working directory at `/`
        vm.clearActiveOutput();
        vm.executeCommand(QStringLiteral("cd missing_dir_xyz || echo fallback_ok"));
        REQUIRE(vm.waitForIdle(5000));
        REQUIRE(lastCommandExitCode() == 0);
        REQUIRE_FALSE(collectStreamText(QStringLiteral("stderr")).contains(QStringLiteral("cd: directory non trovata")));
        REQUIRE(collectStreamText(QStringLiteral("stdout")).contains(QStringLiteral("fallback_ok")));
        REQUIRE(vm.activeRelativeDir() == QStringLiteral("/"));

        // 4. `cd missing_dir_xyz && echo should_not_run` fails and keeps `/`
        vm.clearActiveOutput();
        vm.executeCommand(QStringLiteral("cd missing_dir_xyz && echo should_not_run"));
        REQUIRE(vm.waitForIdle(5000));
        REQUIRE(lastCommandExitCode() != 0);
        REQUIRE_FALSE(collectStreamText(QStringLiteral("stderr")).contains(QStringLiteral("cd: directory non trovata")));
        REQUIRE_FALSE(collectStreamText(QStringLiteral("stdout")).contains(QStringLiteral("should_not_run")));
        REQUIRE(vm.activeRelativeDir() == QStringLiteral("/"));

        // 5. `cd src && git --version` runs in shell, persists `/src`, AND emits gitCommandFinished
        vm.clearActiveOutput();
        QSignalSpy gitFinishedSpy(&vm, &TerminalSessionViewModel::gitCommandFinished);
        vm.executeCommand(QStringLiteral("cd src && git --version"));
        REQUIRE(vm.waitForIdle(5000));
        REQUIRE(lastCommandExitCode() == 0);
        REQUIRE(gitFinishedSpy.count() == 1);
        REQUIRE(gitFinishedSpy.at(0).at(1).toInt() == 0);
        REQUIRE(collectStreamText(QStringLiteral("stdout")).contains(QStringLiteral("git version")));
        REQUIRE(vm.activeRelativeDir() == QStringLiteral("/src"));

        // 6. Real workspace repo reproducer: `cd scripts/ && ls` then `pwd`
        const QString workspaceRepo = QString::fromStdString(std::filesystem::current_path().string());
        if (QDir(workspaceRepo + QStringLiteral("/scripts")).exists())
        {
            vm.setActiveRepository(QStringLiteral("id-self-cd"), QStringLiteral("GitFlux"), workspaceRepo, QStringLiteral("develop"));
            vm.waitForIdle(5000);
            vm.clearActiveOutput();
            vm.executeCommand(QStringLiteral("cd scripts/ && ls"));
            REQUIRE(vm.waitForIdle(5000));
            REQUIRE(lastCommandExitCode() == 0);
            REQUIRE_FALSE(collectStreamText(QStringLiteral("stderr")).contains(QStringLiteral("cd: directory non trovata")));
            const QString outWorkspace = collectStreamText(QStringLiteral("stdout"));
            REQUIRE(outWorkspace.contains(QStringLiteral("windows")));
            REQUIRE(outWorkspace.contains(QStringLiteral("linux")));
            REQUIRE(vm.activeRelativeDir() == QStringLiteral("/scripts"));

            vm.clearActiveOutput();
            vm.executeCommand(QStringLiteral("pwd"));
            REQUIRE(collectStreamText(QStringLiteral("stdout")).endsWith(QStringLiteral("scripts"), Qt::CaseInsensitive));
        }
    }

    SECTION("cd completion shows ExecuteAction row 0 for existing directories so Enter executes instead of diving into first subfolder")
    {
        // Create subfolders inside repoA/src and a sibling folder `src_extra`
        REQUIRE(QDir().mkpath(repoASub + QStringLiteral("/alpha")));
        REQUIRE(QDir().mkpath(repoASub + QStringLiteral("/beta")));
        REQUIRE(QDir().mkpath(repoA + QStringLiteral("/src_extra")));

        // Incomplete prefix `cd sr`: Enter should complete (no ExecuteAction)
        const QString partial = QStringLiteral("cd sr");
        vm.updateCompletions(partial, static_cast<int>(partial.size()));
        REQUIRE(vm.hasCompletions());
        REQUIRE_FALSE(vm.completionModel()->isExecuteAction(0));
        REQUIRE(vm.shouldEnterAcceptCompletion(0, false, partial, static_cast<int>(partial.size())));

        // Exact existing directory `cd src` with sibling `src_extra/`: row 0 is ExecuteAction -> Enter executes!
        const QString exactDir = QStringLiteral("cd src");
        vm.updateCompletions(exactDir, static_cast<int>(exactDir.size()));
        REQUIRE(vm.completionCount() >= 2);
        REQUIRE(vm.completionModel()->isExecuteAction(0));
        REQUIRE_FALSE(vm.shouldEnterAcceptCompletion(0, false, exactDir, static_cast<int>(exactDir.size())));
        REQUIRE_FALSE(vm.shouldEnterAcceptCompletion(0, true, exactDir, static_cast<int>(exactDir.size())));
        // Navigating down to row 1 (`src/` or `src_extra/`) allows Enter to complete it
        REQUIRE(vm.shouldEnterAcceptCompletion(1, true, exactDir, static_cast<int>(exactDir.size())));

        // Clicking row 0 returns executeNow = true
        const QVariantMap execExact = vm.acceptCompletion(0, exactDir, static_cast<int>(exactDir.size()));
        REQUIRE(execExact.value(QStringLiteral("accepted")).toBool());
        REQUIRE(execExact.value(QStringLiteral("executeNow")).toBool());

        // Existing directory with trailing slash `cd src/` and subfolders `alpha/`, `beta/`:
        // row 0 is ExecuteAction so pressing Enter executes `cd src/` instead of completing `src/alpha/`!
        const QString withSlash = QStringLiteral("cd src/");
        vm.updateCompletions(withSlash, static_cast<int>(withSlash.size()));
        REQUIRE(vm.completionCount() >= 3); // ExecuteAction + alpha/ + beta/
        REQUIRE(vm.completionModel()->isExecuteAction(0));
        REQUIRE_FALSE(vm.shouldEnterAcceptCompletion(0, false, withSlash, static_cast<int>(withSlash.size())));
        REQUIRE(vm.shouldEnterAcceptCompletion(1, true, withSlash, static_cast<int>(withSlash.size())));
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

    SECTION("Smart Enter completes incomplete prefixes or arrow-selected items, and executes complete commands directly")
    {
        // Incomplete prefix `git ch`: Enter should complete
        vm.updateCompletions(QStringLiteral("git ch"), 6);
        REQUIRE(vm.hasCompletions());
        REQUIRE(vm.shouldEnterAcceptCompletion(0, false, QStringLiteral("git ch"), 6));

        // Exact command `git log` without arrow navigation: Enter should execute immediately
        vm.updateCompletions(QStringLiteral("git log"), 7);
        REQUIRE(vm.hasCompletions());
        REQUIRE_FALSE(vm.shouldEnterAcceptCompletion(0, false, QStringLiteral("git log"), 7));

        // Exact command `git log` WITH explicit arrow navigation: Enter should accept the chosen suggestion
        REQUIRE(vm.shouldEnterAcceptCompletion(0, true, QStringLiteral("git log"), 7));

        // Trailing space `git log ` without arrow navigation: Enter should execute immediately
        vm.updateCompletions(QStringLiteral("git log "), 8);
        REQUIRE_FALSE(vm.shouldEnterAcceptCompletion(0, false, QStringLiteral("git log "), 8));

        // Trailing space `git log ` WITH explicit arrow navigation (if completions exist): Enter accepts
        if (vm.hasCompletions())
        {
            REQUIRE(vm.shouldEnterAcceptCompletion(0, true, QStringLiteral("git log "), 8));
        }
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

    SECTION("Script completion shows ExecuteAction row 0, chains after accepting switches, and executes on click/Enter")
    {
        const QString workspaceRepo = QString::fromStdString(std::filesystem::current_path().string());
        if (QFileInfo::exists(workspaceRepo + QStringLiteral("/scripts/windows/build.ps1")))
        {
            vm.setActiveRepository(QStringLiteral("id-self"), QStringLiteral("GitFlux"), workspaceRepo, QStringLiteral("develop"));
            vm.waitForIdle(5000);

            const QString input0 = QStringLiteral(".\\scripts\\windows\\build.ps1 ");
            vm.updateCompletions(input0, static_cast<int>(input0.size()));
            REQUIRE(vm.hasCompletions());
            REQUIRE(vm.completionModel()->isExecuteAction(0));
            // Enter on row 0 executes immediately (does NOT treat it as text completion)
            REQUIRE_FALSE(vm.shouldEnterAcceptCompletion(0, false, input0, static_cast<int>(input0.size())));
            REQUIRE_FALSE(vm.shouldEnterAcceptCompletion(0, true, input0, static_cast<int>(input0.size())));

            // Accept parameter at index 1 (e.g. -Qt): popup must stay open with "▶ Esegui script" at row 0
            const QVariantMap acc = vm.acceptCompletion(1, input0, static_cast<int>(input0.size()));
            REQUIRE(acc.value(QStringLiteral("accepted")).toBool());
            REQUIRE_FALSE(acc.value(QStringLiteral("executeNow")).toBool());
            REQUIRE(vm.hasCompletions());
            REQUIRE(vm.completionModel()->isExecuteAction(0));
            const QString row0Label = vm.completionModel()->data(
                vm.completionModel()->index(0, 0), TerminalCompletionListModel::DisplayTextRole).toString();
            REQUIRE(row0Label == QStringLiteral("▶ Esegui script"));

            // Clicking row 0 ("▶ Esegui script") returns executeNow = true
            const QString afterText = acc.value(QStringLiteral("text")).toString();
            const QVariantMap execRes = vm.acceptCompletion(0, afterText, static_cast<int>(afterText.size()));
            REQUIRE(execRes.value(QStringLiteral("accepted")).toBool());
            REQUIRE(execRes.value(QStringLiteral("executeNow")).toBool());
        }
    }
}

TEST_CASE("TerminalDockPanel keeps prompt focus for Ctrl+V and Up/Down after clicking output without selection", "[qml][ui][terminal]")
{
    ensureTerminalTestQtApp();

    const auto discovery = gf::infra::git::GitDiscovery::discover();
    REQUIRE(discovery.has_value());
    const auto gitExe = discovery->directBinaryPath.empty() ? discovery->discoveredInPath : discovery->directBinaryPath;

    gf::infra::git::NativeGitCommandExecutor gitExec(gitExe);
    gf::infra::terminal::NativeTerminalProcessRunner runner(gitExe);
    GitCatalogService catalog(&gitExec);

    QTemporaryDir tempRoot;
    REQUIRE(tempRoot.isValid());

    TerminalSessionViewModel vm(&runner, &catalog, &gitExec);
    vm.setActiveRepository(QStringLiteral("id-qml"), QStringLiteral("QmlRepo"), tempRoot.path(), QStringLiteral("main"));
    vm.setIsVisible(true);
    vm.executeCommand(QStringLiteral("echo alpha_line beta_line gamma_line"));
    REQUIRE(vm.waitForIdle(5000));
    vm.executeCommand(QStringLiteral("echo second_cmd"));
    REQUIRE(vm.waitForIdle(5000));

    const auto qmlPath = std::filesystem::path(__FILE__).parent_path().parent_path().parent_path()
        / "src" / "desktop" / "ui" / "qml" / "views" / "TerminalDockPanel.qml";
    QQmlEngine engine;
    QQmlComponent component(&engine, QUrl::fromLocalFile(QString::fromStdString(qmlPath.string())));
    INFO("QML errors: " << component.errorString().toStdString());
    REQUIRE(component.isReady());

    std::unique_ptr<QObject> panelObj(component.create());
    REQUIRE(panelObj != nullptr);
    auto* panelItem = qobject_cast<QQuickItem*>(panelObj.get());
    REQUIRE(panelItem != nullptr);

    QQuickWindow window;
    window.resize(900, 420);
    panelItem->setParentItem(window.contentItem());
    panelItem->setSize(QSizeF(900, 420));
    panelObj->setProperty("terminalVm", QVariant::fromValue(static_cast<QObject*>(&vm)));
    window.show();
    window.requestActivate();
    QCoreApplication::processEvents();
    REQUIRE(QTest::qWaitForWindowActive(&window));

    auto* outputEdit = panelItem->findChild<QQuickItem*>(QStringLiteral("outputTextEdit"));
    auto* cmdInput = panelItem->findChild<QQuickItem*>(QStringLiteral("cmdInput"));
    REQUIRE(outputEdit != nullptr);
    REQUIRE(cmdInput != nullptr);
    REQUIRE(outputEdit->property("text").toString().contains(QStringLiteral("alpha_line")));

    auto centerInWindow = [](QQuickItem* item, qreal rx = 0.5, qreal ry = 0.5) -> QPoint
    {
        const QPointF p = item->mapToScene(QPointF(item->width() * rx, item->height() * ry));
        return QPoint(static_cast<int>(p.x()), static_cast<int>(p.y()));
    };

    auto findQuickItemByName = [](QQuickItem* root, const QString& name) -> QQuickItem*
    {
        if (!root) return nullptr;
        std::vector<QQuickItem*> stack{root};
        while (!stack.empty())
        {
            QQuickItem* cur = stack.back();
            stack.pop_back();
            if (cur->objectName() == name) return cur;
            const auto children = cur->childItems();
            for (QQuickItem* child : children)
            {
                stack.push_back(child);
            }
        }
        return nullptr;
    };

    SECTION("Single click on output without selecting text returns focus to prompt for Up/Down and Ctrl+V")
    {
        QMetaObject::invokeMethod(panelObj.get(), "focusInput");
        QCoreApplication::processEvents();
        REQUIRE(cmdInput->hasActiveFocus());

        // Single click in the middle/empty area of outputTextEdit
        const QPoint clickPt = centerInWindow(outputEdit, 0.5, 0.7);
        QTest::mouseClick(&window, Qt::LeftButton, Qt::NoModifier, clickPt);
        QCoreApplication::processEvents();

        CHECK(outputEdit->property("selectedText").toString().isEmpty());
        CHECK(cmdInput->hasActiveFocus());
        CHECK_FALSE(outputEdit->hasActiveFocus());

        // Up/Down navigates command history
        QTest::keyClick(&window, Qt::Key_Up);
        QCoreApplication::processEvents();
        CHECK(cmdInput->property("text").toString() == QStringLiteral("echo second_cmd"));

        QTest::keyClick(&window, Qt::Key_Up);
        QCoreApplication::processEvents();
        CHECK(cmdInput->property("text").toString() == QStringLiteral("echo alpha_line beta_line gamma_line"));

        QTest::keyClick(&window, Qt::Key_Down);
        QCoreApplication::processEvents();
        CHECK(cmdInput->property("text").toString() == QStringLiteral("echo second_cmd"));

        QTest::keyClick(&window, Qt::Key_Down);
        QCoreApplication::processEvents();
        CHECK(cmdInput->property("text").toString().isEmpty());

        // Ctrl+V pastes into cmdInput
        QGuiApplication::clipboard()->setText(QStringLiteral("git status"));
        QTest::keyClick(&window, Qt::Key_V, Qt::ControlModifier);
        QCoreApplication::processEvents();
        CHECK(cmdInput->property("text").toString() == QStringLiteral("git status"));
    }

    SECTION("Drag selection keeps output focused for Ctrl+C, while Ctrl+V and Up/Down still jump to prompt")
    {
        const QPoint startPt = outputEdit->mapToScene(QPointF(10, 12)).toPoint();
        const QPoint endPt = outputEdit->mapToScene(QPointF(260, 48)).toPoint();
        QTest::mousePress(&window, Qt::LeftButton, Qt::NoModifier, startPt);
        QTest::mouseMove(&window, endPt, 10);
        QTest::mouseRelease(&window, Qt::LeftButton, Qt::NoModifier, endPt);
        QCoreApplication::processEvents();

        const QString selected = outputEdit->property("selectedText").toString();
        REQUIRE_FALSE(selected.isEmpty());
        CHECK(outputEdit->hasActiveFocus());

        // Ctrl+C copies the selected output text
        QGuiApplication::clipboard()->setText(QStringLiteral("stale"));
        QTest::keyClick(&window, Qt::Key_C, Qt::ControlModifier);
        QCoreApplication::processEvents();
        const QString copiedText = QGuiApplication::clipboard()->text();
        QString normalizedSelected = selected;
        normalizedSelected.replace(QChar(0x2029), QLatin1Char('\n'));
        normalizedSelected.replace(QChar(0x00A0), QLatin1Char(' '));
        CHECK(copiedText == normalizedSelected);
        CHECK(copiedText.contains(QStringLiteral("alpha_line beta_line gamma_line")));
        CHECK(outputEdit->hasActiveFocus());

        // Shift+Up extends keyboard selection inside outputEdit rather than jumping to prompt
        QTest::keyClick(&window, Qt::Key_Up, Qt::ShiftModifier);
        QCoreApplication::processEvents();
        CHECK(outputEdit->hasActiveFocus());

        // Clicking inside the output without dragging clears selection and returns focus to prompt
        QTest::mouseClick(&window, Qt::LeftButton, Qt::NoModifier, startPt);
        QCoreApplication::processEvents();
        CHECK(outputEdit->property("selectedText").toString().isEmpty());
        CHECK(cmdInput->hasActiveFocus());

        // Double-clicking a word selects it and keeps focus on outputEdit
        QTest::mouseDClick(&window, Qt::LeftButton, Qt::NoModifier, startPt);
        QCoreApplication::processEvents();
        CHECK_FALSE(outputEdit->property("selectedText").toString().isEmpty());
        CHECK(outputEdit->hasActiveFocus());

        // Select all via Ctrl+A (output keeps focus), then Up arrow jumps to prompt history
        outputEdit->forceActiveFocus();
        QCoreApplication::processEvents();
        QTest::keyClick(&window, Qt::Key_A, Qt::ControlModifier);
        QCoreApplication::processEvents();
        REQUIRE_FALSE(outputEdit->property("selectedText").toString().isEmpty());
        REQUIRE(outputEdit->hasActiveFocus());

        QTest::keyClick(&window, Qt::Key_Up);
        QCoreApplication::processEvents();
        CHECK(cmdInput->hasActiveFocus());
        CHECK(cmdInput->property("text").toString() == QStringLiteral("echo second_cmd"));

        // Focus output again with Ctrl+A, then Ctrl+V jumps to prompt and pastes
        cmdInput->setProperty("text", QString());
        outputEdit->forceActiveFocus();
        QCoreApplication::processEvents();
        QTest::keyClick(&window, Qt::Key_A, Qt::ControlModifier);
        QCoreApplication::processEvents();
        REQUIRE(outputEdit->hasActiveFocus());

        QGuiApplication::clipboard()->setText(QStringLiteral("git diff --stat"));
        QTest::keyClick(&window, Qt::Key_V, Qt::ControlModifier);
        QCoreApplication::processEvents();
        CHECK(cmdInput->hasActiveFocus());
        CHECK(cmdInput->property("text").toString() == QStringLiteral("git diff --stat"));
    }

    SECTION("While a command is running, prompt highlights stdin mode and disables Up/Down history")
    {
        auto* promptBar = panelItem->findChild<QQuickItem*>(QStringLiteral("promptBar"));
        auto* promptTopBorder = panelItem->findChild<QQuickItem*>(QStringLiteral("promptTopBorder"));
        REQUIRE(promptBar != nullptr);
        REQUIRE(promptTopBorder != nullptr);
        CHECK_FALSE(promptBar->property("isStdinMode").toBool());
        CHECK(promptTopBorder->property("height").toInt() == 1);

        // Start a command that waits for one line on stdin
#ifdef _WIN32
        vm.executeCommand(QStringLiteral("set /p line= && echo GOT:%line%"));
#else
        vm.executeCommand(QStringLiteral("read line && echo \"GOT:$line\""));
#endif
        REQUIRE(QTest::qWaitFor([&]() { return vm.isRunning(); }, 3000));
        QCoreApplication::processEvents();

        CHECK(promptBar->property("isStdinMode").toBool());
        CHECK(promptTopBorder->property("height").toInt() == 2);

        // Pressing Up/Down on cmdInput must NOT inject old shell commands while running
        cmdInput->forceActiveFocus();
        QCoreApplication::processEvents();
        QTest::keyClick(&window, Qt::Key_Up);
        QCoreApplication::processEvents();
        CHECK(cmdInput->property("text").toString().isEmpty());

        QTest::keyClick(&window, Qt::Key_Down);
        QCoreApplication::processEvents();
        CHECK(cmdInput->property("text").toString().isEmpty());

        // Interactive stdin input still works and completes the waiting command
        QGuiApplication::clipboard()->setText(QStringLiteral("hello_stdin"));
        QTest::keyClick(&window, Qt::Key_V, Qt::ControlModifier);
        QCoreApplication::processEvents();
        QTest::keyClick(&window, Qt::Key_Return);
        REQUIRE(vm.waitForIdle(5000));
        QCoreApplication::processEvents();

        CHECK_FALSE(vm.isRunning());
        CHECK_FALSE(promptBar->property("isStdinMode").toBool());
        CHECK(promptTopBorder->property("height").toInt() == 1);
        CHECK(outputEdit->property("text").toString().contains(QStringLiteral("GOT:hello_stdin")));

        // Once finished, Up navigates command history again
        QTest::keyClick(&window, Qt::Key_Up);
        QCoreApplication::processEvents();
        CHECK(cmdInput->property("text").toString().contains(QStringLiteral("echo")));
    }

    SECTION("Ctrl+Shift+T opens new terminal tabs and Ctrl+W closes idle tabs or confirms on running tabs")
    {
        REQUIRE(vm.tabs().size() == 1);
        REQUIRE(vm.isVisible());
        cmdInput->forceActiveFocus();
        QCoreApplication::processEvents();

        // Ctrl+Shift+T opens tab 2
        QTest::keyClick(&window, Qt::Key_T, Qt::ControlModifier | Qt::ShiftModifier);
        QCoreApplication::processEvents();
        REQUIRE(vm.tabs().size() == 2);
        CHECK(vm.activeTabIndex() == 1);
        CHECK(cmdInput->hasActiveFocus());

        // Ctrl+Shift+N does NOT open a terminal tab anymore (stays at 2)
        QTest::keyClick(&window, Qt::Key_N, Qt::ControlModifier | Qt::ShiftModifier);
        QCoreApplication::processEvents();
        CHECK(vm.tabs().size() == 2);

        // Start a running command in tab 2
#ifdef _WIN32
        vm.executeCommand(QStringLiteral("set /p line="));
#else
        vm.executeCommand(QStringLiteral("read line"));
#endif
        REQUIRE(QTest::qWaitFor([&]() { return vm.isRunning(); }, 3000));
        QCoreApplication::processEvents();

        auto* closeDialog = panelObj->findChild<QObject*>(QStringLiteral("closeRunningTabDialog"));
        REQUIRE(closeDialog != nullptr);
        CHECK_FALSE(closeDialog->property("opened").toBool());

        // Ctrl+W on running tab opens confirmation dialog instead of closing immediately
        QTest::keyClick(&window, Qt::Key_W, Qt::ControlModifier);
        QCoreApplication::processEvents();
        REQUIRE(closeDialog->property("opened").toBool());
        CHECK(vm.tabs().size() == 2);
        CHECK(vm.isRunning());

        // Clicking Cancel keeps the running tab open and restores focus to prompt
        auto* cancelBtn = panelObj->findChild<QQuickItem*>(QStringLiteral("cancelCloseTabBtn"));
        REQUIRE(cancelBtn != nullptr);
        QTest::mouseClick(&window, Qt::LeftButton, Qt::NoModifier, centerInWindow(cancelBtn));
        QCoreApplication::processEvents();
        CHECK_FALSE(closeDialog->property("opened").toBool());
        CHECK(vm.tabs().size() == 2);
        CHECK(vm.isRunning());
        CHECK(cmdInput->hasActiveFocus());

        // Ctrl+W again -> Confirm terminates running command and closes tab 2
        QTest::keyClick(&window, Qt::Key_W, Qt::ControlModifier);
        QCoreApplication::processEvents();
        REQUIRE(closeDialog->property("opened").toBool());
        auto* confirmBtn = panelObj->findChild<QQuickItem*>(QStringLiteral("confirmCloseTabBtn"));
        REQUIRE(confirmBtn != nullptr);
        QTest::mouseClick(&window, Qt::LeftButton, Qt::NoModifier, centerInWindow(confirmBtn));
        QCoreApplication::processEvents();

        CHECK_FALSE(closeDialog->property("opened").toBool());
        REQUIRE(vm.tabs().size() == 1);
        CHECK(vm.isVisible());
        CHECK_FALSE(vm.isRunning());
        CHECK(cmdInput->hasActiveFocus());

        // Ctrl+W on the remaining idle tab closes immediately without dialog and hides the dock
        QTest::keyClick(&window, Qt::Key_W, Qt::ControlModifier);
        QCoreApplication::processEvents();
        CHECK_FALSE(closeDialog->property("opened").toBool());
        CHECK(vm.tabs().isEmpty());
        CHECK_FALSE(vm.isVisible());
    }

    SECTION("Copy buttons, CWD path pill and Ctrl+C selection show brief 'Copiato' visual feedback")
    {
        auto* cwdPill = panelObj->findChild<QQuickItem*>(QStringLiteral("cwdPill"));
        auto* cwdLabel = panelObj->findChild<QQuickItem*>(QStringLiteral("cwdLabel"));
        auto* copyOutputBtn = panelObj->findChild<QQuickItem*>(QStringLiteral("copyOutputBtn"));
        auto* copyToastBadge = panelObj->findChild<QQuickItem*>(QStringLiteral("copyToastBadge"));
        auto* copyToastText = panelObj->findChild<QQuickItem*>(QStringLiteral("copyToastText"));
        REQUIRE(cwdPill != nullptr);
        REQUIRE(cwdLabel != nullptr);
        REQUIRE(copyOutputBtn != nullptr);
        REQUIRE(copyToastBadge != nullptr);
        REQUIRE(copyToastText != nullptr);
        CHECK(panelObj->property("copiedFeedbackTarget").toString().isEmpty());
        CHECK_FALSE(copyToastBadge->isVisible());

        // Click CWD path pill -> copies working dir and displays "✓ Copiato" + toast
        QGuiApplication::clipboard()->setText(QStringLiteral("stale"));
        QTest::mouseClick(&window, Qt::LeftButton, Qt::NoModifier, centerInWindow(cwdPill));
        QCoreApplication::processEvents();
        CHECK(QGuiApplication::clipboard()->text() == vm.activeWorkingDir());
        CHECK(panelObj->property("copiedFeedbackTarget").toString() == QStringLiteral("cwd"));
        CHECK(cwdPill->property("isCopied").toBool());
        CHECK(cwdLabel->property("text").toString() == QStringLiteral("✓ Copiato"));
        CHECK(cwdPill->width() >= cwdLabel->property("implicitWidth").toReal() + 13.0);
        CHECK(copyToastBadge->isVisible());
        CHECK(copyToastText->property("text").toString() == QStringLiteral("✓ Percorso copiato negli appunti"));

        // Click header Copy Output button -> copies output and shows feedback
        QGuiApplication::clipboard()->setText(QStringLiteral("stale"));
        QTest::mouseClick(&window, Qt::LeftButton, Qt::NoModifier, centerInWindow(copyOutputBtn));
        QCoreApplication::processEvents();
        CHECK(QGuiApplication::clipboard()->text().contains(QStringLiteral("alpha_line")));
        CHECK(panelObj->property("copiedFeedbackTarget").toString() == QStringLiteral("output"));
        CHECK(copyOutputBtn->property("isCopied").toBool());
        CHECK_FALSE(cwdPill->property("isCopied").toBool());
        CHECK(copyToastBadge->isVisible());
        CHECK(copyToastText->property("text").toString() == QStringLiteral("✓ Output copiato negli appunti"));

        // Ctrl+C on selected output text -> copies selection and shows floating toast
        outputEdit->forceActiveFocus();
        QCoreApplication::processEvents();
        QTest::keyClick(&window, Qt::Key_A, Qt::ControlModifier);
        QCoreApplication::processEvents();
        QTest::keyClick(&window, Qt::Key_C, Qt::ControlModifier);
        QCoreApplication::processEvents();
        CHECK(panelObj->property("copiedFeedbackTarget").toString() == QStringLiteral("selection"));
        CHECK(copyToastBadge->isVisible());
        CHECK(copyToastText->property("text").toString() == QStringLiteral("✓ Selezione copiata negli appunti"));

        // Pager Copy button also shows feedback when pager is open
        const QString workspaceRepo = QString::fromStdString(std::filesystem::current_path().string());
        if (QDir(workspaceRepo + QStringLiteral("/.git")).exists())
        {
            vm.setActiveRepository(QStringLiteral("id-pager"), QStringLiteral("GitFlux"), workspaceRepo, QStringLiteral("develop"));
            vm.executeCommand(QStringLiteral("git log -n 40"));
            REQUIRE(vm.waitForIdle(5000));
            QCoreApplication::processEvents();
            REQUIRE(vm.pagerActive());

            auto* pagerCopyBtn = panelObj->findChild<QQuickItem*>(QStringLiteral("pagerCopyBtn"));
            REQUIRE(pagerCopyBtn != nullptr);
            QGuiApplication::clipboard()->setText(QStringLiteral("stale"));
            QTest::mouseClick(&window, Qt::LeftButton, Qt::NoModifier, centerInWindow(pagerCopyBtn));
            QCoreApplication::processEvents();
            CHECK(QGuiApplication::clipboard()->text().contains(QStringLiteral("commit ")));
            CHECK(panelObj->property("copiedFeedbackTarget").toString() == QStringLiteral("pager"));
            CHECK(pagerCopyBtn->property("isCopied").toBool());

            // Also verify drag selection auto-scrolls pagerFlickable past bottom edge
            auto* pagerFlickable = panelObj->findChild<QQuickItem*>(QStringLiteral("pagerFlickable"));
            auto* pagerEdit = panelObj->findChild<QQuickItem*>(QStringLiteral("pagerTextEdit"));
            REQUIRE(pagerFlickable != nullptr);
            REQUIRE(pagerEdit != nullptr);
            REQUIRE(pagerFlickable->property("contentHeight").toReal() > pagerFlickable->height() + 50.0);

            pagerFlickable->setProperty("contentY", 0.0);
            QCoreApplication::processEvents();
            const QPoint pStart = pagerFlickable->mapToScene(QPointF(24, 24)).toPoint();
            const QPoint pBelow = pagerFlickable->mapToScene(QPointF(48, pagerFlickable->height() + 30)).toPoint();
            QTest::mousePress(&window, Qt::LeftButton, Qt::NoModifier, pStart);
            QTest::mouseMove(&window, pBelow, 10);
            QTest::qWait(120);
            QCoreApplication::processEvents();
            const qreal pScrolledDownY = pagerFlickable->property("contentY").toReal();
            const QString pSelected = pagerEdit->property("selectedText").toString();
            QTest::mouseRelease(&window, Qt::LeftButton, Qt::NoModifier, pBelow);
            QCoreApplication::processEvents();

            CHECK(pScrolledDownY > 10.0);
            CHECK_FALSE(pSelected.isEmpty());
            vm.closePager();
        }
    }

    SECTION("Drag selection auto-scrolls outputFlickable when dragging past bottom or top edge")
    {
        // Populate many lines so outputFlickable becomes scrollable
        vm.executeCommand(QStringLiteral("seq 1 60"));
        REQUIRE(vm.waitForIdle(5000));
        QCoreApplication::processEvents();

        auto* outputFlickable = panelObj->findChild<QQuickItem*>(QStringLiteral("outputFlickable"));
        REQUIRE(outputFlickable != nullptr);
        REQUIRE(outputFlickable->property("contentHeight").toReal() > outputFlickable->height() + 50.0);

        // Start from top of output and drag past the bottom edge
        outputFlickable->setProperty("contentY", 0.0);
        QCoreApplication::processEvents();
        const QPoint startPt = outputFlickable->mapToScene(QPointF(24, 24)).toPoint();
        const QPoint belowEdgePt = outputFlickable->mapToScene(QPointF(48, outputFlickable->height() + 30)).toPoint();

        QTest::mousePress(&window, Qt::LeftButton, Qt::NoModifier, startPt);
        QTest::mouseMove(&window, belowEdgePt, 10);
        QTest::qWait(120);
        QCoreApplication::processEvents();
        const qreal scrolledDownY = outputFlickable->property("contentY").toReal();
        const QString downSelected = outputEdit->property("selectedText").toString();
        QTest::mouseRelease(&window, Qt::LeftButton, Qt::NoModifier, belowEdgePt);
        QCoreApplication::processEvents();

        CHECK(scrolledDownY > 10.0);
        CHECK_FALSE(downSelected.isEmpty());

        // Now start near bottom and drag past the top edge
        const qreal maxY = outputFlickable->property("contentHeight").toReal() - outputFlickable->height();
        outputFlickable->setProperty("contentY", maxY);
        QCoreApplication::processEvents();
        const QPoint nearBottomPt = outputFlickable->mapToScene(QPointF(24, outputFlickable->height() - 24)).toPoint();
        const QPoint aboveEdgePt = outputFlickable->mapToScene(QPointF(48, -30)).toPoint();

        QTest::mousePress(&window, Qt::LeftButton, Qt::NoModifier, nearBottomPt);
        QTest::mouseMove(&window, aboveEdgePt, 10);
        QTest::qWait(120);
        QCoreApplication::processEvents();
        const qreal scrolledUpY = outputFlickable->property("contentY").toReal();
        const QString upSelected = outputEdit->property("selectedText").toString();
        QTest::mouseRelease(&window, Qt::LeftButton, Qt::NoModifier, aboveEdgePt);
        QCoreApplication::processEvents();

        CHECK(scrolledUpY < maxY - 10.0);
        CHECK_FALSE(upSelected.isEmpty());
    }

    SECTION("Completion popup footer adapts when row 0 execute action is highlighted")
    {
        QFile psScript(tempRoot.filePath(QStringLiteral("build.ps1")));
        REQUIRE(psScript.open(QIODevice::WriteOnly | QIODevice::Text));
        psScript.write("param([switch]$Qt, [switch]$Tests)\nWrite-Output 'ok'\n");
        psScript.close();

        auto* footerHint = panelObj->findChild<QQuickItem*>(QStringLiteral("completionFooterHint"));
        REQUIRE(footerHint != nullptr);

        cmdInput->forceActiveFocus();
        QCoreApplication::processEvents();

        const QString cmd = QStringLiteral("./build.ps1 ");
        cmdInput->setProperty("text", cmd);
        cmdInput->setProperty("cursorPosition", static_cast<int>(cmd.size()));
        vm.updateCompletions(cmd, static_cast<int>(cmd.size()), true);
        QCoreApplication::processEvents();

        REQUIRE(vm.hasCompletions());
        REQUIRE(vm.completionModel()->isExecuteAction(0));
        CHECK(footerHint->property("text").toString()
              == QStringLiteral("Invio esegui  •  ↓ / Tab scegli parametro  •  Esc chiudi"));

        // Moving to a parameter row switches footer back to completion hint
        QTest::keyClick(&window, Qt::Key_Down);
        QCoreApplication::processEvents();
        CHECK(footerHint->property("text").toString().contains(QStringLiteral("completa")));

        // Moving back to row 0 restores execute hint
        QTest::keyClick(&window, Qt::Key_Up);
        QCoreApplication::processEvents();
        CHECK(footerHint->property("text").toString()
              == QStringLiteral("Invio esegui  •  ↓ / Tab scegli parametro  •  Esc chiudi"));
    }

    SECTION("Tab rename via double-click or context menu and active tab shell switch via badge or menu")
    {
        auto* tabMa0 = findQuickItemByName(panelItem, QStringLiteral("termTabMouseArea_0"));
        auto* renameDialog = panelObj->findChild<QObject*>(QStringLiteral("renameTabDialog"));
        auto* renameInput = findQuickItemByName(panelItem, QStringLiteral("renameTabInput"));
        if (!renameInput) renameInput = panelObj->findChild<QQuickItem*>(QStringLiteral("renameTabInput"));
        auto* tabMenu = panelObj->findChild<QObject*>(QStringLiteral("tabContextMenu"));
        auto* tabMenuRename = findQuickItemByName(panelItem, QStringLiteral("tabMenuRenameItem"));
        if (!tabMenuRename) tabMenuRename = panelObj->findChild<QQuickItem*>(QStringLiteral("tabMenuRenameItem"));
        REQUIRE(tabMa0 != nullptr);
        REQUIRE(renameDialog != nullptr);
        REQUIRE(renameInput != nullptr);
        REQUIRE(tabMenu != nullptr);
        REQUIRE(tabMenuRename != nullptr);

        // 1. Double-click on tab 0 opens rename dialog and updates tab title on Enter
        QTest::mouseDClick(&window, Qt::LeftButton, Qt::NoModifier, centerInWindow(tabMa0));
        QCoreApplication::processEvents();
        REQUIRE(renameDialog->property("opened").toBool());
        CHECK(renameInput->hasActiveFocus());

        renameInput->setProperty("text", QStringLiteral("Build"));
        QTest::keyClick(&window, Qt::Key_Return);
        QCoreApplication::processEvents();
        CHECK_FALSE(renameDialog->property("opened").toBool());
        REQUIRE(vm.tabs().size() == 1);
        CHECK(vm.tabs().at(0).toMap().value(QStringLiteral("title")).toString() == QStringLiteral("Build"));
        auto* title0 = findQuickItemByName(panelItem, QStringLiteral("termTabTitle_0"));
        REQUIRE(title0 != nullptr);
        CHECK(title0->property("text").toString() == QStringLiteral("Build"));
        CHECK(cmdInput->hasActiveFocus());

        // 2. Right-click on tab 0 opens context menu -> Rename updates title to "Server"
        tabMa0 = findQuickItemByName(panelItem, QStringLiteral("termTabMouseArea_0"));
        REQUIRE(tabMa0 != nullptr);
        REQUIRE(QTest::qWaitFor([&]() { return tabMa0->height() > 10.0; }, 1000));
        QTest::mouseClick(&window, Qt::RightButton, Qt::NoModifier, centerInWindow(tabMa0));
        REQUIRE(QTest::qWaitFor([&]() { return tabMenu->property("opened").toBool(); }, 1000));

        QTest::mouseClick(&window, Qt::LeftButton, Qt::NoModifier, centerInWindow(tabMenuRename));
        QCoreApplication::processEvents();
        REQUIRE(renameDialog->property("opened").toBool());
        renameInput->setProperty("text", QStringLiteral("Server"));
        auto* confirmRenameBtn = panelObj->findChild<QQuickItem*>(QStringLiteral("confirmRenameTabBtn"));
        REQUIRE(confirmRenameBtn != nullptr);
        QTest::mouseClick(&window, Qt::LeftButton, Qt::NoModifier, centerInWindow(confirmRenameBtn));
        QCoreApplication::processEvents();
        CHECK(vm.tabs().at(0).toMap().value(QStringLiteral("title")).toString() == QStringLiteral("Server"));

        // 3. Switch active tab shell via shell badge and via tab context menu (when >= 2 shells available)
        const QVariantList shells = vm.availableShells();
        if (shells.size() > 1)
        {
            const QString firstShell = vm.activeShellId();
            QString otherShell;
            for (const QVariant& sv : shells)
            {
                const QString sid = sv.toMap().value(QStringLiteral("id")).toString();
                if (!sid.isEmpty() && sid != firstShell)
                {
                    otherShell = sid;
                    break;
                }
            }
            REQUIRE_FALSE(otherShell.isEmpty());

            auto* shellBadge = findQuickItemByName(panelItem, QStringLiteral("activeTabShellBadge"));
            auto* shellMenu = panelObj->findChild<QObject*>(QStringLiteral("activeTabShellMenu"));
            REQUIRE(shellBadge != nullptr);
            REQUIRE(shellMenu != nullptr);
            CHECK(shellBadge->isVisible());
            REQUIRE(QTest::qWaitFor([&]() { return shellBadge->height() > 10.0 && shellBadge->width() > 10.0; }, 1000));

            QTest::mouseClick(&window, Qt::LeftButton, Qt::NoModifier, centerInWindow(shellBadge));
            REQUIRE(QTest::qWaitFor([&]() { return shellMenu->property("opened").toBool(); }, 1000));

            auto* shellMenuContent = shellMenu->property("contentItem").value<QQuickItem*>();
            auto* otherShellItem = findQuickItemByName(shellMenuContent, QStringLiteral("shellBadgeMenuItem_") + otherShell);
            REQUIRE(otherShellItem != nullptr);
            QTest::mouseClick(&window, Qt::LeftButton, Qt::NoModifier, centerInWindow(otherShellItem));
            QCoreApplication::processEvents();
            CHECK(vm.activeShellId() == otherShell);
            CHECK(cmdInput->hasActiveFocus());

            // Switch back via tab right-click context menu
            tabMa0 = findQuickItemByName(panelItem, QStringLiteral("termTabMouseArea_0"));
            REQUIRE(tabMa0 != nullptr);
            REQUIRE(QTest::qWaitFor([&]() { return tabMa0->height() > 10.0; }, 1000));
            QTest::mouseClick(&window, Qt::RightButton, Qt::NoModifier, centerInWindow(tabMa0));
            REQUIRE(QTest::qWaitFor([&]() { return tabMenu->property("opened").toBool(); }, 1000));

            auto* tabMenuContent = tabMenu->property("contentItem").value<QQuickItem*>();
            auto* backShellItem = findQuickItemByName(tabMenuContent, QStringLiteral("tabMenuShell_") + firstShell);
            REQUIRE(backShellItem != nullptr);
            QTest::mouseClick(&window, Qt::LeftButton, Qt::NoModifier, centerInWindow(backShellItem));
            QCoreApplication::processEvents();
            CHECK(vm.activeShellId() == firstShell);
            CHECK(cmdInput->hasActiveFocus());
        }
    }

    SECTION("Per-tab color customization via rename dialog, quick color dialog and ViewModel API")
    {
        REQUIRE(vm.tabColorPalette().size() >= 6);
        CHECK(vm.activeTabColor().isEmpty());

        auto* promptRepoText = findQuickItemByName(panelItem, QStringLiteral("promptRepoText"));
        auto* promptBranchChipText = findQuickItemByName(panelItem, QStringLiteral("promptBranchChipText"));
        REQUIRE(promptRepoText != nullptr);
        REQUIRE(promptBranchChipText != nullptr);
        // On first open (default color), repo name and branch chip share the same default accent (#38BDF8)
        CHECK(promptRepoText->property("color").toString().compare(QStringLiteral("#38bdf8"), Qt::CaseInsensitive) == 0);
        CHECK(promptBranchChipText->property("color").toString().compare(QStringLiteral("#38bdf8"), Qt::CaseInsensitive) == 0);

        // Invalid color strings are ignored, valid hex colors are accepted and isolated per tab
        vm.setTabColor(0, QStringLiteral("not-a-color"));
        CHECK(vm.activeTabColor().isEmpty());

        // Open rename dialog on tab 0, pick emerald color + save
        auto* tabMa0 = findQuickItemByName(panelItem, QStringLiteral("termTabMouseArea_0"));
        auto* renameDialog = panelObj->findChild<QObject*>(QStringLiteral("renameTabDialog"));
        auto* renameInput = panelObj->findChild<QQuickItem*>(QStringLiteral("renameTabInput"));
        REQUIRE(tabMa0 != nullptr);
        REQUIRE(renameDialog != nullptr);
        REQUIRE(renameInput != nullptr);

        QTest::mouseDClick(&window, Qt::LeftButton, Qt::NoModifier, centerInWindow(tabMa0));
        QCoreApplication::processEvents();
        REQUIRE(renameDialog->property("opened").toBool());

        auto* renameContent = renameDialog->property("contentItem").value<QQuickItem*>();
        auto* emeraldSwatch = findQuickItemByName(renameContent, QStringLiteral("renameTabSwatch_#34D399"));
        REQUIRE(emeraldSwatch != nullptr);
        REQUIRE(QTest::qWaitFor([&]() { return emeraldSwatch->x() > 10.0 && emeraldSwatch->width() > 10.0; }, 1000));
        QTest::mouseClick(&window, Qt::LeftButton, Qt::NoModifier, centerInWindow(emeraldSwatch));
        QCoreApplication::processEvents();

        renameInput->setProperty("text", QStringLiteral("Server"));
        QTest::keyClick(&window, Qt::Key_Return);
        QCoreApplication::processEvents();
        INFO("after renameDialog activeTabColor=" << vm.activeTabColor().toStdString());
        CHECK(vm.activeTabColor() == QStringLiteral("#34D399"));
        CHECK(vm.tabs().at(0).toMap().value(QStringLiteral("accentColor")).toString() == QStringLiteral("#34D399"));

        auto* dot0 = findQuickItemByName(panelItem, QStringLiteral("termTabDot_0"));
        REQUIRE(dot0 != nullptr);
        CHECK(dot0->property("color").toString().compare(QStringLiteral("#34d399"), Qt::CaseInsensitive) == 0);

        // Create tab 2 -> starts with default empty color, while tab 0 retains #34D399
        vm.createTab();
        QCoreApplication::processEvents();
        REQUIRE(vm.tabs().size() == 2);
        CHECK(vm.activeTabIndex() == 1);
        CHECK(vm.activeTabColor().isEmpty());
        CHECK(vm.tabs().at(0).toMap().value(QStringLiteral("accentColor")).toString() == QStringLiteral("#34D399"));

        // Right-click tab 1 -> "Colore scheda..." opens quick color dialog -> 1-click applies #FBBF24
        auto* tabMa1 = findQuickItemByName(panelItem, QStringLiteral("termTabMouseArea_1"));
        auto* tabMenu = panelObj->findChild<QObject*>(QStringLiteral("tabContextMenu"));
        auto* colorDialog = panelObj->findChild<QObject*>(QStringLiteral("tabColorDialog"));
        REQUIRE(tabMa1 != nullptr);
        REQUIRE(tabMenu != nullptr);
        REQUIRE(colorDialog != nullptr);
        REQUIRE(QTest::qWaitFor([&]() { return tabMa1->height() > 10.0; }, 1000));

        QTest::mouseClick(&window, Qt::RightButton, Qt::NoModifier, centerInWindow(tabMa1));
        REQUIRE(QTest::qWaitFor([&]() { return tabMenu->property("opened").toBool(); }, 1000));
        auto* tabMenuContent = tabMenu->property("contentItem").value<QQuickItem*>();
        auto* tabMenuColor = findQuickItemByName(tabMenuContent, QStringLiteral("tabMenuColorItem"));
        REQUIRE(tabMenuColor != nullptr);
        QTest::mouseClick(&window, Qt::LeftButton, Qt::NoModifier, centerInWindow(tabMenuColor));
        QCoreApplication::processEvents();
        REQUIRE(colorDialog->property("opened").toBool());

        auto* colorContent = colorDialog->property("contentItem").value<QQuickItem*>();
        auto* amberSwatch = findQuickItemByName(colorContent, QStringLiteral("quickTabSwatch_#FBBF24"));
        REQUIRE(amberSwatch != nullptr);
        REQUIRE(QTest::qWaitFor([&]() { return amberSwatch->x() > 10.0 && amberSwatch->width() > 10.0; }, 1000));
        QTest::mouseClick(&window, Qt::LeftButton, Qt::NoModifier, centerInWindow(amberSwatch));
        QCoreApplication::processEvents();

        INFO("after colorDialog activeTabColor=" << vm.activeTabColor().toStdString());
        CHECK_FALSE(colorDialog->property("opened").toBool());
        CHECK(vm.activeTabColor() == QStringLiteral("#FBBF24"));
        CHECK(cmdInput->hasActiveFocus());
        CHECK(promptRepoText->property("color").toString().compare(QStringLiteral("#fbbf24"), Qt::CaseInsensitive) == 0);
        CHECK(promptBranchChipText->property("color").toString().compare(QStringLiteral("#fbbf24"), Qt::CaseInsensitive) == 0);

        // Switching back to tab 0 restores its emerald color; resetting to default restores #38BDF8
        vm.setActiveTabIndex(0);
        QCoreApplication::processEvents();
        CHECK(vm.activeTabColor() == QStringLiteral("#34D399"));
        CHECK(promptRepoText->property("color").toString().compare(QStringLiteral("#34d399"), Qt::CaseInsensitive) == 0);
        CHECK(promptBranchChipText->property("color").toString().compare(QStringLiteral("#34d399"), Qt::CaseInsensitive) == 0);

        vm.setTabColor(0, QString());
        QCoreApplication::processEvents();
        CHECK(vm.activeTabColor().isEmpty());
        CHECK(promptRepoText->property("color").toString().compare(QStringLiteral("#38bdf8"), Qt::CaseInsensitive) == 0);
        CHECK(promptBranchChipText->property("color").toString().compare(QStringLiteral("#38bdf8"), Qt::CaseInsensitive) == 0);
    }
}
