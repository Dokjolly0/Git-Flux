#include "desktop/viewmodels/home_hub_viewmodel.hpp"

#include "core/modules/repositories/bl/repository_service.hpp"
#include "core/modules/repositories/bo/repository.hpp"
#include "core/modules/groups/bl/group_service.hpp"
#include "core/modules/groups/bo/group.hpp"
#include "core/modules/workspaces/bl/workspace_service.hpp"
#include "core/modules/workspaces/bo/workspace.hpp"
#include "core/modules/git/bl/git_clone_service.hpp"

#include <thread>
#include <iostream>
#include <QUrl>
#include <QSettings>
#include <QDesktopServices>
#include <QProcess>
#include <QClipboard>
#include <QGuiApplication>
#include <QStandardPaths>
#include <QDebug>

namespace gf::desktop
{
    HomeHubViewModel::HomeHubViewModel(gf::repositories::bl::RepositoryService& repoService,
                                       gf::groups::bl::GroupService& groupService,
                                       gf::workspaces::bl::WorkspaceService& workspaceService,
                                       gf::git::bl::GitCloneService* cloneService,
                                       QObject* parent)
        : QObject(parent),
          m_repoService(repoService),
          m_groupService(groupService),
          m_workspaceService(workspaceService),
          m_cloneService(cloneService),
          m_repoModel(std::make_unique<RepositoryListModel>(this)),
          m_groupModel(std::make_unique<GroupListModel>(this)),
          m_workspaceModel(std::make_unique<WorkspaceListModel>(this))
    {
        QSettings settings;
        m_lastCloneDestinationPath = settings.value(QStringLiteral("clone/lastDestinationParentFolder"), QString()).
                                              toString();
        if (m_lastCloneDestinationPath.isEmpty())
        {
            auto allRepos = m_repoService.getAllRepositories();
            if (allRepos.has_value() && !allRepos->empty())
            {
                m_lastCloneDestinationPath = QString::fromStdString(allRepos->front().localPath.parent_path().string());
            }
        }
        m_showRepositories = settings.value(QStringLiteral("home/showRepositories"), true).toBool();
        m_showGroups = settings.value(QStringLiteral("home/showGroups"), true).toBool();
        m_showWorkspaces = settings.value(QStringLiteral("home/showWorkspaces"), true).toBool();
        if (!m_showRepositories && !m_showGroups && !m_showWorkspaces)
        {
            m_showRepositories = true;
            m_showGroups = true;
            m_showWorkspaces = true;
        }
        refreshAll();
    }

    RepositoryListModel* HomeHubViewModel::repositoryModel() noexcept
    {
        return m_repoModel.get();
    }

    GroupListModel* HomeHubViewModel::groupModel() noexcept
    {
        return m_groupModel.get();
    }

    WorkspaceListModel* HomeHubViewModel::workspaceModel() noexcept
    {
        return m_workspaceModel.get();
    }

    QString HomeHubViewModel::searchFilter() const noexcept
    {
        return m_searchFilter;
    }

    void HomeHubViewModel::setSearchFilter(const QString& filter)
    {
        if (m_searchFilter != filter)
        {
            m_searchFilter = filter;
            emit searchFilterChanged(m_searchFilter);
            refreshAll();
        }
    }

    int HomeHubViewModel::repositoriesCount() const noexcept
    {
        return m_repoModel->rowCount();
    }

    int HomeHubViewModel::groupsCount() const noexcept
    {
        return m_groupModel->rowCount();
    }

    int HomeHubViewModel::workspacesCount() const noexcept
    {
        return m_workspaceModel->rowCount();
    }

    QString HomeHubViewModel::statusMessage() const noexcept
    {
        return m_statusMessage;
    }

    QString HomeHubViewModel::lastError() const noexcept
    {
        return m_lastError;
    }

    QString HomeHubViewModel::lastSuccessMessage() const noexcept
    {
        return m_lastSuccessMessage;
    }

    void HomeHubViewModel::clearFeedback()
    {
        setLastError(QString());
        setLastSuccessMessage(QString());
    }

    void HomeHubViewModel::setStatus(const QString& msg)
    {
        m_statusMessage = msg;
        emit statusMessageChanged(m_statusMessage);
    }

    void HomeHubViewModel::setLastError(const QString& error)
    {
        if (m_lastError != error)
        {
            m_lastError = error;
            emit lastErrorChanged(m_lastError);
        }
    }

    void HomeHubViewModel::setLastSuccessMessage(const QString& msg)
    {
        if (m_lastSuccessMessage != msg)
        {
            m_lastSuccessMessage = msg;
            emit lastSuccessMessageChanged(m_lastSuccessMessage);
        }
    }

    void HomeHubViewModel::refreshAll()
    {
        // 1. Repositories
        auto allReposResult = m_repoService.getAllRepositories();
        std::vector<gf::repositories::bo::Repository> filteredRepos;

        if (allReposResult)
        {
            auto allRepos = std::move(*allReposResult);
            filteredRepos.reserve(allRepos.size());

            for (auto& repo : allRepos)
            {
                auto statusResult = m_repoService.checkRepositoryStatus(repo.id);
                if (statusResult)
                {
                    repo.pathStatus = statusResult->pathStatus;
                }

                if (m_searchFilter.isEmpty())
                {
                    filteredRepos.push_back(std::move(repo));
                }
                else
                {
                    const QString name = QString::fromStdString(repo.displayName);
                    const QString path = QString::fromStdString(repo.localPath.string());
                    if (name.contains(m_searchFilter, Qt::CaseInsensitive) ||
                        path.contains(m_searchFilter, Qt::CaseInsensitive))
                    {
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

        if (allGroupsResult)
        {
            auto allGroups = std::move(*allGroupsResult);
            filteredGroups.reserve(allGroups.size());

            for (auto& grp : allGroups)
            {
                if (m_searchFilter.isEmpty())
                {
                    filteredGroups.push_back(std::move(grp));
                }
                else
                {
                    const QString name = QString::fromStdString(grp.name);
                    if (name.contains(m_searchFilter, Qt::CaseInsensitive))
                    {
                        filteredGroups.push_back(std::move(grp));
                    }
                }
            }
        }
        m_groupModel->setGroups(filteredGroups, &m_repoService);
        emit groupsChanged();

        // 3. Workspaces
        auto allWorkspacesResult = m_workspaceService.getAllWorkspaces();
        std::vector<gf::workspaces::bo::Workspace> filteredWorkspaces;

        if (allWorkspacesResult)
        {
            auto allWorkspaces = std::move(*allWorkspacesResult);
            filteredWorkspaces.reserve(allWorkspaces.size());

            for (auto& ws : allWorkspaces)
            {
                if (m_searchFilter.isEmpty())
                {
                    filteredWorkspaces.push_back(std::move(ws));
                }
                else
                {
                    const QString name = QString::fromStdString(ws.name);
                    if (name.contains(m_searchFilter, Qt::CaseInsensitive))
                    {
                        filteredWorkspaces.push_back(std::move(ws));
                    }
                }
            }
        }
        m_workspaceModel->setWorkspaces(filteredWorkspaces, m_workspaceService, &m_groupService);
        emit workspacesChanged();
    }

    bool HomeHubViewModel::addRepository(const QString& path, const QString& displayName)
    {
        QString cleanPath = path.trimmed();
        if (cleanPath.isEmpty())
        {
            const QString err = QStringLiteral("Il percorso del repository non può essere vuoto");
            setLastError(err);
            setStatus(err);
            emit errorOccurred(err);
            return false;
        }

        // Convert QUrl if needed (e.g. from QML FolderDialog file:///path)
        if (cleanPath.startsWith(QLatin1String("file:"), Qt::CaseInsensitive))
        {
            const QUrl url(cleanPath);
            if (url.isValid() && url.isLocalFile() && !url.toLocalFile().startsWith(QLatin1String("//")))
            {
                cleanPath = url.toLocalFile();
            }
            else
            {
                if (cleanPath.startsWith(QLatin1String("file:///"), Qt::CaseInsensitive))
                {
                    cleanPath = cleanPath.sliced(8);
                }
                else if (cleanPath.startsWith(QLatin1String("file://"), Qt::CaseInsensitive))
                {
                    cleanPath = cleanPath.sliced(7);
                }
#ifndef _WIN32
                if (!cleanPath.startsWith(QLatin1Char('/')))
                {
                    cleanPath.prepend(QLatin1Char('/'));
                }
#endif
            }
        }

        while (cleanPath.length() > 1 && (cleanPath.endsWith(QLatin1Char('/')) || cleanPath.
            endsWith(QLatin1Char('\\'))))
        {
            cleanPath.chop(1);
        }

        gf::repositories::bl::RegisterRepositoryRequest req;
        req.localPath = std::filesystem::path(cleanPath.toStdString());
        if (!displayName.trimmed().isEmpty())
        {
            req.displayName = displayName.trimmed().toStdString();
        }

        auto result = m_repoService.registerRepository(req);
        if (!result)
        {
            QString errStr;
            switch (result.error().kind)
            {
            case gf::repositories::bl::RepositoryServiceError::Kind::PathNotFound:
                errStr = QStringLiteral("Percorso non trovato: %1\nVerifica che la cartella esista su disco.")
                    .arg(cleanPath);
                break;
            case gf::repositories::bl::RepositoryServiceError::Kind::NotAGitRepository:
                errStr = QStringLiteral(
                        "La cartella specificata non contiene un repository Git valido: %1\nNessuna directory '.git' trovata.")
                    .arg(cleanPath);
                break;
            case gf::repositories::bl::RepositoryServiceError::Kind::AlreadyRegistered:
                errStr = QStringLiteral("Questo repository è già registrato nel catalogo di Git Flux.");
                break;
            default:
                errStr = QStringLiteral("Errore durante la registrazione del repository: ") +
                    QString::fromStdString(result.error().message);
                break;
            }
            setLastError(errStr);
            setStatus(errStr);
            emit errorOccurred(errStr);
            return false;
        }

        const QString repoName = QString::fromStdString(result->displayName);
        const QString repoSavedPath = QString::fromStdString(result->localPath.string());
        const QString successMsg = QStringLiteral("Repository '%1' aggiunto con successo!").arg(repoName);

        setLastSuccessMessage(successMsg);
        setStatus(successMsg);
        setLastError(QString());

        emit repositoryAdded(repoName, repoSavedPath);
        refreshAll();
        return true;
    }

    QString HomeHubViewModel::extractRepoNameFromUrl(const QString& url) const
    {
        return QString::fromStdString(
            gf::git::bl::GitCloneService::extractRepositoryNameFromUrl(url.trimmed().toStdString()));
    }

    void HomeHubViewModel::cloneRepositoryAsync(const QString& remoteUrl,
                                                const QString& destinationParentFolder,
                                                const QString& directoryName,
                                                const QString& displayName)
    {
        std::cout << "[HomeHubViewModel] cloneRepositoryAsync requested: URL=" << remoteUrl.toStdString()
            << " Parent=" << destinationParentFolder.toStdString()
            << " Dir=" << directoryName.toStdString()
            << " DisplayName=" << displayName.toStdString() << std::endl;
        qInfo() << "[HomeHubViewModel] cloneRepositoryAsync requested:"
            << "URL:" << remoteUrl
            << "Parent:" << destinationParentFolder
            << "Dir:" << directoryName
            << "DisplayName:" << displayName;

        if (!m_cloneService)
        {
            const QString err = QStringLiteral("Servizio di clonazione non disponibile");
            qWarning() << "[HomeHubViewModel] Error:" << err;
            setLastError(err);
            setStatus(err);
            emit errorOccurred(err);
            emit cloneCompleted(false, err);
            return;
        }

        const QString cleanUrl = remoteUrl.trimmed();
        if (cleanUrl.isEmpty())
        {
            const QString err = QStringLiteral("L'URL del repository remoto non può essere vuoto");
            setLastError(err);
            setStatus(err);
            emit errorOccurred(err);
            emit cloneCompleted(false, err);
            return;
        }

        QString cleanParent = destinationParentFolder.trimmed();
        const QUrl parentUrl(cleanParent);
        if (parentUrl.isLocalFile())
        {
            cleanParent = parentUrl.toLocalFile();
        }
        else if (cleanParent.startsWith(QLatin1String("file://")))
        {
            cleanParent = cleanParent.sliced(7);
        }
        while (cleanParent.length() > 1 && (cleanParent.endsWith(QLatin1Char('/')) || cleanParent.endsWith(
            QLatin1Char('\\'))))
        {
            cleanParent.chop(1);
        }

        if (cleanParent.isEmpty())
        {
            const QString err = QStringLiteral("Specificare una cartella di destinazione locale valida");
            setLastError(err);
            setStatus(err);
            emit errorOccurred(err);
            emit cloneCompleted(false, err);
            return;
        }

        QString finalDirName = directoryName.trimmed();
        if (finalDirName.isEmpty())
        {
            finalDirName = extractRepoNameFromUrl(cleanUrl);
        }

        const std::filesystem::path targetPath = std::filesystem::path(cleanParent.toStdString()) / finalDirName.
            toStdString();

        m_isCloning = true;
        m_cloneProgressPercent = 0;
        emit isCloningChanged(true);
        emit cloneProgressPercentChanged(0);
        m_cloneStatusMessage = QStringLiteral("Avvio clonazione da %1...").arg(cleanUrl);
        emit cloneStatusMessageChanged(m_cloneStatusMessage);
        setStatus(m_cloneStatusMessage);
        setLastError(QString());

        std::thread([this,
                cleanUrlStd = cleanUrl.toStdString(),
                targetPath,
                dispNameStd = displayName.trimmed().toStdString(),
                cleanParent]()
            {
                gf::git::bo::CloneRepositoryOptions opts{
                    .remoteUrl = cleanUrlStd,
                    .destinationPath = targetPath,
                    .timeout = std::chrono::milliseconds{180'000},
                    .progressCallback = [this](int percent, std::string_view statusText)
                    {
                        QMetaObject::invokeMethod(
                            this, [this, percent, txt = QString::fromUtf8(
                                statusText.data(), static_cast<qsizetype>(statusText.size()))]()
                            {
                                if (percent >= 0)
                                {
                                    m_cloneProgressPercent = percent;
                                    emit cloneProgressPercentChanged(percent);
                                }
                                if (!txt.isEmpty())
                                {
                                    m_cloneStatusMessage = txt;
                                    emit cloneStatusMessageChanged(m_cloneStatusMessage);
                                    setStatus(m_cloneStatusMessage);
                                }
                            }, Qt::QueuedConnection);
                    }
                };

                std::optional<std::string> optDisp;
                if (!dispNameStd.empty())
                {
                    optDisp = dispNameStd;
                }

                auto result = m_cloneService->cloneAndRegister(opts, optDisp);

                QMetaObject::invokeMethod(this, [this, result, targetPath, cleanParent]()
                {
                    m_isCloning = false;
                    m_cloneProgressPercent = result ? 100 : 0;
                    emit isCloningChanged(false);
                    emit cloneProgressPercentChanged(m_cloneProgressPercent);

                    if (!result)
                    {
                        QString errStr;
                        switch (result.error().kind)
                        {
                        case gf::git::ports::GitOpsError::Kind::DestinationAlreadyExists:
                            errStr = QStringLiteral("La cartella di destinazione esiste già e non è vuota: %1")
                                .arg(QString::fromStdString(targetPath.string()));
                            break;
                        case gf::git::ports::GitOpsError::Kind::NetworkAuthenticationFailed:
                            errStr = QStringLiteral(
                                    "Autenticazione fallita o host non raggiungibile durante il clone:\n%1")
                                .arg(QString::fromStdString(result.error().message));
                            break;
                        case gf::git::ports::GitOpsError::Kind::RemoteNotFound:
                            errStr = QStringLiteral("Repository remoto non trovato o URL non valido:\n%1")
                                .arg(QString::fromStdString(result.error().message));
                            break;
                        default:
                            errStr = QStringLiteral("Errore durante la clonazione del repository:\n%1")
                                .arg(QString::fromStdString(result.error().message));
                            break;
                        }

                        m_cloneStatusMessage = errStr;
                        emit cloneStatusMessageChanged(errStr);
                        setLastError(errStr);
                        setStatus(errStr);
                        emit errorOccurred(errStr);
                        emit cloneCompleted(false, errStr);
                    }
                    else
                    {
                        const QString repoName = QString::fromStdString(result->displayName);
                        const QString repoSavedPath = QString::fromStdString(result->localPath.string());
                        const QString successMsg = QStringLiteral("Repository '%1' clonato con successo!").
                            arg(repoName);

                        m_cloneStatusMessage = successMsg;
                        emit cloneStatusMessageChanged(successMsg);
                        setLastSuccessMessage(successMsg);
                        setStatus(successMsg);
                        setLastError(QString());

                        emit repositoryAdded(repoName, repoSavedPath);
                        emit cloneCompleted(true, successMsg);
                        setLastCloneDestinationPath(cleanParent);
                        refreshAll();
                    }
                }, Qt::QueuedConnection);
            }).detach();
    }

    void HomeHubViewModel::setLastCloneDestinationPath(const QString& path)
    {
        const QString trimmed = path.trimmed();
        if (trimmed.isEmpty() || m_lastCloneDestinationPath == trimmed) return;
        m_lastCloneDestinationPath = trimmed;
        QSettings settings;
        settings.setValue(QStringLiteral("clone/lastDestinationParentFolder"), m_lastCloneDestinationPath);
        emit lastCloneDestinationPathChanged(m_lastCloneDestinationPath);
    }

    void HomeHubViewModel::setShowRepositories(bool show)
    {
        if (m_showRepositories == show) return;
        if (!show && !m_showGroups && !m_showWorkspaces) return; // Prevent disabling all 3
        m_showRepositories = show;
        QSettings settings;
        settings.setValue(QStringLiteral("home/showRepositories"), m_showRepositories);
        emit showRepositoriesChanged(m_showRepositories);
    }

    void HomeHubViewModel::setShowGroups(bool show)
    {
        if (m_showGroups == show) return;
        if (!show && !m_showRepositories && !m_showWorkspaces) return; // Prevent disabling all 3
        m_showGroups = show;
        QSettings settings;
        settings.setValue(QStringLiteral("home/showGroups"), m_showGroups);
        emit showGroupsChanged(m_showGroups);
    }

    void HomeHubViewModel::setShowWorkspaces(bool show)
    {
        if (m_showWorkspaces == show) return;
        if (!show && !m_showRepositories && !m_showGroups) return; // Prevent disabling all 3
        m_showWorkspaces = show;
        QSettings settings;
        settings.setValue(QStringLiteral("home/showWorkspaces"), m_showWorkspaces);
        emit showWorkspacesChanged(m_showWorkspaces);
    }

    void HomeHubViewModel::toggleShowRepositories()
    {
        setShowRepositories(!m_showRepositories);
    }

    void HomeHubViewModel::toggleShowGroups()
    {
        setShowGroups(!m_showGroups);
    }

    void HomeHubViewModel::toggleShowWorkspaces()
    {
        setShowWorkspaces(!m_showWorkspaces);
    }

    void HomeHubViewModel::resetCloneState()
    {
        m_isCloning = false;
        m_cloneProgressPercent = 0;
        m_cloneStatusMessage.clear();
        emit isCloningChanged(false);
        emit cloneProgressPercentChanged(0);
        emit cloneStatusMessageChanged(QString());
        setLastError(QString());
        clearFeedback();
    }

    bool HomeHubViewModel::reconnectRepository(const QString& repoId, const QString& newPath)
    {
        if (repoId.trimmed().isEmpty() || newPath.trimmed().isEmpty())
        {
            const QString err = QStringLiteral("Parametri non validi per la riconnessione");
            setLastError(err);
            setStatus(err);
            emit errorOccurred(err);
            return false;
        }

        QString cleanPath = newPath.trimmed();
        const QUrl url(cleanPath);
        if (url.isLocalFile())
        {
            cleanPath = url.toLocalFile();
        }
        else if (cleanPath.startsWith(QLatin1String("file://")))
        {
            cleanPath = cleanPath.sliced(7);
        }
        while (cleanPath.length() > 1 && (cleanPath.endsWith(QLatin1Char('/')) || cleanPath.
            endsWith(QLatin1Char('\\'))))
        {
            cleanPath.chop(1);
        }

        auto result = m_repoService.reconnectRepositoryPath(
            gf::repositories::bo::RepositoryId{repoId.toStdString()},
            std::filesystem::path(cleanPath.toStdString())
        );

        if (!result)
        {
            QString errStr;
            switch (result.error().kind)
            {
            case gf::repositories::bl::RepositoryServiceError::Kind::PathNotFound:
                errStr = QStringLiteral("Nuovo percorso non trovato sul filesystem: %1").arg(cleanPath);
                break;
            case gf::repositories::bl::RepositoryServiceError::Kind::NotAGitRepository:
                errStr = QStringLiteral("Il nuovo percorso non contiene un repository Git valido: %1").arg(cleanPath);
                break;
            default:
                errStr = QStringLiteral("Impossibile ricollegare il repository: ") +
                    QString::fromStdString(result.error().message);
                break;
            }
            setLastError(errStr);
            setStatus(errStr);
            emit errorOccurred(errStr);
            return false;
        }

        const QString successMsg = QStringLiteral("Repository '%1' ricollegato con successo!").arg(
            QString::fromStdString(result->displayName));
        setLastSuccessMessage(successMsg);
        setStatus(successMsg);
        setLastError(QString());
        refreshAll();
        return true;
    }

    bool HomeHubViewModel::removeRepository(const QString& repoId)
    {
        auto result = m_repoService.unregisterRepository(gf::repositories::bo::RepositoryId{repoId.toStdString()});
        if (!result)
        {
            emit errorOccurred(QStringLiteral("Impossibile rimuovere il repository dal catalogo"));
            return false;
        }

        setStatus(QStringLiteral("Repository rimosso dal catalogo"));
        refreshAll();
        return true;
    }

    bool HomeHubViewModel::createGroup(const QString& name, const QStringList& repoIds)
    {
        if (name.trimmed().isEmpty())
        {
            emit errorOccurred(QStringLiteral("Il nome del gruppo non può essere vuoto"));
            return false;
        }

        std::vector<gf::repositories::bo::RepositoryId> ids;
        ids.reserve(static_cast<size_t>(repoIds.size()));
        for (const auto& id : repoIds)
        {
            ids.push_back(gf::repositories::bo::RepositoryId{id.toStdString()});
        }

        auto result = m_groupService.createGroup(name.trimmed().toStdString(), ids);
        if (!result)
        {
            if (result.error().kind == gf::groups::bl::GroupServiceError::Kind::DuplicateName)
            {
                emit errorOccurred(QStringLiteral("Esiste già un gruppo con questo nome"));
            }
            else
            {
                emit errorOccurred(QStringLiteral("Errore durante la creazione del gruppo: ") +
                    QString::fromStdString(result.error().message));
            }
            return false;
        }

        setStatus(QStringLiteral("Gruppo '%1' creato").arg(name));
        refreshAll();
        return true;
    }

    bool HomeHubViewModel::removeGroup(const QString& groupId)
    {
        auto result = m_groupService.deleteGroup(gf::groups::bo::GroupId{groupId.toStdString()});
        if (!result)
        {
            emit errorOccurred(QStringLiteral("Impossibile eliminare il gruppo"));
            return false;
        }

        setStatus(QStringLiteral("Gruppo eliminato"));
        refreshAll();
        return true;
    }

    bool HomeHubViewModel::createWorkspace(const QString& name,
                                           const QStringList& groupIds,
                                           const QStringList& repoIds)
    {
        if (name.trimmed().isEmpty())
        {
            emit errorOccurred(QStringLiteral("Il nome del workspace non può essere vuoto"));
            return false;
        }

        std::vector<gf::groups::bo::GroupId> grpVector;
        grpVector.reserve(static_cast<size_t>(groupIds.size()));
        for (const auto& gid : groupIds)
        {
            grpVector.push_back(gf::groups::bo::GroupId{gid.toStdString()});
        }

        std::vector<gf::repositories::bo::RepositoryId> repoVector;
        repoVector.reserve(static_cast<size_t>(repoIds.size()));
        for (const auto& rid : repoIds)
        {
            repoVector.push_back(gf::repositories::bo::RepositoryId{rid.toStdString()});
        }

        auto result = m_workspaceService.createWorkspace(name.trimmed().toStdString(), grpVector, repoVector);
        if (!result)
        {
            if (result.error().kind == gf::workspaces::bl::WorkspaceServiceError::Kind::DuplicateName)
            {
                emit errorOccurred(QStringLiteral("Esiste già un workspace con questo nome"));
            }
            else
            {
                emit errorOccurred(QStringLiteral("Errore durante la creazione del workspace: ") +
                    QString::fromStdString(result.error().message));
            }
            return false;
        }

        setStatus(QStringLiteral("Workspace '%1' creato").arg(name));
        refreshAll();
        return true;
    }

    bool HomeHubViewModel::removeWorkspace(const QString& workspaceId)
    {
        auto result = m_workspaceService.deleteWorkspace(gf::workspaces::bo::WorkspaceId{workspaceId.toStdString()});
        if (!result)
        {
            emit errorOccurred(QStringLiteral("Impossibile eliminare il workspace"));
            return false;
        }

        setStatus(QStringLiteral("Workspace eliminato"));
        refreshAll();
        return true;
    }

    void HomeHubViewModel::openInFileManager(const QString& path)
    {
        if (path.isEmpty())
        {
            return;
        }
        const QUrl url = QUrl::fromLocalFile(path);
        if (QDesktopServices::openUrl(url))
        {
            setLastSuccessMessage(QStringLiteral("Aperto nel gestore file: %1").arg(path));
        }
        else
        {
            const QString err = QStringLiteral("Impossibile aprire il gestore file per: %1").arg(path);
            setLastError(err);
            emit errorOccurred(err);
        }
    }

    void HomeHubViewModel::openInTerminal(const QString& path)
    {
        if (path.isEmpty())
        {
            return;
        }

        const QStringList terminals = {
            QString::fromLocal8Bit(qgetenv("TERMINAL")),
            QStringLiteral("x-terminal-emulator"),
            QStringLiteral("gnome-terminal"),
            QStringLiteral("konsole"),
            QStringLiteral("xfce4-terminal"),
            QStringLiteral("alacritty"),
            QStringLiteral("kitty"),
            QStringLiteral("wezterm"),
            QStringLiteral("terminator"),
            QStringLiteral("xterm")
        };

        for (const auto& term : terminals)
        {
            if (term.isEmpty())
            {
                continue;
            }
            if (!QStandardPaths::findExecutable(term).isEmpty())
            {
                bool started = false;
                if (term == QLatin1String("gnome-terminal") || term == QLatin1String("xfce4-terminal"))
                {
                    started = QProcess::startDetached(term, {QStringLiteral("--working-directory"), path});
                }
                else if (term == QLatin1String("konsole"))
                {
                    started = QProcess::startDetached(term, {QStringLiteral("--workdir"), path});
                }
                else
                {
                    started = QProcess::startDetached(term, {}, path);
                }
                if (started)
                {
                    setLastSuccessMessage(QStringLiteral("Terminale avviato in: %1").arg(path));
                    return;
                }
            }
        }

        const QString err = QStringLiteral("Nessun emulatore di terminale trovato nel sistema.");
        setLastError(err);
        emit errorOccurred(err);
    }

    void HomeHubViewModel::copyToClipboard(const QString& text)
    {
        if (auto* cb = QGuiApplication::clipboard())
        {
            cb->setText(text);
            setLastSuccessMessage(QStringLiteral("Copiato negli appunti: %1").arg(text));
        }
    }

    bool HomeHubViewModel::renameRepository(const QString& repoId, const QString& newDisplayName)
    {
        if (repoId.trimmed().isEmpty())
        {
            return false;
        }
        auto res = m_repoService.updateDisplayName(
            gf::repositories::bo::RepositoryId{repoId.toStdString()},
            newDisplayName.trimmed().toStdString()
        );
        if (!res)
        {
            const QString err = QStringLiteral("Errore rinomina repository: ") + QString::fromStdString(
                res.error().message);
            setLastError(err);
            emit errorOccurred(err);
            return false;
        }

        const QString successMsg = QStringLiteral("Repository rinominato in: %1").arg(
            QString::fromStdString(res->displayName));
        setLastSuccessMessage(successMsg);
        setStatus(successMsg);
        refreshAll();
        return true;
    }

    bool HomeHubViewModel::renameGroup(const QString& groupId, const QString& newName)
    {
        if (groupId.trimmed().isEmpty() || newName.trimmed().isEmpty())
        {
            const QString err = QStringLiteral("Il nome del gruppo non può essere vuoto");
            setLastError(err);
            emit errorOccurred(err);
            return false;
        }

        auto res = m_groupService.renameGroup(
            gf::groups::bo::GroupId{groupId.toStdString()},
            newName.trimmed().toStdString()
        );
        if (!res)
        {
            QString err;
            if (res.error().kind == gf::groups::bl::GroupServiceError::Kind::DuplicateName)
            {
                err = QStringLiteral("Esiste già un gruppo con questo nome");
            }
            else
            {
                err = QStringLiteral("Errore rinomina gruppo: ") + QString::fromStdString(res.error().message);
            }
            setLastError(err);
            emit errorOccurred(err);
            return false;
        }

        const QString successMsg = QStringLiteral("Gruppo rinominato in: %1").arg(QString::fromStdString(res->name));
        setLastSuccessMessage(successMsg);
        setStatus(successMsg);
        refreshAll();
        return true;
    }

    bool HomeHubViewModel::addRepositoryToGroup(const QString& groupId, const QString& repoId)
    {
        if (groupId.trimmed().isEmpty() || repoId.trimmed().isEmpty())
        {
            return false;
        }

        auto res = m_groupService.addRepositoryToGroup(
            gf::groups::bo::GroupId{groupId.toStdString()},
            gf::repositories::bo::RepositoryId{repoId.toStdString()}
        );
        if (!res)
        {
            const QString err = QStringLiteral("Errore associazione repository al gruppo: ") +
                QString::fromStdString(res.error().message);
            setLastError(err);
            emit errorOccurred(err);
            return false;
        }

        setLastSuccessMessage(QStringLiteral("Repository aggiunto al gruppo"));
        refreshAll();
        return true;
    }

    bool HomeHubViewModel::removeRepositoryFromGroup(const QString& groupId, const QString& repoId)
    {
        if (groupId.trimmed().isEmpty() || repoId.trimmed().isEmpty())
        {
            return false;
        }

        auto res = m_groupService.removeRepositoryFromGroup(
            gf::groups::bo::GroupId{groupId.toStdString()},
            gf::repositories::bo::RepositoryId{repoId.toStdString()}
        );
        if (!res)
        {
            const QString err = QStringLiteral("Errore rimozione repository dal gruppo: ") +
                QString::fromStdString(res.error().message);
            setLastError(err);
            emit errorOccurred(err);
            return false;
        }

        setLastSuccessMessage(QStringLiteral("Repository rimosso dal gruppo"));
        refreshAll();
        return true;
    }

    void HomeHubViewModel::openRepository(const QString& repoId)
    {
        auto repoResult = m_repoService.getRepository(gf::repositories::bo::RepositoryId{repoId.toStdString()});
        if (!repoResult || !repoResult->has_value())
        {
            emit errorOccurred(QStringLiteral("Repository non trovato"));
            return;
        }

        const auto& repo = **repoResult;
        (void)m_repoService.touchLastAccessed(repo.id);
        emit repositoryOpened(repoId, QString::fromStdString(repo.displayName),
                              QString::fromStdString(repo.localPath.string()));
    }

    void HomeHubViewModel::openGroup(const QString& groupId)
    {
        auto grpResult = m_groupService.getGroup(gf::groups::bo::GroupId{groupId.toStdString()});
        if (!grpResult || !grpResult->has_value())
        {
            emit errorOccurred(QStringLiteral("Gruppo non trovato"));
            return;
        }
        const auto& grp = **grpResult;
        const QString groupName = QString::fromStdString(grp.name);
        QStringList repoIds;
        repoIds.reserve(static_cast<qsizetype>(grp.repositoryIds.size()));
        for (const auto& rId : grp.repositoryIds)
        {
            repoIds.append(QString::fromStdString(rId.value()));
        }
        emit groupOpened(groupId, groupName, repoIds);
    }

    void HomeHubViewModel::openWorkspace(const QString& workspaceId)
    {
        auto wsResult = m_workspaceService.getWorkspace(gf::workspaces::bo::WorkspaceId{workspaceId.toStdString()});
        if (!wsResult || !wsResult->has_value())
        {
            emit errorOccurred(QStringLiteral("Workspace non trovato"));
            return;
        }
        emit workspaceOpened(workspaceId);
    }

    bool HomeHubViewModel::isRepositoryInGroup(const QString& groupId, const QString& repoId) const
    {
        const auto gId = groupId.trimmed().toStdString();
        const auto rId = repoId.trimmed().toStdString();
        if (gId.empty() || rId.empty()) return false;

        auto grpOpt = m_groupService.getGroup(gf::groups::bo::GroupId{gId});
        if (!grpOpt || !grpOpt->has_value()) return false;

        const auto& repoIds = (*grpOpt)->repositoryIds;
        return std::find(repoIds.begin(), repoIds.end(), gf::repositories::bo::RepositoryId{rId}) != repoIds.end();
    }

    bool HomeHubViewModel::isRepositoryInWorkspace(const QString& workspaceId, const QString& repoId) const
    {
        const auto wsId = workspaceId.trimmed().toStdString();
        const auto rId = repoId.trimmed().toStdString();
        if (wsId.empty() || rId.empty()) return false;

        auto reposExpected = m_workspaceService.resolveWorkspaceRepositories(gf::workspaces::bo::WorkspaceId{wsId});
        if (!reposExpected) return false;

        for (const auto& r : *reposExpected)
        {
            if (r.id.value() == rId)
            {
                return true;
            }
        }
        return false;
    }

    QStringList HomeHubViewModel::getRepositoryIdsForGroup(const QString& groupId) const
    {
        QStringList result;
        const auto gId = groupId.trimmed().toStdString();
        if (gId.empty()) return result;

        auto grpOpt = m_groupService.getGroup(gf::groups::bo::GroupId{gId});
        if (!grpOpt || !grpOpt->has_value()) return result;

        for (const auto& rId : (*grpOpt)->repositoryIds)
        {
            result.append(QString::fromStdString(rId.value()));
        }
        return result;
    }

    QStringList HomeHubViewModel::getRepositoryIdsForWorkspace(const QString& workspaceId) const
    {
        QStringList result;
        const auto wsId = workspaceId.trimmed().toStdString();
        if (wsId.empty()) return result;

        auto reposExpected = m_workspaceService.resolveWorkspaceRepositories(gf::workspaces::bo::WorkspaceId{wsId});
        if (!reposExpected) return result;

        for (const auto& r : *reposExpected)
        {
            result.append(QString::fromStdString(r.id.value()));
        }
        return result;
    }

    QVariantList HomeHubViewModel::getAvailableRepositoriesForGroup(const QString& groupId) const
    {
        QVariantList list;
        const auto gId = groupId.trimmed().toStdString();
        if (gId.empty()) return list;

        auto grpOpt = m_groupService.getGroup(gf::groups::bo::GroupId{gId});
        if (!grpOpt || !grpOpt->has_value()) return list;

        const auto& existingRepoIds = (*grpOpt)->repositoryIds;
        auto allRepos = m_repoService.getAllRepositories();
        if (!allRepos) return list;

        for (const auto& repo : *allRepos)
        {
            bool alreadyInGroup = std::find(existingRepoIds.begin(), existingRepoIds.end(), repo.id) != existingRepoIds.
                end();
            if (!alreadyInGroup)
            {
                QVariantMap map;
                const QString rIdQStr = QString::fromStdString(repo.id.value());
                map[QStringLiteral("id")] = rIdQStr;
                map[QStringLiteral("displayName")] = QString::fromStdString(repo.displayName);
                map[QStringLiteral("localPath")] = QString::fromStdString(repo.localPath.string());
                auto cached = m_repoModel ? m_repoModel->findById(rIdQStr) : std::nullopt;
                map[QStringLiteral("branch")] = cached ? cached->branch : QString();
                map[QStringLiteral("isMissing")] = (repo.pathStatus ==
                    gf::repositories::bo::RepositoryPathStatus::Missing);
                list.append(map);
            }
        }
        return list;
    }

    QVariantList HomeHubViewModel::getAvailableRepositoriesForWorkspace(const QString& workspaceId) const
    {
        QVariantList list;
        const auto wsId = workspaceId.trimmed().toStdString();
        if (wsId.empty()) return list;

        auto resolved = m_workspaceService.resolveWorkspaceRepositories(gf::workspaces::bo::WorkspaceId{wsId});
        std::vector<std::string> existingIds;
        if (resolved)
        {
            for (const auto& r : *resolved)
            {
                existingIds.push_back(r.id.value());
            }
        }

        auto allRepos = m_repoService.getAllRepositories();
        if (!allRepos) return list;

        for (const auto& repo : *allRepos)
        {
            bool alreadyInWs = std::find(existingIds.begin(), existingIds.end(), repo.id.value()) != existingIds.end();
            if (!alreadyInWs)
            {
                QVariantMap map;
                const QString rIdQStr = QString::fromStdString(repo.id.value());
                map[QStringLiteral("id")] = rIdQStr;
                map[QStringLiteral("displayName")] = QString::fromStdString(repo.displayName);
                map[QStringLiteral("localPath")] = QString::fromStdString(repo.localPath.string());
                auto cached = m_repoModel ? m_repoModel->findById(rIdQStr) : std::nullopt;
                map[QStringLiteral("branch")] = cached ? cached->branch : QString();
                map[QStringLiteral("isMissing")] = (repo.pathStatus ==
                    gf::repositories::bo::RepositoryPathStatus::Missing);
                list.append(map);
            }
        }
        return list;
    }

    QVariantList HomeHubViewModel::getAvailableGroupsForRepository(const QString& repoId) const
    {
        QVariantList list;
        const auto rId = repoId.trimmed().toStdString();
        if (rId.empty()) return list;

        auto allGroups = m_groupService.getAllGroups();
        if (!allGroups) return list;

        for (const auto& grp : *allGroups)
        {
            bool containsRepo = std::find(grp.repositoryIds.begin(), grp.repositoryIds.end(),
                                          gf::repositories::bo::RepositoryId{rId}) != grp.repositoryIds.end();
            if (!containsRepo)
            {
                QVariantMap map;
                map[QStringLiteral("id")] = QString::fromStdString(grp.id.value());
                map[QStringLiteral("name")] = QString::fromStdString(grp.name);
                map[QStringLiteral("repositoryCount")] = static_cast<int>(grp.repositoryIds.size());
                list.append(map);
            }
        }
        return list;
    }

    bool HomeHubViewModel::addRepositoryToWorkspace(const QString& workspaceId, const QString& repoId)
    {
        const auto wsIdStr = workspaceId.trimmed().toStdString();
        const auto rIdStr = repoId.trimmed().toStdString();
        if (wsIdStr.empty() || rIdStr.empty())
        {
            const QString err = QStringLiteral("Parametri non validi per l'aggiunta al workspace.");
            setLastError(err);
            emit errorOccurred(err);
            return false;
        }

        auto wsOptExp = m_workspaceService.getWorkspace(gf::workspaces::bo::WorkspaceId{wsIdStr});
        if (!wsOptExp || !wsOptExp->has_value())
        {
            const QString err = QStringLiteral("Workspace non trovato.");
            setLastError(err);
            emit errorOccurred(err);
            return false;
        }

        auto ws = **wsOptExp;
        gf::repositories::bo::RepositoryId targetRepoId{rIdStr};
        auto it = std::find(ws.directRepositoryIds.begin(), ws.directRepositoryIds.end(), targetRepoId);
        if (it != ws.directRepositoryIds.end())
        {
            const QString err = QStringLiteral("Il repository fa già parte di questo workspace.");
            setLastError(err);
            emit errorOccurred(err);
            return false;
        }

        ws.directRepositoryIds.push_back(targetRepoId);
        auto updateRes = m_workspaceService.updateWorkspaceComposition(ws.id, ws.groupIds, ws.directRepositoryIds);
        if (!updateRes)
        {
            const QString err = QStringLiteral("Impossibile aggiornare la composizione del workspace.");
            setLastError(err);
            emit errorOccurred(err);
            return false;
        }

        setLastSuccessMessage(
            QStringLiteral("Repository aggiunto al workspace '%1'").arg(QString::fromStdString(ws.name)));
        refreshAll();
        emit workspacesChanged();
        return true;
    }

    bool HomeHubViewModel::removeRepositoryFromWorkspace(const QString& workspaceId, const QString& repoId)
    {
        const auto wsIdStr = workspaceId.trimmed().toStdString();
        const auto rIdStr = repoId.trimmed().toStdString();
        if (wsIdStr.empty() || rIdStr.empty()) return false;

        auto wsOptExp = m_workspaceService.getWorkspace(gf::workspaces::bo::WorkspaceId{wsIdStr});
        if (!wsOptExp || !wsOptExp->has_value()) return false;

        auto ws = **wsOptExp;
        gf::repositories::bo::RepositoryId targetRepoId{rIdStr};
        auto it = std::find(ws.directRepositoryIds.begin(), ws.directRepositoryIds.end(), targetRepoId);
        if (it == ws.directRepositoryIds.end()) return false;

        ws.directRepositoryIds.erase(it);
        auto updateRes = m_workspaceService.updateWorkspaceComposition(ws.id, ws.groupIds, ws.directRepositoryIds);
        if (!updateRes) return false;

        setLastSuccessMessage(QStringLiteral("Repository rimosso dal workspace"));
        refreshAll();
        emit workspacesChanged();
        return true;
    }

    bool HomeHubViewModel::addGroupToWorkspace(const QString& workspaceId, const QString& groupId)
    {
        const auto wsIdStr = workspaceId.trimmed().toStdString();
        const auto gIdStr = groupId.trimmed().toStdString();
        if (wsIdStr.empty() || gIdStr.empty())
        {
            const QString err = QStringLiteral("Parametri non validi per l'aggiunta del gruppo al workspace.");
            setLastError(err);
            emit errorOccurred(err);
            return false;
        }

        auto wsOptExp = m_workspaceService.getWorkspace(gf::workspaces::bo::WorkspaceId{wsIdStr});
        if (!wsOptExp || !wsOptExp->has_value())
        {
            const QString err = QStringLiteral("Workspace non trovato.");
            setLastError(err);
            emit errorOccurred(err);
            return false;
        }

        auto ws = **wsOptExp;
        gf::groups::bo::GroupId targetGrpId{gIdStr};
        auto it = std::find(ws.groupIds.begin(), ws.groupIds.end(), targetGrpId);
        if (it != ws.groupIds.end())
        {
            const QString err = QStringLiteral("Il gruppo fa già parte di questo workspace.");
            setLastError(err);
            emit errorOccurred(err);
            return false;
        }

        ws.groupIds.push_back(targetGrpId);
        auto updateRes = m_workspaceService.updateWorkspaceComposition(ws.id, ws.groupIds, ws.directRepositoryIds);
        if (!updateRes)
        {
            const QString err = QStringLiteral("Impossibile aggiornare la composizione del workspace.");
            setLastError(err);
            emit errorOccurred(err);
            return false;
        }

        setLastSuccessMessage(QStringLiteral("Gruppo aggiunto al workspace '%1'").arg(QString::fromStdString(ws.name)));
        refreshAll();
        emit workspacesChanged();
        return true;
    }

    bool HomeHubViewModel::removeGroupFromWorkspace(const QString& workspaceId, const QString& groupId)
    {
        const auto wsIdStr = workspaceId.trimmed().toStdString();
        const auto gIdStr = groupId.trimmed().toStdString();
        if (wsIdStr.empty() || gIdStr.empty()) return false;

        auto wsOptExp = m_workspaceService.getWorkspace(gf::workspaces::bo::WorkspaceId{wsIdStr});
        if (!wsOptExp || !wsOptExp->has_value()) return false;

        auto ws = **wsOptExp;
        gf::groups::bo::GroupId targetGrpId{gIdStr};
        auto it = std::find(ws.groupIds.begin(), ws.groupIds.end(), targetGrpId);
        if (it == ws.groupIds.end()) return false;

        ws.groupIds.erase(it);
        auto updateRes = m_workspaceService.updateWorkspaceComposition(ws.id, ws.groupIds, ws.directRepositoryIds);
        if (!updateRes) return false;

        setLastSuccessMessage(QStringLiteral("Gruppo rimosso dal workspace"));
        refreshAll();
        emit workspacesChanged();
        return true;
    }

    bool HomeHubViewModel::isGroupInWorkspace(const QString& workspaceId, const QString& groupId) const
    {
        const auto wsIdStr = workspaceId.trimmed().toStdString();
        const auto gIdStr = groupId.trimmed().toStdString();
        if (wsIdStr.empty() || gIdStr.empty()) return false;

        auto wsOptExp = m_workspaceService.getWorkspace(gf::workspaces::bo::WorkspaceId{wsIdStr});
        if (!wsOptExp || !wsOptExp->has_value()) return false;

        const auto& grpIds = (*wsOptExp)->groupIds;
        return std::find(grpIds.begin(), grpIds.end(), gf::groups::bo::GroupId{gIdStr}) != grpIds.end();
    }

    QVariantList HomeHubViewModel::getAvailableGroupsForWorkspace(const QString& workspaceId) const
    {
        QVariantList list;
        const auto wsIdStr = workspaceId.trimmed().toStdString();
        if (wsIdStr.empty()) return list;

        auto wsOptExp = m_workspaceService.getWorkspace(gf::workspaces::bo::WorkspaceId{wsIdStr});
        if (!wsOptExp || !wsOptExp->has_value()) return list;

        const auto& existingGrpIds = (*wsOptExp)->groupIds;
        auto allGroups = m_groupService.getAllGroups();
        if (!allGroups) return list;

        for (const auto& grp : *allGroups)
        {
            bool alreadyInWs = std::find(existingGrpIds.begin(), existingGrpIds.end(), grp.id) != existingGrpIds.end();
            if (!alreadyInWs)
            {
                QVariantMap map;
                map[QStringLiteral("id")] = QString::fromStdString(grp.id.value());
                map[QStringLiteral("name")] = QString::fromStdString(grp.name);
                map[QStringLiteral("repositoryCount")] = static_cast<int>(grp.repositoryIds.size());
                list.append(map);
            }
        }
        return list;
    }
} // namespace gf::desktop
