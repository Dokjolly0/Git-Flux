#pragma once

#include <QObject>
#include <QString>
#include <filesystem>
#include <functional>
#include <memory>
#include <string>

#include "desktop/viewmodels/stash_list_model.hpp"

namespace gf::git::bl
{
    class GitStashService;
}

namespace gf::desktop
{
    class SerialGitExecutor;

    /// Stash list and operations. Every git call runs on a SerialGitExecutor (the session's,
    /// shared with the other viewmodels so operations on the repository stay ordered).
    class StashManagerViewModel : public QObject
    {
        Q_OBJECT
        Q_PROPERTY(QString repoPath READ repoPath WRITE setRepoPath NOTIFY repoPathChanged)
        Q_PROPERTY(StashListModel* stashesModel READ stashesModel CONSTANT)
        Q_PROPERTY(bool isBusy READ isBusy NOTIFY isBusyChanged)
        Q_PROPERTY(QString errorMessage READ errorMessage NOTIFY errorMessageChanged)
        Q_PROPERTY(bool hasError READ hasError NOTIFY hasErrorChanged)

    public:
        /// Standalone: owns a private executor.
        explicit StashManagerViewModel(gf::git::bl::GitStashService& stashService,
                                       QObject* parent = nullptr);
        /// Shares `executor` (which must be destroyed before this viewmodel).
        StashManagerViewModel(gf::git::bl::GitStashService& stashService, SerialGitExecutor& executor,
                              QObject* parent = nullptr);
        ~StashManagerViewModel() override;

        [[nodiscard]] QString repoPath() const { return m_repoPath; }
        void setRepoPath(const QString& path);

        [[nodiscard]] StashListModel* stashesModel() noexcept { return &m_stashesModel; }
        [[nodiscard]] bool isBusy() const noexcept { return m_isBusy; }
        [[nodiscard]] QString errorMessage() const { return m_errorMessage; }
        [[nodiscard]] bool hasError() const noexcept { return !m_errorMessage.isEmpty(); }

        // Asynchronous: operations return whether they were started; the outcome arrives with
        // operationSucceeded(name) / operationFailed(name, message) (and errorMessage).
        Q_INVOKABLE void refresh();
        Q_INVOKABLE bool push(const QString& message, bool includeUntracked = true, bool keepIndex = false);
        Q_INVOKABLE bool pop(int index = 0, bool restoreIndex = false);
        Q_INVOKABLE bool apply(int index = 0, bool restoreIndex = false);
        Q_INVOKABLE bool drop(int index);
        Q_INVOKABLE bool clear();

        /// Test helper: processes events until no list read or operation is pending.
        bool waitForIdle(int timeoutMs = 10000);

        signals :

        void repoPathChanged();
        void isBusyChanged();
        void errorMessageChanged();
        void hasErrorChanged();
        void operationSucceeded(const QString& operation);
        void operationFailed(const QString& operation, const QString& message);

    private:
        using Operation = std::function<std::string()>; // returns an error message, empty on success

        void updateBusy();
        void setError(const QString& message);
        void clearError();
        bool runOperation(const QString& name, Operation operation);

        gf::git::bl::GitStashService& m_stashService;
        StashListModel m_stashesModel;
        QString m_repoPath;
        QString m_errorMessage;
        bool m_isBusy{false};
        int m_pendingTasks{0};     // list reads and operations not yet completed
        quint64 m_generation{0};   // bumped on repository change: late results are dropped

        SerialGitExecutor* m_executor{nullptr};
        // Declared last: destroyed first, waiting for its tasks while this object is alive.
        std::unique_ptr<SerialGitExecutor> m_ownedExecutor;
    };
} // namespace gf::desktop
