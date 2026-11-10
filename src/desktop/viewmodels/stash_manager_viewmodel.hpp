#pragma once

#include <QObject>
#include <QString>
#include <filesystem>
#include <memory>

#include "desktop/viewmodels/stash_list_model.hpp"

namespace gf::git::bl
{
    class GitStashService;
}

namespace gf::desktop
{
    class StashManagerViewModel : public QObject
    {
        Q_OBJECT
        Q_PROPERTY(QString repoPath READ repoPath WRITE setRepoPath NOTIFY repoPathChanged)
        Q_PROPERTY(StashListModel* stashesModel READ stashesModel CONSTANT)
        Q_PROPERTY(bool isBusy READ isBusy NOTIFY isBusyChanged)
        Q_PROPERTY(QString errorMessage READ errorMessage NOTIFY errorMessageChanged)
        Q_PROPERTY(bool hasError READ hasError NOTIFY hasErrorChanged)

    public:
        explicit StashManagerViewModel(gf::git::bl::GitStashService& stashService,
                                       QObject* parent = nullptr);
        ~StashManagerViewModel() override = default;

        [[nodiscard]] QString repoPath() const { return m_repoPath; }
        void setRepoPath(const QString& path);

        [[nodiscard]] StashListModel* stashesModel() noexcept { return &m_stashesModel; }
        [[nodiscard]] bool isBusy() const noexcept { return m_isBusy; }
        [[nodiscard]] QString errorMessage() const { return m_errorMessage; }
        [[nodiscard]] bool hasError() const noexcept { return !m_errorMessage.isEmpty(); }

    Q_INVOKABLE void refresh();
    Q_INVOKABLE bool push(const QString& message, bool includeUntracked = true, bool keepIndex = false);
    Q_INVOKABLE bool pop(int index = 0, bool restoreIndex = false);
    Q_INVOKABLE bool apply(int index = 0, bool restoreIndex = false);
    Q_INVOKABLE bool drop(int index);
    Q_INVOKABLE bool clear();

        signals :

        void repoPathChanged();
        void isBusyChanged();
        void errorMessageChanged();
        void hasErrorChanged();
        void operationSucceeded(const QString& operation);

    private:
        void setBusy(bool busy);
        void setError(const QString& message);
        void clearError();

        gf::git::bl::GitStashService& m_stashService;
        StashListModel m_stashesModel;
        QString m_repoPath;
        QString m_errorMessage;
        bool m_isBusy{false};
    };
} // namespace gf::desktop
