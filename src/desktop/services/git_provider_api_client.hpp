#pragma once

#include "core/modules/git/bo/git_provider_profile.hpp"
#include <QObject>
#include <QNetworkAccessManager>
#include <QString>
#include <vector>

namespace gf::desktop::services {

class GitProviderApiClient : public QObject {
    Q_OBJECT

public:
    explicit GitProviderApiClient(QNetworkAccessManager* nam, QObject* parent = nullptr);
    ~GitProviderApiClient() override = default;

    void fetchRepositories(
        const QString& profileId,
        const core::git::bo::GitProviderProfile& profile,
        const QString& decryptedToken
    );

signals:
    void repositoriesFetched(const QString& profileId, const std::vector<core::git::bo::RemoteRepositoryItem>& repos);
    void fetchFailed(const QString& profileId, const QString& errorMessage);

private:
    QNetworkAccessManager* m_nam;

    void fetchGitHub(const QString& profileId, const core::git::bo::GitProviderProfile& profile, const QString& token);
    void fetchGitLab(const QString& profileId, const core::git::bo::GitProviderProfile& profile, const QString& token);
    void fetchBitbucket(const QString& profileId, const core::git::bo::GitProviderProfile& profile, const QString& token);
    void fetchCustom(const QString& profileId, const core::git::bo::GitProviderProfile& profile, const QString& token);
};

} // namespace gf::desktop::services
