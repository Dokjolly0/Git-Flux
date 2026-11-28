#include "desktop/viewmodels/terminal_session_viewmodel.hpp"

#include "core/modules/terminal/bl/ansi_parser.hpp"
#include "core/modules/terminal/bl/command_line_tokenizer.hpp"
#include "core/modules/terminal/bl/git_completion_engine.hpp"
#include "core/shared/accent_palette.hpp"
#include "core/shared/utf8_path.hpp"

#include <QCoreApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QMetaObject>
#include <QThread>

#include <algorithm>
#include <cctype>
#include <chrono>

namespace gf::desktop
{
    struct TerminalSessionViewModel::TerminalTabState
    {
        int id = 0;
        QString title;
        QString repoRootPath;
        QString currentWorkingDir;
        QString previousWorkingDir;
        QString branch;
        QString shellId;
        QString accentColor;
        std::unique_ptr<TerminalOutputListModel> outputModel;
        std::unique_ptr<TerminalOutputListModel> pagerModel;
        QStringList history;
        int historyCursor = -1; // -1 means not browsing history
        QString historyDraft;
        QString inputDraft;
        int inputCursorPos = 0;

        bool isRunning = false;
        int activeCommandHeaderId = 0;
        bool activeCommandWasGit = false;
        bool activeCommandCanPage = false;
        bool pagerActive = false;
        bool closedPagerEarly = false;
        QString pagerCommand;
        std::shared_ptr<gf::terminal::ports::ITerminalProcessHandle> activeProcess;

        std::string stdoutPending;
        std::string stderrPending;
        bool stdoutOverwriteLast = false;
        bool stderrOverwriteLast = false;
        gf::terminal::bo::TerminalTextStyle stdoutStyle{};
        gf::terminal::bo::TerminalTextStyle stderrStyle{};
    };

    struct TerminalSessionViewModel::RepoTerminalBucket
    {
        QString repoId;
        QString repoName;
        QString repoPath;
        QString branch;
        std::vector<std::unique_ptr<TerminalTabState>> tabs;
        int activeTabIndex = -1;
        int nextTabNumber = 1;
    };

    namespace
    {
        [[nodiscard]] std::string makeRepoKey(const QString& repoId, const QString& repoPath)
        {
            if (!repoId.isEmpty()) return repoId.toStdString();
            return repoPath.toStdString();
        }

        [[nodiscard]] QString formatDuration(const std::chrono::milliseconds ms)
        {
            if (ms.count() < 1000)
            {
                return QString::number(ms.count()) + QStringLiteral("ms");
            }
            const double sec = static_cast<double>(ms.count()) / 1000.0;
            return QString::number(sec, 'f', sec < 10.0 ? 2 : 1) + QStringLiteral("s");
        }

        [[nodiscard]] std::string_view trimSv(std::string_view sv) noexcept
        {
            while (!sv.empty() && (sv.front() == ' ' || sv.front() == '\t' || sv.front() == '\r' || sv.front() == '\n'))
            {
                sv.remove_prefix(1);
            }
            while (!sv.empty() && (sv.back() == ' ' || sv.back() == '\t' || sv.back() == '\r' || sv.back() == '\n'))
            {
                sv.remove_suffix(1);
            }
            return sv;
        }

        [[nodiscard]] bool hasUnclosedQuote(const std::string_view input) noexcept
        {
            char quote = '\0';
            for (const char c : input)
            {
                if (quote != '\0')
                {
                    if (c == quote)
                    {
                        quote = '\0';
                    }
                    continue;
                }
                if (c == '\'' || c == '"')
                {
                    quote = c;
                }
            }
            return quote != '\0';
        }

        [[nodiscard]] bool looksLikeGitCommand(const QString& trimmed)
        {
            const std::string utf8 = trimmed.toStdString();
            const std::string_view sv(utf8);
            char quote = '\0';
            std::size_t segStart = 0;

            auto segmentStartsWithGit = [](std::string_view segment) -> bool
            {
                segment = trimSv(segment);
                if (segment.empty()) return false;
                std::size_t end = 0;
                while (end < segment.size() &&
                       segment[end] != ' ' && segment[end] != '\t' &&
                       segment[end] != '\r' && segment[end] != '\n')
                {
                    ++end;
                }
                const std::string_view tok = segment.substr(0, end);
                if (tok.size() == 3)
                {
                    return std::tolower(static_cast<unsigned char>(tok[0])) == 'g' &&
                           std::tolower(static_cast<unsigned char>(tok[1])) == 'i' &&
                           std::tolower(static_cast<unsigned char>(tok[2])) == 't';
                }
                if (tok.size() == 7)
                {
                    constexpr std::string_view kGitExe = "git.exe";
                    for (std::size_t i = 0; i < 7; ++i)
                    {
                        if (std::tolower(static_cast<unsigned char>(tok[i])) != kGitExe[i])
                        {
                            return false;
                        }
                    }
                    return true;
                }
                return false;
            };

            for (std::size_t i = 0; i < sv.size(); ++i)
            {
                const char c = sv[i];
                if (quote != '\0')
                {
                    if (c == quote) quote = '\0';
                    continue;
                }
                if (c == '\'' || c == '"')
                {
                    quote = c;
                    continue;
                }
                if (c == ';' || c == '|' || c == '&' || c == '\n' || c == '\r')
                {
                    if (segmentStartsWithGit(sv.substr(segStart, i - segStart)))
                    {
                        return true;
                    }
                    if ((c == '&' || c == '|') && i + 1 < sv.size() && sv[i + 1] == c)
                    {
                        ++i;
                    }
                    segStart = i + 1;
                }
            }
            return segmentStartsWithGit(sv.substr(segStart));
        }

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
                snap.currentBranch = std::string(trimSv(head));
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
                    const auto t = trimSv(line);
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

    TerminalSessionViewModel::TerminalSessionViewModel(
        gf::terminal::ports::ITerminalProcessRunner* processRunner,
        gf::terminal::bl::GitCatalogService* catalogService,
        gf::git::ports::IGitCommandExecutor* gitExecutor,
        QObject* parent)
        : QObject(parent)
        , m_processRunner(processRunner)
        , m_catalogService(catalogService)
        , m_gitExecutor(gitExecutor)
    {
        m_bgPool.setMaxThreadCount(2);
        connect(this, &TerminalSessionViewModel::activeTabChanged,
                this, &TerminalSessionViewModel::activeTabColorChanged);
    }

    TerminalSessionViewModel::~TerminalSessionViewModel()
    {
        m_alive->store(false, std::memory_order_release);

        // Terminate and collect all running processes before waiting
        std::vector<std::shared_ptr<gf::terminal::ports::ITerminalProcessHandle>> runningHandles;
        for (auto& [key, bucket] : m_buckets)
        {
            for (auto& tab : bucket->tabs)
            {
                if (tab->activeProcess)
                {
                    tab->activeProcess->terminate();
                    runningHandles.push_back(std::move(tab->activeProcess));
                }
            }
        }
        for (auto& handle : runningHandles)
        {
            handle->wait();
        }
        m_bgPool.waitForDone();
    }

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
                emit catalogReadyChanged();
                if (!m_lastCompletionInput.isEmpty())
                {
                    updateCompletions(m_lastCompletionInput, m_lastCompletionCursor, m_lastCompletionExplicit);
                }
            }, Qt::QueuedConnection);
        });
    }

    TerminalSessionViewModel::RepoTerminalBucket* TerminalSessionViewModel::activeBucket() noexcept
    {
        if (m_activeRepoKey.empty()) return nullptr;
        const auto it = m_buckets.find(m_activeRepoKey);
        return (it != m_buckets.end()) ? it->second.get() : nullptr;
    }

    const TerminalSessionViewModel::RepoTerminalBucket* TerminalSessionViewModel::activeBucket() const noexcept
    {
        if (m_activeRepoKey.empty()) return nullptr;
        const auto it = m_buckets.find(m_activeRepoKey);
        return (it != m_buckets.end()) ? it->second.get() : nullptr;
    }

    TerminalSessionViewModel::TerminalTabState* TerminalSessionViewModel::activeTab() noexcept
    {
        auto* bucket = activeBucket();
        if (!bucket || bucket->activeTabIndex < 0 ||
            bucket->activeTabIndex >= static_cast<int>(bucket->tabs.size()))
        {
            return nullptr;
        }
        return bucket->tabs[static_cast<std::size_t>(bucket->activeTabIndex)].get();
    }

    const TerminalSessionViewModel::TerminalTabState* TerminalSessionViewModel::activeTab() const noexcept
    {
        const auto* bucket = activeBucket();
        if (!bucket || bucket->activeTabIndex < 0 ||
            bucket->activeTabIndex >= static_cast<int>(bucket->tabs.size()))
        {
            return nullptr;
        }
        return bucket->tabs[static_cast<std::size_t>(bucket->activeTabIndex)].get();
    }

    TerminalSessionViewModel::TerminalTabState* TerminalSessionViewModel::findTabById(const int tabId) noexcept
    {
        for (auto& [key, bucket] : m_buckets)
        {
            for (auto& tab : bucket->tabs)
            {
                if (tab->id == tabId) return tab.get();
            }
        }
        return nullptr;
    }

    QVariantList TerminalSessionViewModel::tabs() const
    {
        QVariantList list;
        const auto* bucket = activeBucket();
        if (!bucket) return list;

        list.reserve(static_cast<qsizetype>(bucket->tabs.size()));
        for (int i = 0; i < static_cast<int>(bucket->tabs.size()); ++i)
        {
            const auto& tab = bucket->tabs[static_cast<std::size_t>(i)];
            QVariantMap m;
            m[QStringLiteral("id")] = tab->id;
            m[QStringLiteral("index")] = i;
            m[QStringLiteral("title")] = tab->title;
            m[QStringLiteral("isRunning")] = tab->isRunning;
            m[QStringLiteral("shellId")] = tab->shellId;
            m[QStringLiteral("accentColor")] = tab->accentColor;
            m[QStringLiteral("workingDir")] = tab->currentWorkingDir;
            m[QStringLiteral("relativeDir")] = formatRelativeDir(tab->repoRootPath, tab->currentWorkingDir);
            list.append(m);
        }
        return list;
    }

    int TerminalSessionViewModel::activeTabIndex() const noexcept
    {
        const auto* bucket = activeBucket();
        return bucket ? bucket->activeTabIndex : -1;
    }

    void TerminalSessionViewModel::setActiveTabIndex(const int index)
    {
        auto* bucket = activeBucket();
        if (!bucket || index < 0 || index >= static_cast<int>(bucket->tabs.size()))
        {
            return;
        }
        if (bucket->activeTabIndex == index)
        {
            return;
        }
        bucket->activeTabIndex = index;
        dismissCompletions();
        emit activeTabChanged();
        emit activeBranchChanged();
        emit pagerStateChanged();
        emit runningStateChanged();
    }

    int TerminalSessionViewModel::activeTabId() const noexcept
    {
        const auto* tab = activeTab();
        return tab ? tab->id : -1;
    }

    QString TerminalSessionViewModel::activeInputDraft() const
    {
        const auto* tab = activeTab();
        return tab ? tab->inputDraft : QString{};
    }

    int TerminalSessionViewModel::activeInputCursorPos() const noexcept
    {
        const auto* tab = activeTab();
        if (!tab) return 0;
        return std::clamp(tab->inputCursorPos, 0, static_cast<int>(tab->inputDraft.size()));
    }

    void TerminalSessionViewModel::setActiveInputDraft(const QString& text, const int cursorPos)
    {
        auto* tab = activeTab();
        if (!tab) return;
        tab->inputDraft = text;
        const int maxPos = static_cast<int>(text.size());
        tab->inputCursorPos = (cursorPos < 0) ? maxPos : std::clamp(cursorPos, 0, maxPos);
    }

    TerminalOutputListModel* TerminalSessionViewModel::activeOutputModel() const noexcept
    {
        const auto* tab = activeTab();
        return (tab && tab->outputModel) ? tab->outputModel.get() : const_cast<TerminalOutputListModel*>(&m_emptyOutputModel);
    }

    TerminalOutputListModel* TerminalSessionViewModel::activePagerModel() const noexcept
    {
        const auto* tab = activeTab();
        return (tab && tab->pagerModel) ? tab->pagerModel.get() : const_cast<TerminalOutputListModel*>(&m_emptyOutputModel);
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

    bool TerminalSessionViewModel::isBuiltinCdCommand(const std::string_view commandLine) noexcept
    {
        const std::string_view trimmed = trimSv(commandLine);
        if (trimmed.size() < 2)
        {
            return false;
        }
        const auto c0 = static_cast<char>(std::tolower(static_cast<unsigned char>(trimmed[0])));
        const auto c1 = static_cast<char>(std::tolower(static_cast<unsigned char>(trimmed[1])));
        if (c0 != 'c' || c1 != 'd')
        {
            return false;
        }
        if (trimmed.size() > 2 && trimmed[2] != ' ' && trimmed[2] != '\t')
        {
            return false;
        }
        if (gf::terminal::bl::CommandLineTokenizer::containsShellOperators(trimmed))
        {
            return false;
        }
        if (hasUnclosedQuote(trimmed))
        {
            return false;
        }
        return true;
    }

    void TerminalSessionViewModel::setIsVisible(const bool visible)
    {
        if (m_isVisible == visible) return;
        m_isVisible = visible;
        if (m_isVisible)
        {
            ensureInitialTabForActiveRepo();
            if (m_catalogService && !m_catalogService->isDiscovered())
            {
                warmUpCatalog();
            }
            refreshRepoCompletionContext();
        }
        else
        {
            dismissCompletions();
        }
        emit visibilityChanged();
    }

    void TerminalSessionViewModel::setIsMaximized(const bool maximized)
    {
        if (m_isMaximized == maximized) return;
        m_isMaximized = maximized;
        emit maximizedChanged();
    }

    bool TerminalSessionViewModel::isRunning() const noexcept
    {
        const auto* tab = activeTab();
        return tab && tab->isRunning;
    }

    bool TerminalSessionViewModel::anyTabRunning() const noexcept
    {
        for (const auto& [key, bucket] : m_buckets)
        {
            for (const auto& tab : bucket->tabs)
            {
                if (tab->isRunning) return true;
            }
        }
        return false;
    }

    QString TerminalSessionViewModel::activeRepoName() const
    {
        return m_activeRepoName;
    }

    QString TerminalSessionViewModel::activeRepoPath() const
    {
        return m_activeRepoPath;
    }

    QString TerminalSessionViewModel::activeWorkingDir() const
    {
        const auto* tab = activeTab();
        return tab ? tab->currentWorkingDir : m_activeRepoPath;
    }

    QString TerminalSessionViewModel::activeRelativeDir() const
    {
        const auto* tab = activeTab();
        if (!tab) return QStringLiteral("/");
        return formatRelativeDir(tab->repoRootPath, tab->currentWorkingDir);
    }

    QString TerminalSessionViewModel::activeBranch() const
    {
        const auto* tab = activeTab();
        if (tab && !tab->branch.isEmpty()) return tab->branch;
        return m_activeBranch;
    }

    QString TerminalSessionViewModel::activeShellId() const
    {
        const auto* tab = activeTab();
        return tab ? tab->shellId : QStringLiteral("auto");
    }

    QString TerminalSessionViewModel::activeTabColor() const
    {
        const auto* tab = activeTab();
        return tab ? tab->accentColor : QString();
    }

    QStringList TerminalSessionViewModel::tabColorPalette() const
    {
        QStringList out;
        out.reserve(static_cast<qsizetype>(gf::core::kAccentPalette.size()));
        for (const std::string_view hex : gf::core::kAccentPalette)
        {
            out.append(QString::fromUtf8(hex.data(), static_cast<qsizetype>(hex.size())));
        }
        return out;
    }

    QVariantList TerminalSessionViewModel::availableShells() const
    {
        QVariantList out;
        if (!m_processRunner) return out;
        for (const auto& s : m_processRunner->availableShells())
        {
            QVariantMap m;
            m[QStringLiteral("id")] = QString::fromStdString(s.id);
            m[QStringLiteral("displayName")] = QString::fromStdString(s.displayName);
            out.append(m);
        }
        return out;
    }

    int TerminalSessionViewModel::discoveredCommandCount() const
    {
        if (!m_catalogService) return 0;
        return static_cast<int>(m_catalogService->allCommands().size());
    }

    void TerminalSessionViewModel::toggleVisible()
    {
        setIsVisible(!m_isVisible);
        if (m_isVisible)
        {
            emit focusInputRequested();
        }
    }

    void TerminalSessionViewModel::openAndFocus()
    {
        if (!m_isVisible)
        {
            setIsVisible(true);
        }
        else
        {
            ensureInitialTabForActiveRepo();
        }
        emit focusInputRequested();
    }

    void TerminalSessionViewModel::setActiveRepository(const QString& repoId,
                                                       const QString& repoName,
                                                       const QString& repoPath,
                                                       const QString& branch)
    {
        const std::string key = makeRepoKey(repoId, repoPath);
        const bool repoChanged = (key != m_activeRepoKey || repoPath != m_activeRepoPath);

        m_activeRepoKey = key;
        m_activeRepoId = repoId;
        m_activeRepoName = repoName;
        m_activeRepoPath = repoPath;
        if (!branch.isEmpty())
        {
            m_activeBranch = branch;
        }
        else if (repoChanged)
        {
            m_activeBranch.clear();
        }

        if (!m_activeRepoKey.empty() && !m_activeRepoPath.isEmpty())
        {
            auto& bucketPtr = m_buckets[m_activeRepoKey];
            if (!bucketPtr)
            {
                bucketPtr = std::make_unique<RepoTerminalBucket>();
            }
            bucketPtr->repoId = repoId;
            bucketPtr->repoName = repoName;
            bucketPtr->repoPath = repoPath;
            if (!m_activeBranch.isEmpty())
            {
                bucketPtr->branch = m_activeBranch;
                for (auto& tab : bucketPtr->tabs)
                {
                    tab->branch = m_activeBranch;
                }
            }
            if (m_isVisible && bucketPtr->tabs.empty())
            {
                createTab();
            }
        }

        if (repoChanged)
        {
            dismissCompletions();
            if (!m_activeRepoPath.isEmpty() && m_catalogService)
            {
                const std::filesystem::path rp = gf::core::utf8ToPath(m_activeRepoPath.toStdString());
                auto alive = m_alive;
                auto* catalog = m_catalogService;
                m_bgPool.start([this, alive, catalog, rp]()
                {
                    if (!catalog->isDiscovered())
                    {
                        catalog->discoverAllCommands(rp);
                    }
                    else
                    {
                        catalog->refreshAliases(rp);
                    }
                    if (!alive->load(std::memory_order_acquire)) return;
                    QMetaObject::invokeMethod(this, [this, alive]()
                    {
                        if (!alive->load(std::memory_order_acquire)) return;
                        emit catalogReadyChanged();
                    }, Qt::QueuedConnection);
                });
            }
            if (m_isVisible)
            {
                refreshRepoCompletionContext();
            }
        }

        emit tabsChanged();
        emit activeTabChanged();
        emit activeBranchChanged();
        emit pagerStateChanged();
        emit runningStateChanged();
    }

    void TerminalSessionViewModel::updateActiveBranch(const QString& repoPath, const QString& branch)
    {
        if (repoPath.isEmpty()) return;
        bool activeBranchDidChange = false;
        for (auto& [key, bucket] : m_buckets)
        {
            if (bucket->repoPath == repoPath)
            {
                if (bucket->branch != branch)
                {
                    bucket->branch = branch;
                    if (key == m_activeRepoKey)
                    {
                        activeBranchDidChange = true;
                    }
                }
                for (auto& tab : bucket->tabs)
                {
                    if (tab->branch != branch)
                    {
                        tab->branch = branch;
                        if (key == m_activeRepoKey)
                        {
                            activeBranchDidChange = true;
                        }
                    }
                }
                if (key == m_activeRepoKey && m_activeBranch != branch)
                {
                    m_activeBranch = branch;
                    activeBranchDidChange = true;
                }
            }
        }
        if (repoPath == m_activeRepoPath && m_activeBranch != branch)
        {
            m_activeBranch = branch;
            activeBranchDidChange = true;
        }
        if (activeBranchDidChange)
        {
            emit activeBranchChanged();
        }
    }

    void TerminalSessionViewModel::closeRepositoryTerminals(const QString& repoId, const QString& repoPath)
    {
        const std::string key = makeRepoKey(repoId, repoPath);
        const auto it = m_buckets.find(key);
        if (it == m_buckets.end()) return;

        for (auto& tab : it->second->tabs)
        {
            if (tab->activeProcess)
            {
                tab->activeProcess->terminate();
            }
        }
        m_buckets.erase(it);
        m_repoSnapshots.erase(repoPath.toStdString());

        if (key == m_activeRepoKey)
        {
            m_activeRepoKey.clear();
            m_activeRepoId.clear();
            m_activeRepoName.clear();
            m_activeRepoPath.clear();
            m_activeBranch.clear();
            dismissCompletions();
            emit tabsChanged();
            emit activeTabChanged();
        }
        emit runningStateChanged();
    }

    void TerminalSessionViewModel::closeAllTerminals()
    {
        for (auto& [key, bucket] : m_buckets)
        {
            for (auto& tab : bucket->tabs)
            {
                if (tab->activeProcess)
                {
                    tab->activeProcess->terminate();
                }
            }
        }
        m_buckets.clear();
        m_repoSnapshots.clear();
        m_activeRepoKey.clear();
        m_activeRepoId.clear();
        m_activeRepoName.clear();
        m_activeRepoPath.clear();
        m_activeBranch.clear();
        dismissCompletions();
        emit tabsChanged();
        emit activeTabChanged();
        emit activeBranchChanged();
        emit pagerStateChanged();
        emit runningStateChanged();
    }

    void TerminalSessionViewModel::ensureInitialTabForActiveRepo()
    {
        if (m_activeRepoKey.empty() || m_activeRepoPath.isEmpty()) return;
        auto& bucketPtr = m_buckets[m_activeRepoKey];
        if (!bucketPtr)
        {
            bucketPtr = std::make_unique<RepoTerminalBucket>();
            bucketPtr->repoId = m_activeRepoId;
            bucketPtr->repoName = m_activeRepoName;
            bucketPtr->repoPath = m_activeRepoPath;
            bucketPtr->branch = m_activeBranch;
        }
        if (bucketPtr->tabs.empty())
        {
            createTab();
        }
    }

    int TerminalSessionViewModel::createTab(const QString& shellId)
    {
        if (m_activeRepoKey.empty() || m_activeRepoPath.isEmpty())
        {
            return -1;
        }

        auto& bucketPtr = m_buckets[m_activeRepoKey];
        if (!bucketPtr)
        {
            bucketPtr = std::make_unique<RepoTerminalBucket>();
            bucketPtr->repoId = m_activeRepoId;
            bucketPtr->repoName = m_activeRepoName;
            bucketPtr->repoPath = m_activeRepoPath;
            bucketPtr->branch = m_activeBranch;
        }

        QString resolvedShell = shellId;
        if (resolvedShell.isEmpty() && m_processRunner)
        {
            const auto shells = m_processRunner->availableShells();
            if (!shells.empty())
            {
                resolvedShell = QString::fromStdString(shells.front().id);
            }
        }
        if (resolvedShell.isEmpty())
        {
            resolvedShell = QStringLiteral("auto");
        }

        auto tab = std::make_unique<TerminalTabState>();
        tab->id = m_nextTabId++;
        tab->title = QStringLiteral("Terminal %1").arg(bucketPtr->nextTabNumber++);
        tab->repoRootPath = QDir::cleanPath(m_activeRepoPath);
        tab->currentWorkingDir = tab->repoRootPath;
        tab->branch = m_activeBranch;
        tab->shellId = resolvedShell;
        tab->outputModel = std::make_unique<TerminalOutputListModel>();
        tab->pagerModel = std::make_unique<TerminalOutputListModel>(
            nullptr, TerminalOutputListModel::kMaxPagerLines, false);

        const QString welcome = QStringLiteral("GitFlux CLI • Root: %1  (digita `git ` per l'autocompletamento di tutti i comandi)")
                                    .arg(QDir::toNativeSeparators(tab->repoRootPath));
        tab->outputModel->appendLine(QStringLiteral("system"), welcome.toHtmlEscaped(), welcome);

        const int newIdx = static_cast<int>(bucketPtr->tabs.size());
        bucketPtr->tabs.push_back(std::move(tab));
        bucketPtr->activeTabIndex = newIdx;

        emit tabsChanged();
        emit activeTabChanged();
        emit activeBranchChanged();
        emit pagerStateChanged();
        emit runningStateChanged();
        return newIdx;
    }

    void TerminalSessionViewModel::closeTab(const int index)
    {
        auto* bucket = activeBucket();
        if (!bucket || index < 0 || index >= static_cast<int>(bucket->tabs.size()))
        {
            return;
        }

        if (bucket->tabs[static_cast<std::size_t>(index)]->activeProcess)
        {
            bucket->tabs[static_cast<std::size_t>(index)]->activeProcess->terminate();
        }

        bucket->tabs.erase(bucket->tabs.begin() + index);

        if (bucket->tabs.empty())
        {
            bucket->activeTabIndex = -1;
            setIsVisible(false);
        }
        else if (index < bucket->activeTabIndex)
        {
            --bucket->activeTabIndex;
        }
        else if (bucket->activeTabIndex >= static_cast<int>(bucket->tabs.size()))
        {
            bucket->activeTabIndex = static_cast<int>(bucket->tabs.size()) - 1;
        }

        dismissCompletions();
        emit tabsChanged();
        emit activeTabChanged();
        emit activeBranchChanged();
        emit pagerStateChanged();
        emit runningStateChanged();
    }

    void TerminalSessionViewModel::renameTab(const int index, const QString& newTitle)
    {
        auto* bucket = activeBucket();
        const QString trimmed = newTitle.trimmed();
        if (!bucket || trimmed.isEmpty() || index < 0 || index >= static_cast<int>(bucket->tabs.size()))
        {
            return;
        }
        bucket->tabs[static_cast<std::size_t>(index)]->title = trimmed;
        emit tabsChanged();
    }

    void TerminalSessionViewModel::setTabColor(const int index, const QString& hexColor)
    {
        auto* bucket = activeBucket();
        if (!bucket || index < 0 || index >= static_cast<int>(bucket->tabs.size()))
        {
            return;
        }
        const QString trimmed = hexColor.trimmed();
        if (!gf::core::isValidHexColor(trimmed.toStdString(), true))
        {
            return;
        }
        auto& tab = bucket->tabs[static_cast<std::size_t>(index)];
        if (tab->accentColor.compare(trimmed, Qt::CaseInsensitive) == 0)
        {
            return;
        }
        tab->accentColor = trimmed;
        emit tabsChanged();
        if (index == bucket->activeTabIndex)
        {
            emit activeTabColorChanged();
        }
    }

    void TerminalSessionViewModel::setActiveTabShell(const QString& shellId)
    {
        auto* tab = activeTab();
        if (!tab || shellId.isEmpty() || tab->shellId == shellId) return;
        tab->shellId = shellId;
        emit tabsChanged();
        emit activeTabChanged();
    }

    QString TerminalSessionViewModel::formatRelativeDir(const QString& repoRoot, const QString& currentDir) const
    {
        const QString cleanRoot = QDir::cleanPath(repoRoot);
        const QString cleanCur = QDir::cleanPath(currentDir);
        if (cleanRoot.isEmpty() || cleanCur.compare(cleanRoot, Qt::CaseInsensitive) == 0)
        {
            return QStringLiteral("/");
        }
        QDir rootDir(cleanRoot);
        const QString rel = rootDir.relativeFilePath(cleanCur);
        if (!rel.startsWith(QStringLiteral("..")))
        {
            return QStringLiteral("/") + rel;
        }
        return QDir::toNativeSeparators(cleanCur);
    }

    QString TerminalSessionViewModel::buildPromptPrefix(const TerminalTabState& tab) const
    {
        const QString rel = formatRelativeDir(tab.repoRootPath, tab.currentWorkingDir);
        const QString br = tab.branch.isEmpty() ? m_activeBranch : tab.branch;
        QString prefix = m_activeRepoName.isEmpty() ? QFileInfo(tab.repoRootPath).fileName() : m_activeRepoName;
        if (!br.isEmpty())
        {
            prefix += QStringLiteral(" (") + br + QStringLiteral(")");
        }
        if (rel != QStringLiteral("/"))
        {
            prefix += QStringLiteral(" ") + rel;
        }
        prefix += QStringLiteral(" ❯");
        return prefix;
    }

    void TerminalSessionViewModel::handleBuiltinCd(TerminalTabState& tab, const QString& targetArg, const int headerId)
    {
        QString target = targetArg.trimmed();
        if (target.size() >= 2 &&
            ((target.front() == QLatin1Char('"') && target.back() == QLatin1Char('"')) ||
             (target.front() == QLatin1Char('\'') && target.back() == QLatin1Char('\''))))
        {
            target = target.mid(1, target.size() - 2);
        }

        QString destination;
        if (target.isEmpty() || target == QStringLiteral("~"))
        {
            destination = tab.repoRootPath;
        }
        else if (target == QStringLiteral("-"))
        {
            destination = tab.previousWorkingDir.isEmpty() ? tab.repoRootPath : tab.previousWorkingDir;
        }
        else
        {
            QDir baseDir(tab.currentWorkingDir);
            destination = QDir::cleanPath(baseDir.filePath(target));
        }

        const QFileInfo info(destination);
        if (info.exists() && info.isDir())
        {
            tab.previousWorkingDir = tab.currentWorkingDir;
            tab.currentWorkingDir = info.canonicalFilePath().isEmpty() ? destination : info.canonicalFilePath();
            tab.outputModel->finishCommandHeader(headerId, 0, QStringLiteral("0ms"), false);
            emit tabsChanged();
            emit activeTabChanged();
        }
        else
        {
            const QString errMsg = QStringLiteral("cd: directory non trovata: %1").arg(destination);
            tab.outputModel->appendLine(QStringLiteral("stderr"), errMsg.toHtmlEscaped(), errMsg);
            tab.outputModel->finishCommandHeader(headerId, 1, QStringLiteral("0ms"), false);
        }
        emit outputAppended();
    }

    void TerminalSessionViewModel::executeCommand(const QString& commandLine)
    {
        dismissCompletions();
        resetHistoryNavigation();

        ensureInitialTabForActiveRepo();
        auto* tab = activeTab();
        if (!tab) return;

        tab->inputDraft.clear();
        tab->inputCursorPos = 0;

        // If a process is already running in this tab, forward the line to its stdin
        if (tab->isRunning && tab->activeProcess)
        {
            sendStdinLine(commandLine);
            return;
        }

        const QString trimmed = commandLine.trimmed();
        if (trimmed.isEmpty())
        {
            return;
        }

        // Record in command history (avoid consecutive duplicates)
        if (tab->history.isEmpty() || tab->history.back() != trimmed)
        {
            tab->history.append(trimmed);
            if (tab->history.size() > 500)
            {
                tab->history.removeFirst();
            }
        }

        // Built-in commands handled instantaneously in-process
        const QString lower = trimmed.toLower();
        if (lower == QStringLiteral("clear") || lower == QStringLiteral("cls"))
        {
            clearActiveOutput();
            return;
        }
        if (lower == QStringLiteral("exit") || lower == QStringLiteral("quit"))
        {
            closeTab(activeTabIndex());
            return;
        }

        const int headerId = tab->outputModel->appendCommandHeader(buildPromptPrefix(*tab), trimmed);
        emit outputAppended();

        if (lower == QStringLiteral("pwd"))
        {
            const QString p = QDir::toNativeSeparators(tab->currentWorkingDir);
            tab->outputModel->appendLine(QStringLiteral("stdout"), p.toHtmlEscaped(), p);
            tab->outputModel->finishCommandHeader(headerId, 0, QStringLiteral("0ms"), false);
            emit outputAppended();
            return;
        }

        if (isBuiltinCdCommand(trimmed.toStdString()))
        {
            const QString arg = trimmed.mid(2).trimmed();
            handleBuiltinCd(*tab, arg, headerId);
            return;
        }

        if (!m_processRunner)
        {
            const QString err = QStringLiteral("Runner terminale non configurato.");
            tab->outputModel->appendLine(QStringLiteral("stderr"), err.toHtmlEscaped(), err);
            tab->outputModel->finishCommandHeader(headerId, 1, QStringLiteral("0ms"), false);
            emit outputAppended();
            return;
        }

        constexpr int kPagerLineThreshold = 28;
        const bool canPage = shouldPaginateCommand(trimmed.toStdString());

        tab->isRunning = true;
        tab->activeCommandHeaderId = headerId;
        tab->activeCommandWasGit = looksLikeGitCommand(trimmed);
        tab->activeCommandCanPage = canPage;
        tab->closedPagerEarly = false;
        if (canPage)
        {
            tab->pagerModel->clear();
            tab->pagerCommand = trimmed;
            if (tab->pagerActive)
            {
                tab->pagerActive = false;
            }
            emit pagerStateChanged();
        }
        tab->stdoutPending.clear();
        tab->stderrPending.clear();
        tab->stdoutOverwriteLast = false;
        tab->stderrOverwriteLast = false;
        tab->stdoutStyle = {};
        tab->stderrStyle = {};

        emit tabsChanged();
        emit runningStateChanged();

        gf::terminal::bo::TerminalExecutionRequest req;
        req.commandLine = trimmed.toStdString();
        req.workingDirectory = gf::core::utf8ToPath(tab->currentWorkingDir.toStdString());
        req.shellKind = gf::terminal::bo::shellKindFromId(tab->shellId.toStdString());
        req.forceGitColor = true;

        const int tabId = tab->id;
        const QString repoPath = tab->repoRootPath;
        const bool wasGit = tab->activeCommandWasGit;
        auto alive = m_alive;

        gf::terminal::bo::TerminalExecutionCallbacks callbacks;
        callbacks.onOutput = [this, alive, tabId](const gf::terminal::bo::TerminalOutputStream stream,
                                                  const std::string_view chunk)
        {
            if (!alive->load(std::memory_order_acquire) || chunk.empty()) return;
            QByteArray bytes(chunk.data(), static_cast<qsizetype>(chunk.size()));
            const int streamInt = (stream == gf::terminal::bo::TerminalOutputStream::Stderr) ? 1 : 0;
            QMetaObject::invokeMethod(this, [this, alive, tabId, streamInt, bytes = std::move(bytes)]()
            {
                if (!alive->load(std::memory_order_acquire)) return;
                if (auto* t = findTabById(tabId))
                {
                    appendStreamChunk(*t, streamInt, bytes);
                }
            }, Qt::QueuedConnection);
        };

        callbacks.onFinished = [this, alive, tabId, headerId, repoPath, wasGit](
                                   const gf::terminal::bo::TerminalCommandResult& res)
        {
            if (!alive->load(std::memory_order_acquire)) return;
            const int exitCode = res.exitCode;
            const auto duration = res.duration;
            const bool cancelled = res.wasCancelled;
            QString newWorkingDir;
            if (!cancelled && res.finalWorkingDirectory && !res.finalWorkingDirectory->empty())
            {
                newWorkingDir = QString::fromStdString(gf::core::pathToUtf8(*res.finalWorkingDirectory));
            }
            QMetaObject::invokeMethod(this, [this, alive, tabId, headerId, repoPath, wasGit,
                                             exitCode, duration, cancelled, newWorkingDir = std::move(newWorkingDir)]()
            {
                if (!alive->load(std::memory_order_acquire)) return;
                bool emitPagerUpdate = false;
                bool shouldEmitOutputAppended = true;
                bool workingDirChanged = false;
                if (auto* t = findTabById(tabId))
                {
                    flushPendingBuffers(*t);

                    if (!newWorkingDir.isEmpty())
                    {
                        const QFileInfo dirInfo(newWorkingDir);
                        if (dirInfo.exists() && dirInfo.isDir())
                        {
                            const QString resolved = dirInfo.canonicalFilePath().isEmpty()
                                                         ? QDir::cleanPath(newWorkingDir)
                                                         : dirInfo.canonicalFilePath();
#ifdef _WIN32
                            constexpr Qt::CaseSensitivity kPathCs = Qt::CaseInsensitive;
#else
                            constexpr Qt::CaseSensitivity kPathCs = Qt::CaseSensitive;
#endif
                            if (QDir::cleanPath(t->currentWorkingDir).compare(resolved, kPathCs) != 0)
                            {
                                t->previousWorkingDir = t->currentWorkingDir;
                                t->currentWorkingDir = resolved;
                                workingDirChanged = true;
                            }
                        }
                    }

                    const bool closedEarly = t->closedPagerEarly;
                    const bool effectiveCancelled = cancelled && !closedEarly;
                    const int effectiveExitCode = closedEarly ? 0 : exitCode;

                    if (t->activeCommandCanPage)
                    {
                        const int pagedCount = t->pagerModel->count();
                        if (!t->pagerActive && !closedEarly && pagedCount <= kPagerLineThreshold)
                        {
                            // Short output (less -FRX behavior): print directly inline in terminal scrollback
                            t->pagerModel->copyLinesTo(*t->outputModel);
                            t->pagerModel->clear();
                            t->pagerCommand.clear();
                        }
                        else if (pagedCount > 0)
                        {
                            if (!closedEarly && !t->pagerActive)
                            {
                                t->pagerActive = true;
                            }
                            const QString summary = QStringLiteral("📄 Output nel pager (%1 righe) — usa \"Riapri pager\" nella barra in alto per consultarlo di nuovo")
                                                        .arg(pagedCount);
                            t->outputModel->appendLine(QStringLiteral("system"), summary.toHtmlEscaped(), summary);
                            shouldEmitOutputAppended = !t->pagerActive;
                        }
                        t->activeCommandCanPage = false;
                        emitPagerUpdate = true;
                    }

                    if (effectiveCancelled)
                    {
                        const QString msg = QStringLiteral("^C (comando interrotto)");
                        t->outputModel->appendLine(QStringLiteral("system"), msg.toHtmlEscaped(), msg);
                    }
                    t->outputModel->finishCommandHeader(headerId, effectiveExitCode, formatDuration(duration), effectiveCancelled);
                    t->isRunning = false;
                    t->activeProcess.reset();
                }
                emit tabsChanged();
                if (workingDirChanged && activeTab() && activeTab()->id == tabId)
                {
                    emit activeTabChanged();
                }
                emit runningStateChanged();
                if (emitPagerUpdate)
                {
                    emit pagerStateChanged();
                }
                if (shouldEmitOutputAppended)
                {
                    emit outputAppended();
                }

                if (wasGit && !repoPath.isEmpty())
                {
                    refreshRepoCompletionContext();
                    emit gitCommandFinished(repoPath, exitCode);
                }
            }, Qt::QueuedConnection);
        };

        auto started = m_processRunner->start(req, std::move(callbacks));
        if (!started)
        {
            const QString err = QString::fromStdString(started.error());
            tab->outputModel->appendLine(QStringLiteral("stderr"), err.toHtmlEscaped(), err);
            tab->outputModel->finishCommandHeader(headerId, 1, QStringLiteral("0ms"), false);
            tab->isRunning = false;
            tab->activeCommandCanPage = false;
            emit tabsChanged();
            emit runningStateChanged();
            emit outputAppended();
            return;
        }

        tab->activeProcess = std::move(*started);
    }

    void TerminalSessionViewModel::appendStreamChunk(TerminalTabState& tab,
                                                     const int streamKind,
                                                     const QByteArray& utf8Bytes)
    {
        constexpr int kPagerLineThreshold = 28;
        const bool isStderr = (streamKind == 1);
        const bool routeToPager = (!isStderr && tab.activeCommandCanPage);
        if (routeToPager && tab.closedPagerEarly)
        {
            return;
        }

        TerminalOutputListModel* targetModel = routeToPager ? tab.pagerModel.get() : tab.outputModel.get();
        std::string& pending = isStderr ? tab.stderrPending : tab.stdoutPending;
        bool& overwriteLast = isStderr ? tab.stderrOverwriteLast : tab.stdoutOverwriteLast;
        auto& style = isStderr ? tab.stderrStyle : tab.stdoutStyle;
        const QString kindStr = isStderr ? QStringLiteral("stderr") : QStringLiteral("stdout");

        pending.append(utf8Bytes.constData(), static_cast<std::size_t>(utf8Bytes.size()));

        std::size_t pos = 0;
        while (true)
        {
            const std::size_t nl = pending.find('\n', pos);
            if (nl == std::string::npos) break;

            const std::string_view rawLine(pending.data() + pos, nl - pos);
            const auto rendered = gf::terminal::bl::AnsiParser::parseLine(rawLine, style);
            const QString html = QString::fromStdString(rendered.richHtml);
            const QString plain = QString::fromStdString(rendered.plainText);

            if (overwriteLast)
            {
                targetModel->upsertProgressLine(kindStr, html, plain);
                overwriteLast = false;
            }
            else
            {
                targetModel->appendLine(kindStr, html, plain);
            }
            pos = nl + 1;
        }

        if (pos > 0)
        {
            pending.erase(0, pos);
        }

        // Handle live carriage-return progress updates (e.g. `Receiving objects: 42%\r`) before a newline arrives
        if (!pending.empty() && pending.find('\r') != std::string::npos)
        {
            auto previewStyle = style;
            const auto rendered = gf::terminal::bl::AnsiParser::parseLine(pending, previewStyle);
            const QString html = QString::fromStdString(rendered.richHtml);
            const QString plain = QString::fromStdString(rendered.plainText);
            if (!plain.isEmpty())
            {
                if (overwriteLast)
                {
                    targetModel->upsertProgressLine(kindStr, html, plain);
                }
                else
                {
                    targetModel->appendLine(kindStr, html, plain);
                    overwriteLast = true;
                }
            }
            // Keep only the tail after the last '\r'
            const std::size_t lastCr = pending.rfind('\r');
            if (lastCr != std::string::npos && lastCr + 1 < pending.size())
            {
                pending.erase(0, lastCr + 1);
            }
            else
            {
                pending.clear();
            }
        }

        if (routeToPager)
        {
            if (!tab.pagerActive && tab.pagerModel->count() > kPagerLineThreshold)
            {
                tab.pagerActive = true;
            }
            if (tab.pagerActive)
            {
                emit pagerStateChanged();
            }
        }
        else
        {
            emit outputAppended();
        }
    }

    void TerminalSessionViewModel::flushPendingBuffers(TerminalTabState& tab)
    {
        auto flushOne = [&](std::string& pending, bool& overwriteLast,
                            gf::terminal::bo::TerminalTextStyle& style, const QString& kindStr,
                            TerminalOutputListModel* targetModel)
        {
            if (pending.empty() || !targetModel) return;
            const auto rendered = gf::terminal::bl::AnsiParser::parseLine(pending, style);
            const QString html = QString::fromStdString(rendered.richHtml);
            const QString plain = QString::fromStdString(rendered.plainText);
            if (overwriteLast)
            {
                targetModel->upsertProgressLine(kindStr, html, plain);
                overwriteLast = false;
            }
            else
            {
                targetModel->appendLine(kindStr, html, plain);
            }
            pending.clear();
        };

        TerminalOutputListModel* stdoutTarget = (tab.activeCommandCanPage && !tab.closedPagerEarly)
                                                    ? tab.pagerModel.get()
                                                    : tab.outputModel.get();
        flushOne(tab.stdoutPending, tab.stdoutOverwriteLast, tab.stdoutStyle, QStringLiteral("stdout"), stdoutTarget);
        flushOne(tab.stderrPending, tab.stderrOverwriteLast, tab.stderrStyle, QStringLiteral("stderr"), tab.outputModel.get());
    }

    void TerminalSessionViewModel::sendStdinLine(const QString& text)
    {
        auto* tab = activeTab();
        if (!tab || !tab->isRunning || !tab->activeProcess) return;
        tab->inputDraft.clear();
        tab->inputCursorPos = 0;
        const std::string payload = text.toStdString() + "\n";
        tab->activeProcess->writeStdin(payload);
        tab->outputModel->appendLine(QStringLiteral("system"),
                                     (QStringLiteral("› ") + text).toHtmlEscaped(),
                                     QStringLiteral("› ") + text);
        emit outputAppended();
    }

    void TerminalSessionViewModel::interruptRunningCommand()
    {
        auto* tab = activeTab();
        if (!tab || !tab->isRunning || !tab->activeProcess) return;
        tab->activeProcess->terminate();
    }

    void TerminalSessionViewModel::clearActiveOutput()
    {
        auto* tab = activeTab();
        if (!tab || !tab->outputModel) return;
        tab->outputModel->clear();
        if (tab->pagerActive)
        {
            closePager();
        }
    }

    QString TerminalSessionViewModel::copyActiveOutput() const
    {
        const auto* tab = activeTab();
        if (!tab || !tab->outputModel) return {};
        return tab->outputModel->allPlainText();
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

    bool TerminalSessionViewModel::waitForIdle(const int timeoutMs)
    {
        QElapsedTimer timer;
        timer.start();
        while (timer.elapsed() < timeoutMs)
        {
            QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
            if (!anyTabRunning() && m_bgPool.activeThreadCount() == 0)
            {
                // One extra pass to drain queued invocations posted just as worker finished
                QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
                if (!anyTabRunning() && m_bgPool.activeThreadCount() == 0)
                {
                    return true;
                }
            }
            QThread::msleep(10);
        }
        return !anyTabRunning() && m_bgPool.activeThreadCount() == 0;
    }
} // namespace gf::desktop
