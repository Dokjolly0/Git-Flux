#pragma once

#include "desktop/viewmodels/repository_list_model.hpp"
#include "desktop/viewmodels/group_list_model.hpp"
#include "desktop/viewmodels/workspace_list_model.hpp"

#include <QObject>
#include <QString>
#include <QStringList>
#include <memory>

namespace gf::repositories::bl {
class RepositoryService;
}

namespace gf::groups::bl {
class GroupService;
}

namespace gf::workspaces::bl {
class WorkspaceService;
}

namespace gf::desktop {

class HomeHubViewModel : public QObject {
    Q_OBJECT

    Q_PROPERTY(gf::desktop::RepositoryListModel* repositories READ repositoryModel CONSTANT)
    Q_PROPERTY(gf::desktop::GroupListModel* groups READ groupModel CONSTANT)
    Q_PROPERTY(gf::desktop::WorkspaceListModel* workspaces READ workspaceModel CONSTANT)

    Q_PROPERTY(QString searchFilter READ searchFilter WRITE setSearchFilter NOTIFY searchFilterChanged)
    Q_PROPERTY(int repositoriesCount READ repositoriesCount NOTIFY repositoriesChanged)
    Q_PROPERTY(int groupsCount READ groupsCount NOTIFY groupsChanged)
    Q_PROPERTY(int workspacesCount READ workspacesCount NOTIFY workspacesChanged)
    Q_PROPERTY(QString statusMessage READ statusMessage NOTIFY statusMessageChanged)
    Q_PROPERTY(QString lastError READ lastError NOTIFY lastErrorChanged)
    Q_PROPERTY(QString lastSuccessMessage READ lastSuccessMessage NOTIFY lastSuccessMessageChanged)

public:
    explicit HomeHubViewModel(gf::repositories::bl::RepositoryService& repoService,
                              gf::groups::bl::GroupService& groupService,
                              gf::workspaces::bl::WorkspaceService& workspaceService,
                              QObject* parent = nullptr);
    ~HomeHubViewModel() override = default;

    [[nodiscard]] RepositoryListModel* repositoryModel() noexcept;
    [[nodiscard]] GroupListModel* groupModel() noexcept;
    [[nodiscard]] WorkspaceListModel* workspaceModel() noexcept;

    [[nodiscard]] QString searchFilter() const noexcept;
    void setSearchFilter(const QString& filter);

    [[nodiscard]] int repositoriesCount() const noexcept;
    [[nodiscard]] int groupsCount() const noexcept;
    [[nodiscard]] int workspacesCount() const noexcept;
    [[nodiscard]] QString statusMessage() const noexcept;
    [[nodiscard]] QString lastError() const noexcept;
    [[nodiscard]] QString lastSuccessMessage() const noexcept;

    Q_INVOKABLE void refreshAll();
    Q_INVOKABLE void clearFeedback();

    Q_INVOKABLE bool addRepository(const QString& path, const QString& displayName = QString());
    Q_INVOKABLE bool reconnectRepository(const QString& repoId, const QString& newPath);
    Q_INVOKABLE bool removeRepository(const QString& repoId);

    Q_INVOKABLE bool createGroup(const QString& name, const QStringList& repoIds = {});
    Q_INVOKABLE bool removeGroup(const QString& groupId);

    Q_INVOKABLE bool createWorkspace(const QString& name,
                                     const QStringList& groupIds = {},
                                     const QStringList& repoIds = {});
    Q_INVOKABLE bool removeWorkspace(const QString& workspaceId);

    Q_INVOKABLE void openRepository(const QString& repoId);
    Q_INVOKABLE void openGroup(const QString& groupId);
    Q_INVOKABLE void openWorkspace(const QString& workspaceId);

signals:
    void searchFilterChanged(const QString& filter);
    void repositoriesChanged();
    void groupsChanged();
    void workspacesChanged();
    void statusMessageChanged(const QString& message);

    void repositoryOpened(const QString& repoId, const QString& path);
    void repositoryAdded(const QString& repoName, const QString& path);
    void groupOpened(const QString& groupId);
    void workspaceOpened(const QString& workspaceId);
    void errorOccurred(const QString& message);
    void lastErrorChanged(const QString& error);
    void lastSuccessMessageChanged(const QString& message);

private:
    void setStatus(const QString& msg);
    void setLastError(const QString& error);
    void setLastSuccessMessage(const QString& msg);

    gf::repositories::bl::RepositoryService& m_repoService;
    gf::groups::bl::GroupService& m_groupService;
    gf::workspaces::bl::WorkspaceService& m_workspaceService;

    std::unique_ptr<RepositoryListModel> m_repoModel;
    std::unique_ptr<GroupListModel> m_groupModel;
    std::unique_ptr<WorkspaceListModel> m_workspaceModel;

    QString m_searchFilter;
    QString m_statusMessage;
    QString m_lastError;
    QString m_lastSuccessMessage;
};

} // namespace gf::desktop
