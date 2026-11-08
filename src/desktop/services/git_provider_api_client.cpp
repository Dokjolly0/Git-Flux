#include "desktop/services/git_provider_api_client.hpp"

#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QUrl>

namespace gf::desktop::services {

GitProviderApiClient::GitProviderApiClient(QNetworkAccessManager* nam, QObject* parent)
    : QObject(parent), m_nam(nam) {}

void GitProviderApiClient::fetchRepositories(
    const QString& profileId,
    const core::git::bo::GitProviderProfile& profile,
    const QString& decryptedToken
) {
    switch (profile.providerType) {
        case core::git::bo::GitProviderType::GitHub:
            fetchGitHub(profileId, profile, decryptedToken);
            break;
        case core::git::bo::GitProviderType::GitLab:
            fetchGitLab(profileId, profile, decryptedToken);
            break;
        case core::git::bo::GitProviderType::Bitbucket:
            fetchBitbucket(profileId, profile, decryptedToken);
            break;
        case core::git::bo::GitProviderType::Custom:
            fetchCustom(profileId, profile, decryptedToken);
            break;
    }
}

void GitProviderApiClient::fetchGitHub(
    const QString& profileId,
    const core::git::bo::GitProviderProfile& profile,
    const QString& token
) {
    QUrl url;
    if (!token.isEmpty()) {
        url = QUrl(QStringLiteral("https://api.github.com/user/repos?per_page=100&sort=updated&visibility=all&affiliation=owner,collaborator,organization_member"));
    } else if (!profile.username.empty()) {
        url = QUrl(QStringLiteral("https://api.github.com/users/%1/repos?per_page=100&sort=updated")
                       .arg(QString::fromStdString(profile.username)));
    } else {
        emit fetchFailed(profileId, QStringLiteral("Nessun token o username GitHub configurato."));
        return;
    }

    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("GitFlux-Desktop"));
    request.setRawHeader("Accept", "application/vnd.github+json");
    request.setRawHeader("X-GitHub-Api-Version", "2022-11-28");
    if (!token.isEmpty()) {
        request.setRawHeader("Authorization", QStringLiteral("Bearer %1").arg(token).toUtf8());
    }

    auto* reply = m_nam->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply, profileId]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            const int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            if (statusCode == 401) {
                emit fetchFailed(profileId, QStringLiteral("Token non valido o scaduto (HTTP 401). Verifica il token inserito."));
            } else if (statusCode == 403) {
                emit fetchFailed(profileId, QStringLiteral("Permessi insufficienti (HTTP 403). Assicurati che il token abbia lo scope 'repo' per visualizzare i repository privati."));
            } else {
                emit fetchFailed(profileId, QStringLiteral("Errore API GitHub (%1): %2").arg(statusCode).arg(reply->errorString()));
            }
            return;
        }

        const auto data = reply->readAll();
        QJsonParseError parseErr;
        const auto doc = QJsonDocument::fromJson(data, &parseErr);
        if (parseErr.error != QJsonParseError::NoError || !doc.isArray()) {
            emit fetchFailed(profileId, QStringLiteral("Risposta JSON GitHub non valida"));
            return;
        }

        std::vector<core::git::bo::RemoteRepositoryItem> items;
        const auto arr = doc.array();
        items.reserve(arr.size());

        for (const auto& val : arr) {
            if (!val.isObject()) continue;
            const auto obj = val.toObject();

            core::git::bo::RemoteRepositoryItem item;
            item.name = obj.value(QStringLiteral("name")).toString().toStdString();
            item.fullName = obj.value(QStringLiteral("full_name")).toString().toStdString();
            item.description = obj.value(QStringLiteral("description")).toString().toStdString();
            item.isPrivate = obj.value(QStringLiteral("private")).toBool();
            item.defaultBranch = obj.value(QStringLiteral("default_branch")).toString(QStringLiteral("main")).toStdString();
            item.httpsUrl = obj.value(QStringLiteral("clone_url")).toString().toStdString();
            item.sshUrl = obj.value(QStringLiteral("ssh_url")).toString().toStdString();
            item.updatedAt = obj.value(QStringLiteral("updated_at")).toString().toStdString();
            item.starsCount = obj.value(QStringLiteral("stargazers_count")).toInt();
            item.forksCount = obj.value(QStringLiteral("forks_count")).toInt();

            items.push_back(std::move(item));
        }

        emit repositoriesFetched(profileId, items);
    });
}

void GitProviderApiClient::fetchGitLab(
    const QString& profileId,
    const core::git::bo::GitProviderProfile& profile,
    const QString& token
) {
    QString baseUrl = profile.customApiUrl.empty()
                          ? QStringLiteral("https://gitlab.com/api/v4")
                          : QString::fromStdString(profile.customApiUrl);
    if (baseUrl.endsWith(QLatin1Char('/'))) baseUrl.chop(1);

    const QUrl url(QStringLiteral("%1/projects?membership=true&per_page=100&order_by=updated_at").arg(baseUrl));
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("GitFlux-Desktop"));
    if (!token.isEmpty()) {
        request.setRawHeader("PRIVATE-TOKEN", token.toUtf8());
    }

    auto* reply = m_nam->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply, profileId]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            emit fetchFailed(profileId, QStringLiteral("Errore API GitLab: %1").arg(reply->errorString()));
            return;
        }

        const auto doc = QJsonDocument::fromJson(reply->readAll());
        if (!doc.isArray()) {
            emit fetchFailed(profileId, QStringLiteral("Risposta JSON GitLab non valida"));
            return;
        }

        std::vector<core::git::bo::RemoteRepositoryItem> items;
        const auto arr = doc.array();
        items.reserve(arr.size());

        for (const auto& val : arr) {
            if (!val.isObject()) continue;
            const auto obj = val.toObject();

            core::git::bo::RemoteRepositoryItem item;
            item.name = obj.value(QStringLiteral("name")).toString().toStdString();
            item.fullName = obj.value(QStringLiteral("path_with_namespace")).toString().toStdString();
            item.description = obj.value(QStringLiteral("description")).toString().toStdString();
            const auto vis = obj.value(QStringLiteral("visibility")).toString();
            item.isPrivate = (vis == QStringLiteral("private") || vis == QStringLiteral("internal"));
            item.defaultBranch = obj.value(QStringLiteral("default_branch")).toString(QStringLiteral("main")).toStdString();
            item.httpsUrl = obj.value(QStringLiteral("http_url_to_repo")).toString().toStdString();
            item.sshUrl = obj.value(QStringLiteral("ssh_url_to_repo")).toString().toStdString();
            item.updatedAt = obj.value(QStringLiteral("last_activity_at")).toString().toStdString();

            items.push_back(std::move(item));
        }

        emit repositoriesFetched(profileId, items);
    });
}

void GitProviderApiClient::fetchBitbucket(
    const QString& profileId,
    const core::git::bo::GitProviderProfile& profile,
    const QString& token
) {
    if (token.isEmpty()) {
        emit fetchFailed(profileId, QStringLiteral("Inserisci un App Password o Token per caricare i repository Bitbucket."));
        return;
    }

    const QUrl url(QStringLiteral("https://api.bitbucket.org/2.0/repositories?role=member&pagelen=100&sort=-updated_on"));
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("GitFlux-Desktop"));
    request.setRawHeader("Accept", "application/json");

    if (!profile.username.empty()) {
        const QString credentials = QStringLiteral("%1:%2").arg(QString::fromStdString(profile.username), token);
        const QString authHeader = QStringLiteral("Basic %1").arg(QString::fromLatin1(credentials.toUtf8().toBase64()));
        request.setRawHeader("Authorization", authHeader.toUtf8());
    } else {
        request.setRawHeader("Authorization", QStringLiteral("Bearer %1").arg(token).toUtf8());
    }

    auto* reply = m_nam->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply, profileId]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            const int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            if (statusCode == 401) {
                emit fetchFailed(profileId, QStringLiteral("Autenticazione fallita (HTTP 401). Verifica Bitbucket Username e App Password (in Personal settings > App passwords con permesso 'Repositories: Read')."));
            } else if (statusCode == 403) {
                emit fetchFailed(profileId, QStringLiteral("Accesso negato (HTTP 403). Assicurati che l'App Password abbia i permessi di lettura per i repository."));
            } else {
                emit fetchFailed(profileId, QStringLiteral("Errore API Bitbucket (%1): %2").arg(statusCode).arg(reply->errorString()));
            }
            return;
        }

        const auto doc = QJsonDocument::fromJson(reply->readAll());
        if (!doc.isObject()) {
            emit fetchFailed(profileId, QStringLiteral("Risposta JSON Bitbucket non valida"));
            return;
        }

        const auto rootObj = doc.object();
        const auto arr = rootObj.value(QStringLiteral("values")).toArray();

        std::vector<core::git::bo::RemoteRepositoryItem> items;
        items.reserve(arr.size());

        for (const auto& val : arr) {
            if (!val.isObject()) continue;
            const auto obj = val.toObject();

            core::git::bo::RemoteRepositoryItem item;
            item.name = obj.value(QStringLiteral("name")).toString().toStdString();
            item.fullName = obj.value(QStringLiteral("full_name")).toString().toStdString();
            const auto desc = obj.value(QStringLiteral("description")).toString();
            item.description = desc.toStdString();
            item.isPrivate = obj.value(QStringLiteral("is_private")).toBool();

            const auto links = obj.value(QStringLiteral("links")).toObject();
            const auto cloneLinks = links.value(QStringLiteral("clone")).toArray();
            for (const auto& cl : cloneLinks) {
                const auto cObj = cl.toObject();
                const auto cName = cObj.value(QStringLiteral("name")).toString();
                const auto href = cObj.value(QStringLiteral("href")).toString().toStdString();
                if (cName == QStringLiteral("https")) {
                    item.httpsUrl = href;
                } else if (cName == QStringLiteral("ssh")) {
                    item.sshUrl = href;
                }
            }

            item.updatedAt = obj.value(QStringLiteral("updated_on")).toString().toStdString();
            items.push_back(std::move(item));
        }

        emit repositoriesFetched(profileId, items);
    });
}

void GitProviderApiClient::fetchCustom(
    const QString& profileId,
    const core::git::bo::GitProviderProfile& profile,
    const QString& token
) {
    QString apiUrl = profile.customApiUrl.empty()
                         ? QStringLiteral("https://%1").arg(QString::fromStdString(profile.host))
                         : QString::fromStdString(profile.customApiUrl);

    // Strip accidental SSH port like :2222 if present
    if (apiUrl.contains(QStringLiteral(":2222"))) {
        apiUrl.replace(QStringLiteral(":2222"), QStringLiteral(""));
    }
    if (apiUrl.endsWith(QLatin1Char('/'))) apiUrl.chop(1);

    QString baseApi = apiUrl;
    if (!baseApi.contains(QStringLiteral("/api/v1"))) {
        baseApi.append(QStringLiteral("/api/v1"));
    }

    QUrl url;
    if (!token.isEmpty()) {
        url = QUrl(baseApi + QStringLiteral("/user/repos?limit=100"));
    } else {
        // Without token: search all public repositories on Gitea/Forgejo instance
        url = QUrl(baseApi + QStringLiteral("/repos/search?limit=100"));
    }

    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("GitFlux-Desktop"));
    request.setRawHeader("Accept", "application/json");
    if (!token.isEmpty()) {
        request.setRawHeader("Authorization", QStringLiteral("token %1").arg(token).toUtf8());
    }

    auto* reply = m_nam->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply, profileId]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            const int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            emit fetchFailed(profileId, QStringLiteral("Errore connessione server personalizzato (%1): %2").arg(statusCode).arg(reply->errorString()));
            return;
        }

        const auto doc = QJsonDocument::fromJson(reply->readAll());
        QJsonArray arr;
        if (doc.isArray()) {
            arr = doc.array();
        } else if (doc.isObject()) {
            const auto obj = doc.object();
            if (obj.contains(QStringLiteral("data")) && obj.value(QStringLiteral("data")).isArray()) {
                arr = obj.value(QStringLiteral("data")).toArray();
            }
        }

        if (arr.isEmpty() && !doc.isArray() && (!doc.isObject() || !doc.object().contains(QStringLiteral("data")))) {
            emit fetchFailed(profileId, QStringLiteral("Risposta non valida dal server personalizzato"));
            return;
        }

        std::vector<core::git::bo::RemoteRepositoryItem> items;
        items.reserve(arr.size());

        for (const auto& val : arr) {
            if (!val.isObject()) continue;
            const auto obj = val.toObject();

            core::git::bo::RemoteRepositoryItem item;
            item.name = obj.value(QStringLiteral("name")).toString().toStdString();
            item.fullName = obj.value(QStringLiteral("full_name")).toString().toStdString();
            item.description = obj.value(QStringLiteral("description")).toString().toStdString();
            item.isPrivate = obj.value(QStringLiteral("private")).toBool();
            item.httpsUrl = obj.value(QStringLiteral("clone_url")).toString().toStdString();
            item.sshUrl = obj.value(QStringLiteral("ssh_url")).toString().toStdString();
            item.defaultBranch = obj.value(QStringLiteral("default_branch")).toString(QStringLiteral("main")).toStdString();
            item.starsCount = obj.value(QStringLiteral("stars_count")).toInt();
            item.forksCount = obj.value(QStringLiteral("forks_count")).toInt();
            item.updatedAt = obj.value(QStringLiteral("updated_at")).toString().toStdString();

            items.push_back(std::move(item));
        }

        emit repositoriesFetched(profileId, items);
    });
}

} // namespace gf::desktop::services
