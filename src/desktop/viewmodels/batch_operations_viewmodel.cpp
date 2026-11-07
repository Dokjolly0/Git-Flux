#include "desktop/viewmodels/batch_operations_viewmodel.hpp"

#include "core/modules/git/bl/multi_repo_batch_coordinator.hpp"

#include <QMetaObject>
#include <QVariantMap>

namespace gf::desktop {

BatchOperationsViewModel::BatchOperationsViewModel(gf::git::bl::MultiRepoBatchCoordinator& coordinator,
                                                   QObject* parent)
    : QObject(parent)
    , m_coordinator(coordinator) {}

void BatchOperationsViewModel::fetchGroup(const QString& groupId, const QString& remoteName) {
    if (groupId.isEmpty() || m_isRunning) {
        return;
    }

    m_isRunning = true;
    m_progressPercentage = 0;
    m_currentOperationName = "Fetch Gruppo";
    m_statusMessage = "Avvio fetch su gruppo...";
    m_currentRepoName.clear();
    emit isRunningChanged();
    emit progressChanged();
    emit statusMessageChanged();

    m_currentOperationId = QString::fromStdString(m_coordinator.executeGroupFetchAsync(
        groupId.toStdString(),
        remoteName.toStdString(),
        [this](const gf::git::bo::BatchProgressUpdate& update) {
            onProgressUpdate(update);
        },
        [this](const gf::git::bo::BatchOperationReport& report) {
            onReportCompleted(report);
        }
    ));
}

void BatchOperationsViewModel::fetchWorkspace(const QString& workspaceId, const QString& remoteName) {
    if (workspaceId.isEmpty() || m_isRunning) {
        return;
    }

    m_isRunning = true;
    m_progressPercentage = 0;
    m_currentOperationName = "Fetch Workspace";
    m_statusMessage = "Avvio fetch su workspace...";
    m_currentRepoName.clear();
    emit isRunningChanged();
    emit progressChanged();
    emit statusMessageChanged();

    m_currentOperationId = QString::fromStdString(m_coordinator.executeWorkspaceFetchAsync(
        workspaceId.toStdString(),
        remoteName.toStdString(),
        [this](const gf::git::bo::BatchProgressUpdate& update) {
            onProgressUpdate(update);
        },
        [this](const gf::git::bo::BatchOperationReport& report) {
            onReportCompleted(report);
        }
    ));
}

void BatchOperationsViewModel::fetchRepositories(const QStringList& repoIds, const QString& remoteName) {
    if (repoIds.isEmpty() || m_isRunning) {
        return;
    }

    std::vector<std::string> ids;
    ids.reserve(repoIds.size());
    for (const auto& id : repoIds) {
        ids.push_back(id.toStdString());
    }

    auto targets = m_coordinator.resolveTargets(ids);
    if (targets.empty()) {
        emit operationFailed("Nessun repository valido selezionato");
        return;
    }

    m_isRunning = true;
    m_progressPercentage = 0;
    m_currentOperationName = "Fetch Selezionati";
    m_statusMessage = "Avvio fetch su repository...";
    m_currentRepoName.clear();
    emit isRunningChanged();
    emit progressChanged();
    emit statusMessageChanged();

    m_currentOperationId = QString::fromStdString(m_coordinator.executeBatchFetchAsync(
        targets,
        remoteName.toStdString(),
        [this](const gf::git::bo::BatchProgressUpdate& update) {
            onProgressUpdate(update);
        },
        [this](const gf::git::bo::BatchOperationReport& report) {
            onReportCompleted(report);
        }
    ));
}

void BatchOperationsViewModel::pullRepositories(const QStringList& repoIds, const QString& remoteName) {
    if (repoIds.isEmpty() || m_isRunning) {
        return;
    }

    std::vector<std::string> ids;
    ids.reserve(repoIds.size());
    for (const auto& id : repoIds) {
        ids.push_back(id.toStdString());
    }

    auto targets = m_coordinator.resolveTargets(ids);
    if (targets.empty()) {
        emit operationFailed("Nessun repository valido selezionato");
        return;
    }

    m_isRunning = true;
    m_progressPercentage = 0;
    m_currentOperationName = "Pull Selezionati";
    m_statusMessage = "Avvio pull su repository...";
    m_currentRepoName.clear();
    emit isRunningChanged();
    emit progressChanged();
    emit statusMessageChanged();

    m_currentOperationId = QString::fromStdString(m_coordinator.executeBatchPullAsync(
        targets,
        remoteName.toStdString(),
        [this](const gf::git::bo::BatchProgressUpdate& update) {
            onProgressUpdate(update);
        },
        [this](const gf::git::bo::BatchOperationReport& report) {
            onReportCompleted(report);
        }
    ));
}

void BatchOperationsViewModel::checkStatusRepositories(const QStringList& repoIds) {
    if (repoIds.isEmpty() || m_isRunning) {
        return;
    }

    std::vector<std::string> ids;
    ids.reserve(repoIds.size());
    for (const auto& id : repoIds) {
        ids.push_back(id.toStdString());
    }

    auto targets = m_coordinator.resolveTargets(ids);
    if (targets.empty()) {
        emit operationFailed("Nessun repository valido selezionato");
        return;
    }

    m_isRunning = true;
    m_progressPercentage = 0;
    m_currentOperationName = "Verifica Stato";
    m_statusMessage = "Scansione working tree su repository...";
    m_currentRepoName.clear();
    emit isRunningChanged();
    emit progressChanged();
    emit statusMessageChanged();

    m_currentOperationId = QString::fromStdString(m_coordinator.executeBatchStatusAsync(
        targets,
        [this](const gf::git::bo::BatchProgressUpdate& update) {
            onProgressUpdate(update);
        },
        [this](const gf::git::bo::BatchOperationReport& report) {
            onReportCompleted(report);
        }
    ));
}

bool BatchOperationsViewModel::cancelCurrentOperation() {
    if (!m_isRunning || m_currentOperationId.isEmpty()) {
        return false;
    }

    return m_coordinator.cancelBatch(m_currentOperationId.toStdString());
}

void BatchOperationsViewModel::onProgressUpdate(const gf::git::bo::BatchProgressUpdate& update) {
    QMetaObject::invokeMethod(this, [this, update]() {
        m_progressPercentage = update.percentage();
        m_totalRepos = static_cast<int>(update.totalCount);
        m_completedRepos = static_cast<int>(update.completedCount);
        m_currentRepoName = QString::fromStdString(update.currentRepoName);
        m_statusMessage = QString::fromStdString(update.currentMessage);

        emit progressChanged();
        emit statusMessageChanged();
    }, Qt::QueuedConnection);
}

void BatchOperationsViewModel::onReportCompleted(const gf::git::bo::BatchOperationReport& report) {
    QMetaObject::invokeMethod(this, [this, report]() {
        m_isRunning = false;
        m_currentOperationId.clear();
        m_progressPercentage = 100;
        m_succeededCount = static_cast<int>(report.succeededCount);
        m_failedCount = static_cast<int>(report.failedCount);

        QVariantList itemsList;
        itemsList.reserve(static_cast<qsizetype>(report.items.size()));

        for (const auto& item : report.items) {
            QVariantMap map;
            map["repoId"] = QString::fromStdString(item.repoId);
            map["repoName"] = QString::fromStdString(item.repoName);
            map["status"] = static_cast<int>(item.status);
            map["isSuccess"] = item.isSuccess();
            map["isFailed"] = item.isFailed();
            map["message"] = QString::fromStdString(item.message);
            map["durationMs"] = static_cast<qlonglong>(item.durationMs.count());
            itemsList.append(map);
        }

        m_lastReportItems = itemsList;

        QString summary = QString("%1 completato: %2 riusciti, %3 falliti")
                              .arg(m_currentOperationName)
                              .arg(m_succeededCount)
                              .arg(m_failedCount);

        m_statusMessage = summary;

        emit isRunningChanged();
        emit progressChanged();
        emit statusMessageChanged();
        emit reportChanged();

        if (report.hasFailures()) {
            emit operationFailed(summary);
        } else {
            emit operationCompleted(summary);
        }
    }, Qt::QueuedConnection);
}

} // namespace gf::desktop
