#include "desktop/viewmodels/terminal_session_internal.hpp"
#include "desktop/services/desktop_viewmodel_utils.hpp"

#include "core/modules/terminal/bl/ansi_parser.hpp"
#include "core/modules/terminal/bl/command_line_tokenizer.hpp"
#include "core/shared/accent_palette.hpp"
#include "core/shared/utf8_path.hpp"

#include <QDir>
#include <QFileInfo>
#include <QMetaObject>

#include <algorithm>
#include <cctype>
#include <chrono>

namespace gf::desktop
{
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
                segment = trimTerminalSv(segment);
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

    bool TerminalSessionViewModel::isBuiltinCdCommand(const std::string_view commandLine) noexcept
    {
        const std::string_view trimmed = trimTerminalSv(commandLine);
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

    void TerminalSessionViewModel::toggleVisible()
    {
        setIsVisible(!m_isVisible);
        if (m_isVisible)
        {
            emit focusInputRequested();
        }
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

    int TerminalSessionViewModel::prepareExternalCommandHeader(TerminalTabState& tab, const QString& trimmed)
    {
        const QString lower = trimmed.toLower();
        if (lower == QStringLiteral("clear") || lower == QStringLiteral("cls"))
        {
            clearActiveOutput();
            return -1;
        }
        if (lower == QStringLiteral("exit") || lower == QStringLiteral("quit"))
        {
            closeTab(activeTabIndex());
            return -1;
        }

        const int headerId = tab.outputModel->appendCommandHeader(buildPromptPrefix(tab), trimmed);
        emit outputAppended();

        if (lower == QStringLiteral("pwd"))
        {
            const QString p = QDir::toNativeSeparators(tab.currentWorkingDir);
            tab.outputModel->appendLine(QStringLiteral("stdout"), p.toHtmlEscaped(), p);
            tab.outputModel->finishCommandHeader(headerId, 0, QStringLiteral("0ms"), false);
            emit outputAppended();
            return -1;
        }

        if (isBuiltinCdCommand(trimmed.toStdString()))
        {
            handleBuiltinCd(tab, trimmed.mid(2).trimmed(), headerId);
            return -1;
        }

        if (!m_processRunner)
        {
            const QString err = QStringLiteral("Runner terminale non configurato.");
            tab.outputModel->appendLine(QStringLiteral("stderr"), err.toHtmlEscaped(), err);
            tab.outputModel->finishCommandHeader(headerId, 1, QStringLiteral("0ms"), false);
            emit outputAppended();
            return -1;
        }

        return headerId;
    }

    void TerminalSessionViewModel::handleProcessFinished(const int tabId,
                                                         const int headerId,
                                                         const QString& repoPath,
                                                         const bool wasGit,
                                                         const int exitCode,
                                                         const qint64 durationMs,
                                                         const bool cancelled,
                                                         const QString& newWorkingDir)
    {
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
                if (!t->pagerActive && !closedEarly && pagedCount <= kTerminalPagerLineThreshold)
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
            t->outputModel->finishCommandHeader(
                headerId, effectiveExitCode, formatDuration(std::chrono::milliseconds{durationMs}), effectiveCancelled);
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
    }

    void TerminalSessionViewModel::launchExternalCommand(TerminalTabState& tab,
                                                         const int headerId,
                                                         const QString& trimmed)
    {
        const bool canPage = shouldPaginateCommand(trimmed.toStdString());

        tab.isRunning = true;
        tab.activeCommandHeaderId = headerId;
        tab.activeCommandWasGit = looksLikeGitCommand(trimmed);
        tab.activeCommandCanPage = canPage;
        tab.closedPagerEarly = false;
        if (canPage)
        {
            tab.pagerModel->clear();
            tab.pagerCommand = trimmed;
            tab.pagerActive = false;
            emit pagerStateChanged();
        }
        tab.stdoutPending.clear();
        tab.stderrPending.clear();
        tab.stdoutOverwriteLast = false;
        tab.stderrOverwriteLast = false;
        tab.stdoutStyle = {};
        tab.stderrStyle = {};

        emit tabsChanged();
        emit runningStateChanged();

        gf::terminal::bo::TerminalExecutionRequest req;
        req.commandLine = trimmed.toStdString();
        req.workingDirectory = gf::core::utf8ToPath(tab.currentWorkingDir.toStdString());
        req.shellKind = gf::terminal::bo::shellKindFromId(tab.shellId.toStdString());
        req.forceGitColor = true;

        const int tabId = tab.id;
        const QString repoPath = tab.repoRootPath;
        const bool wasGit = tab.activeCommandWasGit;
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
            const qint64 durationMs = static_cast<qint64>(res.duration.count());
            const bool cancelled = res.wasCancelled;
            QString newWorkingDir;
            if (!cancelled && res.finalWorkingDirectory && !res.finalWorkingDirectory->empty())
            {
                newWorkingDir = QString::fromStdString(gf::core::pathToUtf8(*res.finalWorkingDirectory));
            }
            QMetaObject::invokeMethod(this, [this, alive, tabId, headerId, repoPath, wasGit,
                                             exitCode, durationMs, cancelled, newWorkingDir = std::move(newWorkingDir)]()
            {
                if (!alive->load(std::memory_order_acquire)) return;
                handleProcessFinished(tabId, headerId, repoPath, wasGit, exitCode, durationMs, cancelled, newWorkingDir);
            }, Qt::QueuedConnection);
        };

        auto started = m_processRunner->start(req, std::move(callbacks));
        if (!started)
        {
            const QString err = QString::fromStdString(started.error());
            tab.outputModel->appendLine(QStringLiteral("stderr"), err.toHtmlEscaped(), err);
            tab.outputModel->finishCommandHeader(headerId, 1, QStringLiteral("0ms"), false);
            tab.isRunning = false;
            tab.activeCommandCanPage = false;
            emit tabsChanged();
            emit runningStateChanged();
            emit outputAppended();
            return;
        }

        tab.activeProcess = std::move(*started);
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

        const int headerId = prepareExternalCommandHeader(*tab, trimmed);
        if (headerId < 0)
        {
            return;
        }

        launchExternalCommand(*tab, headerId, trimmed);
    }

    void TerminalSessionViewModel::appendStreamChunk(TerminalTabState& tab,
                                                     const int streamKind,
                                                     const QByteArray& utf8Bytes)
    {
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
            if (!tab.pagerActive && tab.pagerModel->count() > kTerminalPagerLineThreshold)
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

    bool TerminalSessionViewModel::waitForIdle(const int timeoutMs)
    {
        return waitUntilIdleDraining(timeoutMs, [this]()
        {
            return !anyTabRunning() && m_bgPool.activeThreadCount() == 0;
        });
    }
} // namespace gf::desktop
