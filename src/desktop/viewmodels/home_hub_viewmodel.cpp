#include "desktop/viewmodels/home_hub_viewmodel.hpp"

#include "core/modules/repositories/bl/repository_service.hpp"
#include "core/modules/groups/bl/group_service.hpp"
#include "core/modules/workspaces/bl/workspace_service.hpp"

namespace gf::desktop {

HomeHubViewModel::HomeHubViewModel(gf::repositories::bl::RepositoryService& repoService,
                                   gf::groups::bl::GroupService& groupService,
                                   gf::workspaces::bl::WorkspaceService& workspaceService,
                                   QObject* parent)
    : QObject(parent),
      m_repoService(repoService),
      m_groupService(groupService),
      m_workspaceService(workspaceService),
      m_repoModel(std::make_unique<RepositoryListModel>(this)),
      m_groupModel(std::make_unique<GroupListModel>(this)),
      m_workspaceModel(std::make_unique<WorkspaceListModel>(this)) {
    refreshAll();
}

RepositoryListModel* HomeHubViewModel::repositoryModel() noexcept {
    return m_repoModel.get();
}

GroupListModel* HomeHubViewModel::groupModel() noexcept {
    return m_groupModel.get();
}

WorkspaceListModel* HomeHubViewModel::workspaceModel() noexcept {
    return m_workspaceModel.get();
}

QString HomeHubViewModel::searchFilter() const noexcept {
    return m_searchFilter;
}

void HomeHubViewModel::setSearchFilter(const QString& filter) {
    if (m_searchFilter != filter) {
        m_searchFilter = filter;
        emit searchFilterChanged(m_searchFilter);
        refreshAll();
    }
}

int HomeHubViewModel::repositoriesCount() const noexcept {
    return m_repoModel->rowCount();
}

int HomeHubViewModel::groupsCount() const noexcept {
    return m_groupModel->rowCount();
}

int HomeHubViewModel::workspacesCount() const noexcept {
    return m_workspaceModel->rowCount();
}

QString HomeHubViewModel::statusMessage() const noexcept {
    return m_statusMessage;
}

void HomeHubViewModel::setStatus(const QString& msg) {
    m_statusMessage = msg;
    emit statusMessageChanged(m_statusMessage);
}

void HomeHubViewModel::refreshAll() {
    // 1. Repositories
    auto allReposResult = m_repoService.getAllRepositories();
    std::vector<gf::repositories::bo::Repository> filteredRepos;

    if (allReposResult) {
        auto allRepos = std::move(*allReposResult);
        filteredRepos.reserve(allRepos.size());

        for (auto& repo : allRepos) {
            auto statusResult = m_repoService.checkRepositoryStatus(repo.id);
            if (statusResult) {
                repo.pathStatus = statusResult->pathStatus;
            }

            if (m_searchFilter.isEmpty()) {
                filteredRepos.push_back(std::move(repo));
            } else {
                const QString name = QString::fromStdString(repo.displayName);
                const QString path = QString::fromStdString(repo.localPath.string());
                if (name.contains(m_searchFilter, Qt::CaseInsensitive) ||
                    path.contains(m_searchFilter, Qt::CaseInsensitive)) {
                    filteredRepos.push_back(std::move(repo));
                }
            }
        }
    }
    m_repoModel->setRepositories(filteredRepos);
    emit repositoriesChanged();

    // 2. Groups
    auto allGroupsResult = m_groupService.getAllGroups();
    std::vector<gf::groups::bo::Group> filteredGroups;

    if (allGroupsResult) {
        auto allGroups = std::move(*allGroupsResult);
        filteredGroups.reserve(allGroups.size());

        for (auto& grp : allGroups) {
            if (m_searchFilter.isEmpty()) {
                filteredGroups.push_back(std::move(grp));
            } else {
                const QString name = QString::fromStdString(grp.name);
                if (name.contains(m_searchFilter, Qt::CaseInsensitive)) {
                    filteredGroups.push_back(std::move(grp));
                }
            }
        }
    }
    m_groupModel->setGroups(filteredGroups);
    emit groupsChanged();

    // 3. Workspaces
    auto allWorkspacesResult = m_workspaceService.getAllWorkspaces();
    std::vector<gf::workspaces::bo::Workspace> filteredWorkspaces;

    if (allWorkspacesResult) {
        auto allWorkspaces = std::move(*allWorkspacesResult);
        filteredWorkspaces.reserve(allWorkspaces.size());

        for (auto& ws : allWorkspaces) {
            if (m_searchFilter.isEmpty()) {
                filteredWorkspaces.push_back(std::move(ws));
            } else {
                const QString name = QString::fromStdString(ws.name);
                if (name.contains(m_searchFilter, Qt::CaseInsensitive)) {
                    filteredWorkspaces.push_back(std::move(ws));
                }
            }
        }
    }
    m_workspaceModel->setWorkspaces(filteredWorkspaces, m_workspaceService);
    emit workspacesChanged();
}

bool HomeHubViewModel::addRepository(const QString& path, const QString& displayName) {
    if (path.trimmed().isEmpty()) {
        emit errorOccurred(QStringLiteral("Il percorso del repository non può essere vuoto"));
        return false;
    }

    gf::repositories::bl::RegisterRepositoryRequest req;
    req.localPath = std::filesystem::path(path.toStdString());
    if (!displayName.trimmed().isEmpty()) {
        req.displayName = displayName.trimmed().toStdString();
    }

    auto result = m_repoService.registerRepository(req);
    if (!result) {
        QString errStr;
        switch (result.error().kind) {
        case gf::repositories::bl::RepositoryServiceError::Kind::PathNotFound:
            errStr = QStringLiteral("Percorso non trovato su filesystem");
            break;
        case gf::repositories::bl::RepositoryServiceError::Kind::NotAGitRepository:
            errStr = QStringLiteral("La cartella specificata non contiene un repository Git valido");
            break;
        case gf::repositories::bl::RepositoryServiceError::Kind::AlreadyRegistered:
            errStr = QStringLiteral("Il repository è già registrato nel catalogo");
            break;
        default:
            errStr = QStringLiteral("Errore durante la registrazione del repository: ") +
                     QString::fromStdString(result.error().message);
            break;
        }
        emit errorOccurred(errStr);
        return false;
    }

    setStatus(QStringLiteral("Repository '%1' registrato con successo").arg(QString::fromStdString(result->displayName)));
    refreshAll();
    return true;
}

bool HomeHubViewModel::reconnectRepository(const QString& repoId, const QString& newPath) {
    if (repoId.trimmed().isEmpty() || newPath.trimmed().isEmpty()) {
        emit errorOccurred(QStringLiteral("Parametri non validi per la riconnessione"));
        return false;
    }

    auto result = m_repoService.reconnectRepositoryPath(
        gf::repositories::bo::RepositoryId{repoId.toStdString()},
        std::filesystem::path(newPath.toStdString())
    );

    if (!result) {
        emit errorOccurred(QStringLiteral("Impossibile ricollegare il repository al percorso specificato: ") +
                           QString::fromStdString(result.error().message));
        return false;
    }

    setStatus(QStringLiteral("Repository ricollegato con successo al nuovo percorso"));
    refreshAll();
    return true;
}

bool HomeHubViewModel::removeRepository(const QString& repoId) {
    auto result = m_repoService.unregisterRepository(gf::repositories::bo::RepositoryId{repoId.toStdString()});
    if (!result) {
        emit errorOccurred(QStringLiteral("Impossibile rimuovere il repository dal catalogo"));
        return false;
    }

    setStatus(QStringLiteral("Repository rimosso dal catalogo"));
    refreshAll();
    return true;
}

bool HomeHubViewModel::createGroup(const QString& name, const QStringList& repoIds) {
    if (name.trimmed().isEmpty()) {
        emit errorOccurred(QStringLiteral("Il nome del gruppo non può essere vuoto"));
        return false;
    }

    std::vector<gf::repositories::bo::RepositoryId> ids;
    ids.reserve(static_cast<size_t>(repoIds.size()));
    for (const auto& id : repoIds) {
        ids.push_back(gf::repositories::bo::RepositoryId{id.toStdString()});
    }

    auto result = m_groupService.createGroup(name.trimmed().toStdString(), ids);
    if (!result) {
        if (result.error().kind == gf::groups::bl::GroupServiceError::Kind::DuplicateName) {
            emit errorOccurred(QStringLiteral("Esiste già un gruppo con questo nome"));
        } else {
            emit errorOccurred(QStringLiteral("Errore durante la creazione del gruppo: ") +
                               QString::fromStdString(result.error().message));
        }
        return false;
    }

    setStatus(QStringLiteral("Gruppo '%1' creato").arg(name));
    refreshAll();
    return true;
}

bool HomeHubViewModel::removeGroup(const QString& groupId) {
    auto result = m_groupService.deleteGroup(gf::groups::bo::GroupId{groupId.toStdString()});
    if (!result) {
        emit errorOccurred(QStringLiteral("Impossibile eliminare il gruppo"));
        return false;
    }

    setStatus(QStringLiteral("Gruppo eliminato"));
    refreshAll();
    return true;
}

bool HomeHubViewModel::createWorkspace(const QString& name,
                                       const QStringList& groupIds,
                                       const QStringList& repoIds) {
    if (name.trimmed().isEmpty()) {
        emit errorOccurred(QStringLiteral("Il nome del workspace non può essere vuoto"));
        return false;
    }

    std::vector<gf::groups::bo::GroupId> grpVector;
    grpVector.reserve(static_cast<size_t>(groupIds.size()));
    for (const auto& gid : groupIds) {
        grpVector.push_back(gf::groups::bo::GroupId{gid.toStdString()});
    }

    std::vector<gf::repositories::bo::RepositoryId> repoVector;
    repoVector.reserve(static_cast<size_t>(repoIds.size()));
    for (const auto& rid : repoIds) {
        repoVector.push_back(gf::repositories::bo::RepositoryId{rid.toStdString()});
    }

    auto result = m_workspaceService.createWorkspace(name.trimmed().toStdString(), grpVector, repoVector);
    if (!result) {
        if (result.error().kind == gf::workspaces::bl::WorkspaceServiceError::Kind::DuplicateName) {
            emit errorOccurred(QStringLiteral("Esiste già un workspace con questo nome"));
        } else {
            emit errorOccurred(QStringLiteral("Errore durante la creazione del workspace: ") +
                               QString::fromStdString(result.error().message));
        }
        return false;
    }

    setStatus(QStringLiteral("Workspace '%1' creato").arg(name));
    refreshAll();
    return true;
}

bool HomeHubViewModel::removeWorkspace(const QString& workspaceId) {
    auto result = m_workspaceService.deleteWorkspace(gf::workspaces::bo::WorkspaceId{workspaceId.toStdString()});
    if (!result) {
        emit errorOccurred(QStringLiteral("Impossibile eliminare il workspace"));
        return false;
    }

    setStatus(QStringLiteral("Workspace eliminato"));
    refreshAll();
    return true;
}

void HomeHubViewModel::openRepository(const QString& repoId) {
    auto repoResult = m_repoService.getRepository(gf::repositories::bo::RepositoryId{repoId.toStdString()});
    if (!repoResult || !repoResult->has_value()) {
        emit errorOccurred(QStringLiteral("Repository non trovato"));
        return;
    }

    const auto& repo = **repoResult;
    (void)m_repoService.touchLastAccessed(repo.id);
    emit repositoryOpened(repoId, QString::fromStdString(repo.localPath.string()));
}

void HomeHubViewModel::openGroup(const QString& groupId) {
    auto grpResult = m_groupService.getGroup(gf::groups::bo::GroupId{groupId.toStdString()});
    if (!grpResult || !grpResult->has_value()) {
        emit errorOccurred(QStringLiteral("Gruppo non trovato"));
        return;
    }
    emit groupOpened(groupId);
}

void HomeHubViewModel::openWorkspace(const QString& workspaceId) {
    auto wsResult = m_workspaceService.getWorkspace(gf::workspaces::bo::WorkspaceId{workspaceId.toStdString()});
    if (!wsResult || !wsResult->has_value()) {
        emit errorOccurred(QStringLiteral("Workspace non trovato"));
        return;
    }
    emit workspaceOpened(workspaceId);
}

} // namespace gf::desktop
