#pragma once

#include "core/modules/git/bl/multi_repo_batch_coordinator.hpp"

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantList>
#include <memory>

namespace gf::desktop {

class BatchOperationsViewModel final : public QObject {
    Q_OBJECT

    Q_PROPERTY(bool isRunning READ isRunning NOTIFY isRunningChanged)
    Q_PROPERTY(int progressPercentage READ progressPercentage NOTIFY progressChanged)
    Q_PROPERTY(QString currentOperationName READ currentOperationName NOTIFY progressChanged)
    Q_PROPERTY(QString currentRepoName READ currentRepoName NOTIFY progressChanged)
    Q_PROPERTY(QString statusMessage READ statusMessage NOTIFY statusMessageChanged)
    Q_PROPERTY(int totalRepos READ totalRepos NOTIFY progressChanged)
    Q_PROPERTY(int completedRepos READ completedRepos NOTIFY progressChanged)
    Q_PROPERTY(int succeededCount READ succeededCount NOTIFY reportChanged)
    Q_PROPERTY(int failedCount READ failedCount NOTIFY reportChanged)
    Q_PROPERTY(QVariantList lastReportItems READ lastReportItems NOTIFY reportChanged)

public:
    explicit BatchOperationsViewModel(gf::git::bl::MultiRepoBatchCoordinator& coordinator,
                                     QObject* parent = nullptr);
    ~BatchOperationsViewModel() override = default;

    [[nodiscard]] bool isRunning() const noexcept { return m_isRunning; }
    [[nodiscard]] int progressPercentage() const noexcept { return m_progressPercentage; }
    [[nodiscard]] QString currentOperationName() const { return m_currentOperationName; }
    [[nodiscard]] QString currentRepoName() const { return m_currentRepoName; }
    [[nodiscard]] QString statusMessage() const { return m_statusMessage; }
    [[nodiscard]] int totalRepos() const noexcept { return m_totalRepos; }
    [[nodiscard]] int completedRepos() const noexcept { return m_completedRepos; }
    [[nodiscard]] int succeededCount() const noexcept { return m_succeededCount; }
    [[nodiscard]] int failedCount() const noexcept { return m_failedCount; }
    [[nodiscard]] QVariantList lastReportItems() const { return m_lastReportItems; }

    Q_INVOKABLE void fetchGroup(const QString& groupId, const QString& remoteName = "origin");
    Q_INVOKABLE void fetchWorkspace(const QString& workspaceId, const QString& remoteName = "origin");
    Q_INVOKABLE void fetchRepositories(const QStringList& repoIds, const QString& remoteName = "origin");
    Q_INVOKABLE void pullRepositories(const QStringList& repoIds, const QString& remoteName = "origin");
    Q_INVOKABLE void checkStatusRepositories(const QStringList& repoIds);
    Q_INVOKABLE bool cancelCurrentOperation();

signals:
    void isRunningChanged();
    void progressChanged();
    void statusMessageChanged();
    void reportChanged();
    void operationCompleted(const QString& summary);
    void operationFailed(const QString& error);

private:
    void onProgressUpdate(const gf::git::bo::BatchProgressUpdate& update);
    void onReportCompleted(const gf::git::bo::BatchOperationReport& report);

    gf::git::bl::MultiRepoBatchCoordinator& m_coordinator;

    bool m_isRunning{false};
    int m_progressPercentage{0};
    QString m_currentOperationName{};
    QString m_currentRepoName{};
    QString m_statusMessage{};
    int m_totalRepos{0};
    int m_completedRepos{0};
    int m_succeededCount{0};
    int m_failedCount{0};
    QVariantList m_lastReportItems{};

    QString m_currentOperationId{};
};

} // namespace gf::desktop
