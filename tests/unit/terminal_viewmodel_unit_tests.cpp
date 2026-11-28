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
}
