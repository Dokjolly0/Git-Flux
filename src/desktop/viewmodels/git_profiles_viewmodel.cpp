#include "desktop/viewmodels/git_profiles_viewmodel.hpp"
#include "core/shared/env_var.hpp"
#include <algorithm>
#include <optional>

namespace gf::desktop::viewmodels {

GitProfilesViewModel::GitProfilesViewModel(
    infra::persistence::profiles::SqliteProfileStore& store,
    infra::security::CryptoVault& vault,
    infra::git::credentials::SystemCredentialDetector& detector,
    services::GitProviderApiClient* apiClient,
    QObject* parent
)
    : QObject(parent),
      m_store(store),
      m_vault(vault),
      m_detector(detector),
      m_apiClient(apiClient),
      m_repoModel(new RemoteRepoListModel(this))
{
    if (m_apiClient) {
        connect(m_apiClient, &services::GitProviderApiClient::repositoriesFetched,
                this, [this](const QString& profileId, const std::vector<core::git::bo::RemoteRepositoryItem>& repos) {
            if (profileId == m_activeProfileId) {
                m_isFetching = false;
                m_fetchError.clear();
                m_repoModel->setRepositories(repos);
                emit isFetchingChanged();
                emit fetchErrorChanged();
            }
        });

        connect(m_apiClient, &services::GitProviderApiClient::fetchFailed,
                this, [this](const QString& profileId, const QString& errorMessage) {
            if (profileId == m_activeProfileId) {
                m_isFetching = false;
                m_fetchError = errorMessage;
                m_repoModel->clear();
                emit isFetchingChanged();
                emit fetchErrorChanged();
            }
        });
    }
}

void GitProfilesViewModel::initialize() {
    // 1. Detect system credentials and SSH hosts
    auto detected = m_detector.detectProfiles();

    // 2. Load stored profiles from SQLite
    auto storedRes = m_store.getAllProfiles();
    std::vector<core::git::bo::GitProviderProfile> stored;
    if (storedRes) {
        stored = std::move(*storedRes);
    }

    // Purge any stored custom profiles that are NOT git hosts (cleans up any old non-git VPS servers)
    for (auto it = stored.begin(); it != stored.end();) {
        if (it->id.starts_with("custom_")) {
            std::string h = it->host;
            std::transform(h.begin(), h.end(), h.begin(), ::tolower);
            std::string d = it->displayName;
            std::transform(d.begin(), d.end(), d.begin(), ::tolower);
            std::string u = it->username;
            std::transform(u.begin(), u.end(), u.begin(), ::tolower);

            bool isGit = (u == "git") ||
                         (h.find("git") != std::string::npos) ||
                         (d.find("git") != std::string::npos) ||
                         (h.find("gitea") != std::string::npos) ||
                         (h.find("forgejo") != std::string::npos) ||
                         (h.find("codeberg") != std::string::npos);
            if (!isGit) {
                (void)m_store.deleteProfile(it->id);
                it = stored.erase(it);
                continue;
            }

            // Sanitize customApiUrl: strip SSH ports (like :2222) and ensure valid https://
            if (it->customApiUrl.find(":2222") != std::string::npos || it->customApiUrl.empty()) {
                it->customApiUrl = "https://" + it->host;
                (void)m_store.saveProfile(*it);
            }
        }
        ++it;
    }

    // 3. Merge: augment stored with detected SSH keys, or add newly detected
    for (const auto& det : detected) {
        auto it = std::find_if(stored.begin(), stored.end(), [&](const auto& s) {
            return s.id == det.id;
        });

        if (it != stored.end()) {
            if (det.hasSshKey && !it->hasSshKey) {
                it->hasSshKey = true;
                it->sshKeyPath = det.sshKeyPath;
            }
            if (it->username.empty() && !det.username.empty()) {
                it->username = det.username;
            }
            if (!det.customApiUrl.empty() && (it->customApiUrl.empty() || it->customApiUrl.find(":2222") != std::string::npos)) {
                it->customApiUrl = det.customApiUrl;
            }
            (void)m_store.saveProfile(*it);
        } else {
            stored.push_back(det);
            (void)m_store.saveProfile(det);
        }
    }

    m_profiles = std::move(stored);
    emit profilesChanged();

    // Set initial active profile and fetch
    selectProfile(QStringLiteral("github"));
}

const core::git::bo::GitProviderProfile* GitProfilesViewModel::findProfile(const QString& id) const {
    const auto stdId = id.toStdString();
    for (const auto& p : m_profiles) {
        if (p.id == stdId) return &p;
    }
    return nullptr;
}

core::git::bo::GitProviderProfile* GitProfilesViewModel::findProfileMut(const QString& id) {
    const auto stdId = id.toStdString();
    for (auto& p : m_profiles) {
        if (p.id == stdId) return &p;
    }
    return nullptr;
}

QVariantList GitProfilesViewModel::profiles() const {
    QVariantList list;

    // First tab: Direct URL
    {
        QVariantMap urlMap;
        urlMap[QStringLiteral("id")] = QStringLiteral("url");
        urlMap[QStringLiteral("displayName")] = QStringLiteral("URL Diretto");
        urlMap[QStringLiteral("host")] = QStringLiteral("");
        urlMap[QStringLiteral("icon")] = QStringLiteral("🔗");
        urlMap[QStringLiteral("hasSsh")] = false;
        urlMap[QStringLiteral("hasToken")] = false;
        list.push_back(urlMap);
    }

    // Provider profiles
    for (const auto& p : m_profiles) {
        QVariantMap map;
        map[QStringLiteral("id")] = QString::fromStdString(p.id);
        map[QStringLiteral("displayName")] = QString::fromStdString(p.displayName);
        map[QStringLiteral("host")] = QString::fromStdString(p.host);
        map[QStringLiteral("username")] = QString::fromStdString(p.username);
        map[QStringLiteral("hasSsh")] = p.hasSshKey;
        map[QStringLiteral("hasToken")] = p.hasToken;

        QString icon = QStringLiteral("🖥️");
        switch (p.providerType) {
            case core::git::bo::GitProviderType::GitHub:    icon = QStringLiteral("🐙"); break;
            case core::git::bo::GitProviderType::Bitbucket: icon = QStringLiteral("🪣"); break;
            case core::git::bo::GitProviderType::GitLab:    icon = QStringLiteral("🦊"); break;
            case core::git::bo::GitProviderType::Custom:    icon = QStringLiteral("🖥️"); break;
        }
        map[QStringLiteral("icon")] = icon;
        list.push_back(map);
    }
    return list;
}

QString GitProfilesViewModel::activeProfileDisplayName() const {
    if (m_activeProfileId == QStringLiteral("url")) return QStringLiteral("URL Diretto");
    const auto* p = findProfile(m_activeProfileId);
    return p ? QString::fromStdString(p->displayName) : QStringLiteral("");
}

QString GitProfilesViewModel::activeProfileHost() const {
    const auto* p = findProfile(m_activeProfileId);
    return p ? QString::fromStdString(p->host) : QStringLiteral("");
}

QString GitProfilesViewModel::activeProfileType() const {
    const auto* p = findProfile(m_activeProfileId);
    return p ? QString::fromStdString(core::git::bo::gitProviderTypeToString(p->providerType)) : QStringLiteral("");
}

QString GitProfilesViewModel::activeProfileUsername() const {
    const auto* p = findProfile(m_activeProfileId);
    if (!p) return QStringLiteral("");
    if (p->username == "git") return QStringLiteral("");
    return QString::fromStdString(p->username);
}

bool GitProfilesViewModel::activeProfileHasSsh() const {
    const auto* p = findProfile(m_activeProfileId);
    return p ? p->hasSshKey : false;
}

QString GitProfilesViewModel::activeProfileSshKeyPath() const {
    const auto* p = findProfile(m_activeProfileId);
    return p ? QString::fromStdString(p->sshKeyPath) : QStringLiteral("");
}

bool GitProfilesViewModel::activeProfileHasToken() const {
    const auto* p = findProfile(m_activeProfileId);
    return p ? p->hasToken : false;
}

void GitProfilesViewModel::setSelectedProtocol(const QString& proto) {
    if (m_selectedProtocol == proto) return;
    m_selectedProtocol = proto;
    emit selectedProtocolChanged();
    m_repoModel->setSelectedProtocol(proto);
}

void GitProfilesViewModel::selectProfile(const QString& profileId) {
    m_activeProfileId = profileId;
    m_fetchError.clear();
    emit activeProfileChanged();
    emit fetchErrorChanged();

    const auto* p = findProfile(profileId);
    if (p) {
        if (p->hasSshKey) {
            setSelectedProtocol(QStringLiteral("ssh"));
        } else {
            setSelectedProtocol(QStringLiteral("https"));
        }
        fetchActiveProfileRepos();
    } else {
        m_repoModel->clear();
    }
}

void GitProfilesViewModel::fetchActiveProfileRepos() {
    if (m_activeProfileId == QStringLiteral("url")) {
        m_repoModel->clear();
        m_isFetching = false;
        emit isFetchingChanged();
        return;
    }

    const auto* p = findProfile(m_activeProfileId);
    if (!p || !m_apiClient) return;

    m_isFetching = true;
    m_fetchError.clear();
    emit isFetchingChanged();
    emit fetchErrorChanged();

    QString decryptedToken;
    if (p->hasToken && !p->tokenSecret.empty()) {
        auto decRes = m_vault.decrypt(p->tokenSecret);
        if (decRes) {
            decryptedToken = QString::fromStdString(*decRes);
        } else {
            m_fetchError = QStringLiteral("Errore decrittazione token Argon2id: %1").arg(QString::fromStdString(decRes.error()));
            emit fetchErrorChanged();
        }
    }

    if (decryptedToken.isEmpty()) {
        std::optional<std::string> envTok;
        if (p->providerType == core::git::bo::GitProviderType::GitHub) {
            envTok = gf::core::getEnvVar("GITHUB_TOKEN");
            if (!envTok) envTok = gf::core::getEnvVar("GH_TOKEN");
        } else if (p->providerType == core::git::bo::GitProviderType::GitLab) {
            envTok = gf::core::getEnvVar("GITLAB_TOKEN");
        } else if (p->providerType == core::git::bo::GitProviderType::Bitbucket) {
            envTok = gf::core::getEnvVar("BITBUCKET_TOKEN");
            if (!envTok) envTok = gf::core::getEnvVar("BITBUCKET_APP_PASSWORD");
        }
        if (envTok && !envTok->empty()) {
            decryptedToken = QString::fromUtf8(envTok->c_str());
            auto* pMut = findProfileMut(m_activeProfileId);
            if (pMut && !pMut->hasToken) {
                auto encRes = m_vault.encrypt(*envTok);
                if (encRes) {
                    pMut->tokenSecret = *encRes;
                    pMut->hasToken = true;
                    (void)m_store.saveProfile(*pMut);
                    emit profilesChanged();
                    emit activeProfileChanged();
                }
            }
        }
    }

    m_apiClient->fetchRepositories(m_activeProfileId, *p, decryptedToken);
}

void GitProfilesViewModel::refreshRepositories() {
    fetchActiveProfileRepos();
}

void GitProfilesViewModel::clearCache() {
    if (m_repoModel) {
        m_repoModel->clear();
    }
    m_fetchError.clear();
    emit fetchErrorChanged();
}

void GitProfilesViewModel::saveProfileToken(const QString& profileId, const QString& token, const QString& username) {
    auto* p = findProfileMut(profileId);
    if (!p) return;

    if (!username.trimmed().isEmpty()) {
        p->username = username.trimmed().toStdString();
    }

    const auto trimmedToken = token.trimmed();
    if (!trimmedToken.isEmpty()) {
        auto encRes = m_vault.encrypt(trimmedToken.toStdString());
        if (encRes) {
            p->tokenSecret = *encRes;
            p->hasToken = true;
        } else {
            m_fetchError = QStringLiteral("Errore durante la crittografia con Argon2id: %1").arg(QString::fromStdString(encRes.error()));
            emit fetchErrorChanged();
            return;
        }
    }

    auto saveRes = m_store.saveProfile(*p);
    if (!saveRes) {
        m_fetchError = QStringLiteral("Errore durante il salvataggio su SQLite: %1").arg(QString::fromStdString(saveRes.error().message));
        emit fetchErrorChanged();
        return;
    }

    emit profilesChanged();
    emit activeProfileChanged();

    fetchActiveProfileRepos();
}

void GitProfilesViewModel::clearProfileToken(const QString& profileId) {
    auto* p = findProfileMut(profileId);
    if (!p) return;

    p->tokenSecret = core::git::bo::EncryptedSecret{};
    p->hasToken = false;
    (void)m_store.saveProfile(*p);

    emit profilesChanged();
    emit activeProfileChanged();

    fetchActiveProfileRepos();
}

void GitProfilesViewModel::addCustomProfile(
    const QString& host,
    const QString& displayName,
    const QString& token,
    const QString& apiUrl
) {
    if (host.trimmed().isEmpty()) return;

    core::git::bo::GitProviderProfile p;
    p.id = "custom_" + host.trimmed().toStdString();
    p.providerType = core::git::bo::GitProviderType::Custom;
    p.displayName = displayName.trimmed().isEmpty() ? host.trimmed().toStdString() : displayName.trimmed().toStdString();
    p.host = host.trimmed().toStdString();
    p.customApiUrl = apiUrl.trimmed().toStdString();
    p.preferredProtocol = "ssh";

    if (!token.trimmed().isEmpty()) {
        auto encRes = m_vault.encrypt(token.trimmed().toStdString());
        if (encRes) {
            p.tokenSecret = *encRes;
            p.hasToken = true;
        }
    }

    (void)m_store.saveProfile(p);
    m_profiles.push_back(p);
    emit profilesChanged();

    selectProfile(QString::fromStdString(p.id));
}

} // namespace gf::desktop::viewmodels
