#include "desktop/viewmodels/terminal_session_internal.hpp"

#include "core/modules/terminal/bl/command_line_tokenizer.hpp"
#include "core/modules/terminal/bl/git_completion_engine.hpp"
#include "core/shared/utf8_path.hpp"

#include <QMetaObject>

#include <algorithm>
#include <cctype>
#include <chrono>
#include <filesystem>
#include <vector>

namespace gf::desktop
{
    namespace
    {
        [[nodiscard]] gf::terminal::bo::RepoCompletionSnapshot collectRepoSnapshot(
            gf::git::ports::IGitCommandExecutor* executor,
            const std::filesystem::path& repoPath)
        {
            gf::terminal::bo::RepoCompletionSnapshot snap;
            if (!executor || repoPath.empty() || !std::filesystem::exists(repoPath))
            {
                return snap;
            }

            auto runGit = [&](std::vector<std::string> args) -> std::string
            {
                gf::git::bo::GitCommand cmd;
                cmd.args = std::move(args);
                cmd.workingDirectory = repoPath;
                cmd.timeout = std::chrono::milliseconds{3000};
                if (const auto res = executor->execute(cmd); res && res->succeeded())
                {
                    return res->stdoutBytes;
                }
                return {};
            };

            auto forEachLine = [](const std::string_view text, const auto& fn)
            {
                std::size_t pos = 0;
                while (pos < text.size())
                {
                    std::size_t nl = text.find('\n', pos);
                    if (nl == std::string_view::npos) nl = text.size();
                    std::string_view line = text.substr(pos, nl - pos);
                    if (!line.empty() && line.back() == '\r') line.remove_suffix(1);
                    if (!line.empty()) fn(line);
                    pos = (nl < text.size()) ? nl + 1 : nl;
                }
            };

            // 1. Current branch
            {
                const std::string head = runGit({"symbolic-ref", "--short", "HEAD"});
                snap.currentBranch = std::string(trimTerminalSv(head));
            }

            // 2. Local branches, remote branches, tags in a single git invocation
            {
                const std::string refs = runGit({
                    "for-each-ref",
                    "--format=%(refname:short)|%(refname)",
                    "refs/heads",
                    "refs/remotes",
                    "refs/tags"
                });
                forEachLine(refs, [&](const std::string_view line)
                {
                    const std::size_t sep = line.find('|');
                    if (sep == std::string_view::npos) return;
                    const std::string_view shortName = line.substr(0, sep);
                    const std::string_view fullName = line.substr(sep + 1);
                    if (fullName.starts_with("refs/heads/"))
                    {
                        snap.localBranches.emplace_back(shortName);
                    }
                    else if (fullName.starts_with("refs/remotes/"))
                    {
                        if (!shortName.ends_with("/HEAD"))
                        {
                            snap.remoteBranches.emplace_back(shortName);
                        }
                    }
                    else if (fullName.starts_with("refs/tags/"))
                    {
                        snap.tags.emplace_back(shortName);
                    }
                });
            }

            // 3. Remotes
            {
                const std::string remotes = runGit({"remote"});
                forEachLine(remotes, [&](const std::string_view line)
                {
                    const auto t = trimTerminalSv(line);
                    if (!t.empty()) snap.remotes.emplace_back(t);
                });
            }

            // 4. Stashes
            {
                const std::string stashes = runGit({"stash", "list", "--format=%gd|%gs"});
                forEachLine(stashes, [&](const std::string_view line)
                {
                    const std::size_t sep = line.find('|');
                    if (sep == std::string_view::npos) return;
                    snap.stashes.push_back(gf::terminal::bo::RepoStashHint{
                        .selector = std::string(line.substr(0, sep)),
                        .message = std::string(line.substr(sep + 1))
                    });
                });
            }

            // 5. Working tree modified & untracked files
            {
                const std::string status = runGit({"status", "--porcelain=v1", "-uall"});
                forEachLine(status, [&](const std::string_view line)
                {
                    if (line.size() < 4) return;
                    const std::string_view xy = line.substr(0, 2);
                    std::string_view path = line.substr(3);
                    if (const std::size_t arrow = path.find(" -> "); arrow != std::string_view::npos)
                    {
                        path = path.substr(arrow + 4);
                    }
                    if (path.size() >= 2 && path.front() == '"' && path.back() == '"')
                    {
                        path = path.substr(1, path.size() - 2);
                    }
                    if (xy == "??")
                    {
                        snap.untrackedFiles.emplace_back(path);
                    }
                    else
                    {
                        snap.modifiedFiles.emplace_back(path);
                    }
                });
            }

            // 6. Recent commits
            {
                const std::string logOut = runGit({"log", "-n", "15", "--format=%h|%s"});
                forEachLine(logOut, [&](const std::string_view line)
                {
                    const std::size_t sep = line.find('|');
                    if (sep == std::string_view::npos) return;
                    snap.recentCommits.push_back(gf::terminal::bo::RepoCommitHint{
                        .shortSha = std::string(line.substr(0, sep)),
                        .summary = std::string(line.substr(sep + 1))
                    });
                });
            }

            return snap;
        }
    } // namespace

    void TerminalSessionViewModel::warmUpCatalog()
    {
        if (!m_catalogService) return;
        const std::filesystem::path repoPath = gf::core::utf8ToPath(m_activeRepoPath.toStdString());
        auto alive = m_alive;
        auto* catalog = m_catalogService;

        m_bgPool.start([this, alive, catalog, repoPath]()
        {
            catalog->discoverAllCommands(repoPath);
            if (!alive->load(std::memory_order_acquire)) return;
            QMetaObject::invokeMethod(this, [this, alive]()
            {
                if (!alive->load(std::memory_order_acquire)) return;
                if (!m_lastCompletionInput.isEmpty())
                {
                    updateCompletions(m_lastCompletionInput, m_lastCompletionCursor, m_lastCompletionExplicit);
                }
            }, Qt::QueuedConnection);
        });
    }

    bool TerminalSessionViewModel::pagerActive() const noexcept
    {
        const auto* tab = activeTab();
        return tab && tab->pagerActive;
    }

    QString TerminalSessionViewModel::pagerCommand() const
    {
        const auto* tab = activeTab();
        return tab ? tab->pagerCommand : QString{};
    }

    int TerminalSessionViewModel::pagerLineCount() const noexcept
    {
        const auto* tab = activeTab();
        return (tab && tab->pagerModel) ? tab->pagerModel->count() : 0;
    }

    bool TerminalSessionViewModel::hasLastPagerOutput() const noexcept
    {
        const auto* tab = activeTab();
        return tab && tab->pagerModel && tab->pagerModel->count() > 0;
    }

    bool TerminalSessionViewModel::shouldPaginateCommand(const std::string_view commandLine)
    {
        std::vector<std::string> args;
        if (!gf::terminal::bl::CommandLineTokenizer::trySplitSimpleArgs(commandLine, args) || args.size() < 2)
        {
            return false;
        }

        auto toLowerStr = [](const std::string_view sv)
        {
            std::string out;
            out.reserve(sv.size());
            for (const char c : sv)
            {
                out.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
            }
            return out;
        };

        const std::string cmd0 = toLowerStr(args[0]);
        if (cmd0 != "git" && cmd0 != "git.exe")
        {
            return false;
        }

        bool explicitPaginate = false;
        std::size_t subIdx = std::string::npos;
        bool skipNext = false;
        for (std::size_t i = 1; i < args.size(); ++i)
        {
            const std::string_view tok = args[i];
            if (skipNext)
            {
                skipNext = false;
                continue;
            }
            if (tok == "-P" || tok == "--no-pager")
            {
                return false;
            }
            if (tok == "-p" || tok == "--paginate")
            {
                explicitPaginate = true;
                continue;
            }
            if (tok == "-C" || tok == "-c" || tok == "--git-dir" ||
                tok == "--work-tree" || tok == "--namespace" || tok == "--config-env")
            {
                skipNext = true;
                continue;
            }
            if (tok.starts_with('-'))
            {
                continue;
            }
            subIdx = i;
            break;
        }

        if (explicitPaginate)
        {
            return true;
        }
        if (subIdx == std::string::npos)
        {
            return false;
        }

        const std::string_view sub = args[subIdx];
        if (sub == "log" || sub == "diff" || sub == "show" || sub == "reflog" ||
            sub == "shortlog" || sub == "blame" || sub == "whatchanged" ||
            sub == "range-diff" || sub == "grep" || sub == "help")
        {
            return true;
        }
        if (sub == "stash" && subIdx + 1 < args.size())
        {
            const std::string_view action = args[subIdx + 1];
            return action == "list" || action == "show";
        }
        if (sub == "branch" || sub == "tag")
        {
            // Paginate listing operations (skip mutating flags like -d/-D/-m/-M/-c/-C/-a/-s)
            for (std::size_t i = subIdx + 1; i < args.size(); ++i)
            {
                const std::string_view a = args[i];
                if (a == "-d" || a == "-D" || a == "--delete" || a == "-m" || a == "-M" ||
                    a == "--move" || a == "-c" || a == "-C" || a == "--copy" || a == "-s")
                {
                    return false;
                }
            }
            return true;
        }
        return false;
    }

    void TerminalSessionViewModel::closePager()
    {
        auto* tab = activeTab();
        if (!tab || !tab->pagerActive) return;

        tab->pagerActive = false;
        if (tab->isRunning && tab->activeCommandCanPage && tab->activeProcess)
        {
            tab->closedPagerEarly = true;
            tab->activeProcess->terminate();
        }
        emit pagerStateChanged();
        emit focusInputRequested();
    }

    void TerminalSessionViewModel::reopenLastPager()
    {
        auto* tab = activeTab();
        if (!tab || !tab->pagerModel || tab->pagerModel->count() == 0) return;
        tab->pagerActive = true;
        dismissCompletions();
        emit pagerStateChanged();
    }

    QString TerminalSessionViewModel::copyPagerOutput() const
    {
        const auto* tab = activeTab();
        if (!tab || !tab->pagerModel) return {};
        return tab->pagerModel->allPlainText();
    }

    QString TerminalSessionViewModel::historyPrevious(const QString& currentInput)
    {
        auto* tab = activeTab();
        if (!tab || tab->history.isEmpty()) return currentInput;

        if (tab->historyCursor < 0)
        {
            tab->historyDraft = currentInput;
            tab->historyCursor = static_cast<int>(tab->history.size()) - 1;
        }
        else if (tab->historyCursor > 0)
        {
            --tab->historyCursor;
        }
        const QString result = tab->history[tab->historyCursor];
        tab->inputDraft = result;
        tab->inputCursorPos = static_cast<int>(result.size());
        return result;
    }

    QString TerminalSessionViewModel::historyNext()
    {
        auto* tab = activeTab();
        if (!tab || tab->historyCursor < 0) return {};

        if (tab->historyCursor + 1 < static_cast<int>(tab->history.size()))
        {
            ++tab->historyCursor;
            const QString result = tab->history[tab->historyCursor];
            tab->inputDraft = result;
            tab->inputCursorPos = static_cast<int>(result.size());
            return result;
        }
        tab->historyCursor = -1;
        tab->inputDraft = tab->historyDraft;
        tab->inputCursorPos = static_cast<int>(tab->historyDraft.size());
        return tab->historyDraft;
    }

    void TerminalSessionViewModel::resetHistoryNavigation()
    {
        if (auto* tab = activeTab())
        {
            tab->historyCursor = -1;
        }
    }

    void TerminalSessionViewModel::updateCompletions(const QString& inputText,
                                                     const int cursorPos,
                                                     const bool explicitTrigger)
    {
        ++m_completionGeneration;
        m_lastCompletionInput = inputText;
        m_lastCompletionCursor = cursorPos;
        m_lastCompletionExplicit = explicitTrigger;

        if (!m_catalogService)
        {
            dismissCompletions();
            return;
        }

        const QString workDirStr = activeWorkingDir();
        const std::filesystem::path workDir = gf::core::utf8ToPath(workDirStr.toStdString());

        // Map QString UTF-16 cursorPos to UTF-8 byte offset accurately
        const int clampedCursor = std::clamp(cursorPos, 0, static_cast<int>(inputText.size()));
        const std::string utf8Before = inputText.left(clampedCursor).toStdString();
        const std::string utf8Full = inputText.toStdString();
        const std::size_t utf8Cursor = utf8Before.size();

        gf::terminal::bo::RepoCompletionSnapshot emptySnap;
        const gf::terminal::bo::RepoCompletionSnapshot* snapPtr = &emptySnap;
        if (const auto it = m_repoSnapshots.find(m_activeRepoPath.toStdString());
            it != m_repoSnapshots.end() && it->second)
        {
            snapPtr = it->second.get();
        }

        auto result = gf::terminal::bl::GitCompletionEngine::complete(
            utf8Full, utf8Cursor, workDir, *m_catalogService, *snapPtr, explicitTrigger);

        // Convert UTF-8 byte offsets back to UTF-16 QString indices
        const int replaceStart16 = static_cast<int>(
            QString::fromUtf8(utf8Full.data(), static_cast<qsizetype>(result.replaceStart)).size());
        const int replaceEnd16 = static_cast<int>(
            QString::fromUtf8(utf8Full.data(), static_cast<qsizetype>(result.replaceStart + result.replaceLength)).size());

        m_replaceStart = replaceStart16;
        m_replaceLength = std::max(0, replaceEnd16 - replaceStart16);

        // Compute inline ghost text for top completion if cursor is at end of input and top item extends prefix
        QString nextGhost;
        if (!result.items.empty() && clampedCursor == static_cast<int>(inputText.size()))
        {
            const auto& top = result.items.front();
            const QString insertQ = QString::fromStdString(top.insertText);
            const QString prefixQ = QString::fromStdString(result.activePrefix);
            if (insertQ.startsWith(prefixQ, Qt::CaseInsensitive) && insertQ.size() > prefixQ.size())
            {
                nextGhost = insertQ.mid(prefixQ.size());
            }
        }
        m_ghostText = nextGhost;

        const bool needsAsyncLoad = result.needsSubcommandOptionsLoad;
        const QString subcmdToLoad = QString::fromStdString(result.activeSubcommand);

        m_completionModel.setItems(std::move(result.items));
        emit completionsChanged();

        if (needsAsyncLoad && !subcmdToLoad.isEmpty())
        {
            requestSubcommandOptionsAsync(subcmdToLoad);
        }
    }

    void TerminalSessionViewModel::requestSubcommandOptionsAsync(const QString& subcommand)
    {
        if (!m_catalogService || subcommand.isEmpty()) return;
        const std::string subStd = subcommand.toStdString();
        if (!m_loadingOptionsSubcommands.insert(subStd).second)
        {
            return; // already loading
        }

        auto alive = m_alive;
        auto* catalog = m_catalogService;
        const quint64 gen = m_completionGeneration;

        m_bgPool.start([this, alive, catalog, subStd, gen]()
        {
            [[maybe_unused]] auto opts = catalog->loadOptionsFor(subStd);
            if (!alive->load(std::memory_order_acquire)) return;
            QMetaObject::invokeMethod(this, [this, alive, subStd, gen]()
            {
                if (!alive->load(std::memory_order_acquire)) return;
                m_loadingOptionsSubcommands.erase(subStd);
                // Re-run completions if the user is still typing the same or extended command
                if (m_completionGeneration >= gen && !m_lastCompletionInput.isEmpty())
                {
                    updateCompletions(m_lastCompletionInput, m_lastCompletionCursor, m_lastCompletionExplicit);
                }
            }, Qt::QueuedConnection);
        });
    }

    bool TerminalSessionViewModel::shouldEnterAcceptCompletion(const int selectedIndex,
                                                               const bool userNavigated,
                                                               const QString& inputText,
                                                               const int /*cursorPos*/) const
    {
        if (m_completionModel.count() <= 0)
        {
            return false;
        }
        if (selectedIndex < 0 || selectedIndex >= m_completionModel.count())
        {
            return false;
        }

        // Explicit execute action row ("▶ Esegui senza parametri" / "▶ Esegui script"):
        // pressing Enter executes the command immediately!
        if (const auto* selectedItem = m_completionModel.itemAt(selectedIndex);
            selectedItem && selectedItem->kind == gf::terminal::bo::CompletionItemKind::ExecuteAction)
        {
            return false;
        }

        // If the user explicitly moved selection inside the popup with Up/Down arrows, Enter accepts it.
        if (userNavigated)
        {
            return true;
        }

        const int start = std::clamp(m_replaceStart, 0, static_cast<int>(inputText.size()));
        const int len = std::clamp(m_replaceLength, 0, static_cast<int>(inputText.size()) - start);
        const QString activePrefix = inputText.mid(start, len);

        // Trailing space (empty active prefix, e.g. `git log ` after Tab): execute immediately on Enter.
        if (activePrefix.isEmpty())
        {
            return false;
        }

        // If what the user typed already matches any suggested command/flag/ref/path exactly, execute immediately.
        for (int i = 0; i < m_completionModel.count(); ++i)
        {
            const auto* item = m_completionModel.itemAt(i);
            if (!item) continue;
            const QString insertQ = QString::fromStdString(item->insertText);
            const QString displayQ = QString::fromStdString(item->displayText);
            if (activePrefix.compare(insertQ, Qt::CaseInsensitive) == 0 ||
                activePrefix.compare(displayQ, Qt::CaseInsensitive) == 0)
            {
                return false;
            }
        }

        // Incomplete prefix (e.g. `git ch`, `git commit --am`): Smart Enter completes the highlighted suggestion.
        return true;
    }

    QVariantMap TerminalSessionViewModel::acceptCompletion(const int index,
                                                           const QString& inputText,
                                                           const int cursorPos)
    {
        QVariantMap out;
        out[QStringLiteral("accepted")] = false;
        out[QStringLiteral("executeNow")] = false;
        out[QStringLiteral("text")] = inputText;
        out[QStringLiteral("cursorPos")] = cursorPos;

        const auto* item = m_completionModel.itemAt(index);
        if (!item) return out;

        // Clicking or accepting the explicit execute row runs the current command line immediately
        if (item->kind == gf::terminal::bo::CompletionItemKind::ExecuteAction)
        {
            dismissCompletions();
            out[QStringLiteral("accepted")] = true;
            out[QStringLiteral("executeNow")] = true;
            return out;
        }

        const int start = std::clamp(m_replaceStart, 0, static_cast<int>(inputText.size()));
        const int len = std::clamp(m_replaceLength, 0, static_cast<int>(inputText.size()) - start);

        QString replacement = QString::fromStdString(item->insertText);
        const bool shouldAddSpace = item->appendSpace;
        const bool alreadyHasTrailingSpace =
            (start + len < inputText.size()) && inputText.at(start + len) == QLatin1Char(' ');

        if (shouldAddSpace && !alreadyHasTrailingSpace)
        {
            replacement.append(QLatin1Char(' '));
        }

        const QString newText = inputText.left(start) + replacement + inputText.mid(start + len);
        const int newCursor = start + static_cast<int>(replacement.size()) +
                              ((shouldAddSpace && alreadyHasTrailingSpace) ? 1 : 0);

        // Determine if we should immediately chain completions for the next stage
        // (e.g. accepted "git" -> all git commands; accepted script -> "▶ Esegui" + script params;
        //  accepted script param/value -> "▶ Esegui script" + remaining unused params!)
        const bool isScriptFile =
            (item->kind == gf::terminal::bo::CompletionItemKind::FilePath) &&
            (item->categoryLabel == "ps1" || item->categoryLabel == "sh" ||
             item->categoryLabel == "bat" || item->categoryLabel == "py");

        const bool isScriptParamOrValue =
            (item->kind == gf::terminal::bo::CompletionItemKind::GitFlag) &&
            (item->categoryLabel == "param" || item->categoryLabel == "value");

        const bool chainNext =
            (item->kind == gf::terminal::bo::CompletionItemKind::ShellBuiltin && item->insertText == "git") ||
            (item->kind == gf::terminal::bo::CompletionItemKind::GitCommand) ||
            (item->kind == gf::terminal::bo::CompletionItemKind::GitSubcommand) ||
            isScriptFile ||
            isScriptParamOrValue ||
            (item->kind == gf::terminal::bo::CompletionItemKind::FilePath &&
             (item->insertText.ends_with('/') || item->insertText.ends_with('\\')));

        out[QStringLiteral("accepted")] = true;
        out[QStringLiteral("text")] = newText;
        out[QStringLiteral("cursorPos")] = newCursor;

        if (chainNext)
        {
            updateCompletions(newText, newCursor, false);
        }
        else
        {
            dismissCompletions();
        }
        return out;
    }

    void TerminalSessionViewModel::dismissCompletions()
    {
        const bool hadAny = (m_completionModel.count() > 0) || !m_ghostText.isEmpty();
        m_completionModel.clear();
        m_ghostText.clear();
        m_replaceStart = 0;
        m_replaceLength = 0;
        if (hadAny)
        {
            emit completionsChanged();
        }
    }

    void TerminalSessionViewModel::refreshRepoCompletionContext()
    {
        if (!m_gitExecutor || m_activeRepoPath.isEmpty()) return;

        const QString repoPathQ = m_activeRepoPath;
        const std::filesystem::path repoPath = gf::core::utf8ToPath(repoPathQ.toStdString());
        auto alive = m_alive;
        auto* executor = m_gitExecutor;

        m_bgPool.start([this, alive, executor, repoPath, repoPathQ]()
        {
            auto snap = std::make_shared<gf::terminal::bo::RepoCompletionSnapshot>(
                collectRepoSnapshot(executor, repoPath));
            if (!alive->load(std::memory_order_acquire)) return;

            QMetaObject::invokeMethod(this, [this, alive, repoPathQ, snap = std::move(snap)]() mutable
            {
                if (!alive->load(std::memory_order_acquire)) return;
                const QString detectedBranch = QString::fromStdString(snap->currentBranch);
                m_repoSnapshots[repoPathQ.toStdString()] = std::move(snap);
                if (!detectedBranch.isEmpty())
                {
                    updateActiveBranch(repoPathQ, detectedBranch);
                }
            }, Qt::QueuedConnection);
        });
    }
} // namespace gf::desktop
