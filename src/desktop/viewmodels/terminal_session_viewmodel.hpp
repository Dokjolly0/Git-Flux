#pragma once

#include "desktop/viewmodels/terminal_completion_list_model.hpp"
#include "desktop/viewmodels/terminal_output_list_model.hpp"

#include <QObject>
#include <QString>
#include <QStringList>
#include <QThreadPool>
#include <QVariantList>
#include <QVariantMap>

#include <atomic>
#include <filesystem>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#ifndef Q_MOC_RUN
#include "core/modules/git/ports/git_command_executor.hpp"
#include "core/modules/terminal/bl/git_catalog_service.hpp"
#include "core/modules/terminal/bo/git_completion_models.hpp"
#include "core/modules/terminal/bo/terminal_models.hpp"
#include "core/modules/terminal/ports/terminal_process_port.hpp"
#else
namespace gf::git::ports
{
    class IGitCommandExecutor;
}
namespace gf::terminal::ports
{
    class ITerminalProcessRunner;
    class ITerminalProcessHandle;
}
namespace gf::terminal::bl
{
    class GitCatalogService;
}
namespace gf::terminal::bo
{
    struct RepoCompletionSnapshot;
    struct TerminalTextStyle;
}
#endif

namespace gf::desktop
{
    class TerminalSessionViewModel final : public QObject
    {
        Q_OBJECT

        Q_PROPERTY(QVariantList tabs READ tabs NOTIFY tabsChanged)
        Q_PROPERTY(int activeTabIndex READ activeTabIndex WRITE setActiveTabIndex NOTIFY activeTabChanged)
        Q_PROPERTY(int activeTabId READ activeTabId NOTIFY activeTabChanged)
        Q_PROPERTY(QString activeInputDraft READ activeInputDraft NOTIFY activeTabChanged)
        Q_PROPERTY(int activeInputCursorPos READ activeInputCursorPos NOTIFY activeTabChanged)
        Q_PROPERTY(TerminalOutputListModel* activeOutputModel READ activeOutputModel NOTIFY activeTabChanged)
        Q_PROPERTY(TerminalCompletionListModel* completionModel READ completionModel CONSTANT)
        Q_PROPERTY(bool isVisible READ isVisible WRITE setIsVisible NOTIFY visibilityChanged)
        Q_PROPERTY(bool isMaximized READ isMaximized WRITE setIsMaximized NOTIFY maximizedChanged)
        Q_PROPERTY(bool isRunning READ isRunning NOTIFY runningStateChanged)
        Q_PROPERTY(bool anyTabRunning READ anyTabRunning NOTIFY runningStateChanged)
        Q_PROPERTY(QString activeRepoName READ activeRepoName NOTIFY activeTabChanged)
        Q_PROPERTY(QString activeRepoPath READ activeRepoPath NOTIFY activeTabChanged)
        Q_PROPERTY(QString activeWorkingDir READ activeWorkingDir NOTIFY activeTabChanged)
        Q_PROPERTY(QString activeRelativeDir READ activeRelativeDir NOTIFY activeTabChanged)
        Q_PROPERTY(QString activeBranch READ activeBranch NOTIFY activeBranchChanged)
        Q_PROPERTY(QString activeShellId READ activeShellId NOTIFY activeTabChanged)
        Q_PROPERTY(QString activeTabColor READ activeTabColor NOTIFY activeTabColorChanged)
        Q_PROPERTY(QStringList tabColorPalette READ tabColorPalette CONSTANT)
        Q_PROPERTY(QVariantList availableShells READ availableShells CONSTANT)
        Q_PROPERTY(bool pagerActive READ pagerActive NOTIFY pagerStateChanged)
        Q_PROPERTY(TerminalOutputListModel* activePagerModel READ activePagerModel NOTIFY pagerStateChanged)
        Q_PROPERTY(QString pagerCommand READ pagerCommand NOTIFY pagerStateChanged)
        Q_PROPERTY(int pagerLineCount READ pagerLineCount NOTIFY pagerStateChanged)
        Q_PROPERTY(bool hasLastPagerOutput READ hasLastPagerOutput NOTIFY pagerStateChanged)
        Q_PROPERTY(QString ghostText READ ghostText NOTIFY completionsChanged)
        Q_PROPERTY(int completionCount READ completionCount NOTIFY completionsChanged)
        Q_PROPERTY(bool hasCompletions READ hasCompletions NOTIFY completionsChanged)
        Q_PROPERTY(int completionReplaceStart READ completionReplaceStart NOTIFY completionsChanged)

    public:
        TerminalSessionViewModel(gf::terminal::ports::ITerminalProcessRunner* processRunner,
                                 gf::terminal::bl::GitCatalogService* catalogService,
                                 gf::git::ports::IGitCommandExecutor* gitExecutor,
                                 QObject* parent = nullptr);
        ~TerminalSessionViewModel() override;

        /// Triggers asynchronous discovery of all installed Git commands & aliases.
        void warmUpCatalog();

        [[nodiscard]] QVariantList tabs() const;
        [[nodiscard]] int activeTabIndex() const noexcept;
        void setActiveTabIndex(int index);
        [[nodiscard]] int activeTabId() const noexcept;
        [[nodiscard]] QString activeInputDraft() const;
        [[nodiscard]] int activeInputCursorPos() const noexcept;

        [[nodiscard]] TerminalOutputListModel* activeOutputModel() const noexcept;
        [[nodiscard]] TerminalOutputListModel* activePagerModel() const noexcept;
        [[nodiscard]] TerminalCompletionListModel* completionModel() noexcept { return &m_completionModel; }

        [[nodiscard]] bool pagerActive() const noexcept;
        [[nodiscard]] QString pagerCommand() const;
        [[nodiscard]] int pagerLineCount() const noexcept;
        [[nodiscard]] bool hasLastPagerOutput() const noexcept;

        /// Returns true if the given command should use the integrated `less -FRX`-style pager.
        [[nodiscard]] static bool shouldPaginateCommand(std::string_view commandLine);

        /// Returns true if the given command line is a pure `cd` invocation to handle in-process
        /// (i.e. starts with `cd` and contains no unquoted shell operators such as `&&`, `||`, `;`, `|`).
        [[nodiscard]] static bool isBuiltinCdCommand(std::string_view commandLine) noexcept;

        [[nodiscard]] bool isVisible() const noexcept { return m_isVisible; }
        void setIsVisible(bool visible);

        [[nodiscard]] bool isMaximized() const noexcept { return m_isMaximized; }
        void setIsMaximized(bool maximized);

        [[nodiscard]] bool isRunning() const noexcept;
        [[nodiscard]] bool anyTabRunning() const noexcept;

        [[nodiscard]] QString activeRepoName() const;
        [[nodiscard]] QString activeRepoPath() const;
        [[nodiscard]] QString activeWorkingDir() const;
        [[nodiscard]] QString activeRelativeDir() const;
        [[nodiscard]] QString activeBranch() const;
        [[nodiscard]] QString activeShellId() const;
        [[nodiscard]] QString activeTabColor() const;
        [[nodiscard]] QStringList tabColorPalette() const;
        [[nodiscard]] QVariantList availableShells() const;

        [[nodiscard]] QString ghostText() const noexcept { return m_ghostText; }
        [[nodiscard]] int completionCount() const noexcept { return m_completionModel.count(); }
        [[nodiscard]] bool hasCompletions() const noexcept { return m_completionModel.count() > 0; }
        [[nodiscard]] int completionReplaceStart() const noexcept { return m_replaceStart; }

        Q_INVOKABLE void toggleVisible();
        void setActiveRepository(const QString& repoId,
                                 const QString& repoName,
                                 const QString& repoPath,
                                 const QString& branch = {});
        void updateActiveBranch(const QString& repoPath, const QString& branch);
        void closeRepositoryTerminals(const QString& repoId, const QString& repoPath);
        void closeAllTerminals();

        Q_INVOKABLE int createTab(const QString& shellId = {});
        Q_INVOKABLE void closeTab(int index);
        Q_INVOKABLE void renameTab(int index, const QString& newTitle);
        Q_INVOKABLE void setTabColor(int index, const QString& hexColor);
        Q_INVOKABLE void setActiveTabShell(const QString& shellId);
        Q_INVOKABLE void setActiveInputDraft(const QString& text, int cursorPos = -1);

        Q_INVOKABLE void executeCommand(const QString& commandLine);
        Q_INVOKABLE void interruptRunningCommand();
        Q_INVOKABLE void clearActiveOutput();
        Q_INVOKABLE QString copyActiveOutput() const;
        Q_INVOKABLE void closePager();
        Q_INVOKABLE void reopenLastPager();
        Q_INVOKABLE QString copyPagerOutput() const;

        Q_INVOKABLE QString historyPrevious(const QString& currentInput);
        Q_INVOKABLE QString historyNext();

        Q_INVOKABLE void updateCompletions(const QString& inputText, int cursorPos, bool explicitTrigger = false);
        Q_INVOKABLE bool shouldEnterAcceptCompletion(int selectedIndex,
                                                     bool userNavigated,
                                                     const QString& inputText,
                                                     int cursorPos) const;
        Q_INVOKABLE QVariantMap acceptCompletion(int index, const QString& inputText, int cursorPos);
        Q_INVOKABLE void dismissCompletions();

        /// Test helper: waits for background catalog/context tasks and running commands to settle.
        bool waitForIdle(int timeoutMs = 5000);

    signals:
        void tabsChanged();
        void activeTabChanged();
        void activeTabColorChanged();
        void activeBranchChanged();
        void visibilityChanged();
        void maximizedChanged();
        void runningStateChanged();
        void pagerStateChanged();
        void completionsChanged();
        void focusInputRequested();
        void outputAppended();
        void gitCommandFinished(const QString& repoPath, int exitCode);

    private:
        struct TerminalTabState;
        struct RepoTerminalBucket;

        [[nodiscard]] RepoTerminalBucket* activeBucket() noexcept;
        [[nodiscard]] const RepoTerminalBucket* activeBucket() const noexcept;
        [[nodiscard]] TerminalTabState* activeTab() noexcept;
        [[nodiscard]] const TerminalTabState* activeTab() const noexcept;
        [[nodiscard]] TerminalTabState* findTabById(int tabId) noexcept;

        void ensureInitialTabForActiveRepo();
        void sendStdinLine(const QString& text);
        void resetHistoryNavigation();
        void refreshRepoCompletionContext();
        void handleBuiltinCd(TerminalTabState& tab, const QString& targetArg, int headerId);
        void appendStreamChunk(TerminalTabState& tab, int streamKind, const QByteArray& utf8Bytes);
        void flushPendingBuffers(TerminalTabState& tab);
        void requestSubcommandOptionsAsync(const QString& subcommand);
        [[nodiscard]] int prepareExternalCommandHeader(TerminalTabState& tab, const QString& trimmed);
        void launchExternalCommand(TerminalTabState& tab, int headerId, const QString& trimmed);
        void handleProcessFinished(int tabId,
                                   int headerId,
                                   const QString& repoPath,
                                   bool wasGit,
                                   int exitCode,
                                   qint64 durationMs,
                                   bool cancelled,
                                   const QString& newWorkingDir);
        [[nodiscard]] QString buildPromptPrefix(const TerminalTabState& tab) const;
        [[nodiscard]] QString formatRelativeDir(const QString& repoRoot, const QString& currentDir) const;

        gf::terminal::ports::ITerminalProcessRunner* m_processRunner{nullptr};
        gf::terminal::bl::GitCatalogService* m_catalogService{nullptr};
        gf::git::ports::IGitCommandExecutor* m_gitExecutor{nullptr};

        TerminalCompletionListModel m_completionModel;
        TerminalOutputListModel m_emptyOutputModel;

        std::unordered_map<std::string, std::unique_ptr<RepoTerminalBucket>> m_buckets;
        std::string m_activeRepoKey;
        QString m_activeRepoId;
        QString m_activeRepoName;
        QString m_activeRepoPath;
        QString m_activeBranch;

        bool m_isVisible = false;
        bool m_isMaximized = false;
        int m_nextTabId = 1;

        // Autocomplete state
        QString m_ghostText;
        int m_replaceStart = 0;
        int m_replaceLength = 0;
        QString m_lastCompletionInput;
        int m_lastCompletionCursor = 0;
        bool m_lastCompletionExplicit = false;
        quint64 m_completionGeneration = 0;
        std::unordered_set<std::string> m_loadingOptionsSubcommands;

        // Per-repo cached completion snapshot
        std::unordered_map<std::string, std::shared_ptr<gf::terminal::bo::RepoCompletionSnapshot>> m_repoSnapshots;

        std::shared_ptr<std::atomic<bool>> m_alive = std::make_shared<std::atomic<bool>>(true);
        QThreadPool m_bgPool;
    };
} // namespace gf::desktop
