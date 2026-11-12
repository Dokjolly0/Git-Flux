#include "desktop/viewmodels/git_profiles_viewmodel.hpp"
#include "core/shared/env_var.hpp"
#include <QCoreApplication>
#include <QEventLoop>
#include <QThread>
#include <QElapsedTimer>

#include <algorithm>
#include <optional>

namespace gf::desktop::viewmodels
{
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
          m_repoModel(new RemoteRepoListModel(this)),
          m_cryptoExecutor(std::make_unique<SerialGitExecutor>())
    {
        if (m_apiClient)
        {
            connect(m_apiClient, &services::GitProviderApiClient::repositoriesFetched,
                    this, [this](const QString& profileId,
                                 const std::vector<core::git::bo::RemoteRepositoryItem>& repos)
                    {
                        if (profileId == m_activeProfileId)
                        {
                            m_isFetching = false;
                            m_fetchError.clear();
                            m_repoModel->setRepositories(repos);
                            emit isFetchingChanged();
                            emit fetchErrorChanged();
                        }
                    });

            connect(m_apiClient, &services::GitProviderApiClient::fetchFailed,
                    this, [this](const QString& profileId, const QString& errorMessage)
                    {
                        if (profileId == m_activeProfileId)
                        {
                            m_isFetching = false;
                            m_fetchError = errorMessage;
                            m_repoModel->clear();
                            emit isFetchingChanged();
                            emit fetchErrorChanged();
                        }
                    });
        }
    }

    // The executor (declared last) is destroyed first and waits for the running derivation.
    GitProfilesViewModel::~GitProfilesViewModel() = default;

    template <typename Work, typename Done>
    void GitProfilesViewModel::runCrypto(Work work, Done done)
    {
        ++m_pendingCrypto;
        m_cryptoExecutor->run(this, std::move(work), [this, done = std::move(done)](auto result) mutable
        {
            --m_pendingCrypto;
            done(std::move(result));
        });
    }

    bool GitProfilesViewModel::waitForIdle(int timeoutMs)
    {
        QElapsedTimer timer;
        timer.start();
        while (m_pendingCrypto > 0)
        {
            if (timer.elapsed() > timeoutMs)
            {
                return false;
            }
            QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
            QThread::msleep(1);
        }
        return true;
    }

    void GitProfilesViewModel::initialize()
    {
        // 1. Detect system credentials and SSH hosts
        auto detected = m_detector.detectProfiles();

        // 2. Load stored profiles from SQLite
        auto storedRes = m_store.getAllProfiles();
        std::vector<core::git::bo::GitProviderProfile> stored;
        if (storedRes)
        {
            stored = std::move(*storedRes);
        }

        // Purge any stored custom profiles that are NOT git hosts (cleans up any old non-git VPS servers)
        for (auto it = stored.begin(); it != stored.end();)
        {
            if (it->id.starts_with("custom_"))
            {
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
                if (!isGit)
                {
                    (void)m_store.deleteProfile(it->id);
                    it = stored.erase(it);
                    continue;
                }

                // Sanitize customApiUrl: strip SSH ports (like :2222) and ensure valid https://
                if (it->customApiUrl.find(":2222") != std::string::npos || it->customApiUrl.empty())
                {
                    it->customApiUrl = "https://" + it->host;
                    (void)m_store.saveProfile(*it);
                }
            }
            ++it;
        }

        // 3. Merge: augment stored with detected SSH keys, or add newly detected
        for (const auto& det : detected)
        {
            auto it = std::find_if(stored.begin(), stored.end(), [&](const auto& s)
            {
                return s.id == det.id;
            });

            if (it != stored.end())
            {
                if (det.hasSshKey && !it->hasSshKey)
                {
                    it->hasSshKey = true;
                    it->sshKeyPath = det.sshKeyPath;
                }
                if (it->username.empty() && !det.username.empty())
                {
                    it->username = det.username;
                }
                if (!det.customApiUrl.empty() && (it->customApiUrl.empty() || it->customApiUrl.find(":2222") !=
                    std::string::npos))
                {
                    it->customApiUrl = det.customApiUrl;
                }
                (void)m_store.saveProfile(*it);
            }
            else
            {
                stored.push_back(det);
                (void)m_store.saveProfile(det);
            }
        }

        m_profiles = std::move(stored);
        emit profilesChanged();

        // Set initial active profile and fetch
        selectProfile(QStringLiteral("github"));
    }

    const core::git::bo::GitProviderProfile* GitProfilesViewModel::findProfile(const QString& id) const
    {
        const auto stdId = id.toStdString();
        for (const auto& p : m_profiles)
        {
            if (p.id == stdId) return &p;
        }
        return nullptr;
    }

    core::git::bo::GitProviderProfile* GitProfilesViewModel::findProfileMut(const QString& id)
    {
        const auto stdId = id.toStdString();
        for (auto& p : m_profiles)
        {
            if (p.id == stdId) return &p;
        }
        return nullptr;
    }

    QVariantList GitProfilesViewModel::profiles() const
    {
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
        for (const auto& p : m_profiles)
        {
            QVariantMap map;
            map[QStringLiteral("id")] = QString::fromStdString(p.id);
            map[QStringLiteral("displayName")] = QString::fromStdString(p.displayName);
            map[QStringLiteral("host")] = QString::fromStdString(p.host);
            map[QStringLiteral("username")] = QString::fromStdString(p.username);
            map[QStringLiteral("hasSsh")] = p.hasSshKey;
            map[QStringLiteral("hasToken")] = p.hasToken;

            QString icon = QStringLiteral("🖥️");
            switch (p.providerType)
            {
            case core::git::bo::GitProviderType::GitHub: icon = QStringLiteral("🐙");
                break;
            case core::git::bo::GitProviderType::Bitbucket: icon = QStringLiteral("🪣");
                break;
            case core::git::bo::GitProviderType::GitLab: icon = QStringLiteral("🦊");
                break;
            case core::git::bo::GitProviderType::Custom: icon = QStringLiteral("🖥️");
                break;
            }
            map[QStringLiteral("icon")] = icon;
            list.push_back(map);
        }
        return list;
    }

    QString GitProfilesViewModel::activeProfileDisplayName() const
    {
        if (m_activeProfileId == QStringLiteral("url")) return QStringLiteral("URL Diretto");
        const auto* p = findProfile(m_activeProfileId);
        return p ? QString::fromStdString(p->displayName) : QStringLiteral("");
    }

    QString GitProfilesViewModel::activeProfileHost() const
    {
        const auto* p = findProfile(m_activeProfileId);
        return p ? QString::fromStdString(p->host) : QStringLiteral("");
    }

    QString GitProfilesViewModel::activeProfileType() const
    {
        const auto* p = findProfile(m_activeProfileId);
        return p ? QString::fromStdString(core::git::bo::gitProviderTypeToString(p->providerType)) : QStringLiteral("");
    }

    QString GitProfilesViewModel::activeProfileUsername() const
    {
        const auto* p = findProfile(m_activeProfileId);
        if (!p) return QStringLiteral("");
        if (p->username == "git") return QStringLiteral("");
        return QString::fromStdString(p->username);
    }

    bool GitProfilesViewModel::activeProfileHasSsh() const
    {
        const auto* p = findProfile(m_activeProfileId);
        return p ? p->hasSshKey : false;
    }

    QString GitProfilesViewModel::activeProfileSshKeyPath() const
    {
        const auto* p = findProfile(m_activeProfileId);
        return p ? QString::fromStdString(p->sshKeyPath) : QStringLiteral("");
    }

    bool GitProfilesViewModel::activeProfileHasToken() const
    {
        const auto* p = findProfile(m_activeProfileId);
        return p ? p->hasToken : false;
    }

    void GitProfilesViewModel::setSelectedProtocol(const QString& proto)
    {
        if (m_selectedProtocol == proto) return;
        m_selectedProtocol = proto;
        emit selectedProtocolChanged();
        m_repoModel->setSelectedProtocol(proto);
    }

    void GitProfilesViewModel::selectProfile(const QString& profileId)
    {
        m_activeProfileId = profileId;
        m_fetchError.clear();
        emit activeProfileChanged();
        emit fetchErrorChanged();

        const auto* p = findProfile(profileId);
        if (p)
        {
            if (p->hasSshKey)
            {
                setSelectedProtocol(QStringLiteral("ssh"));
            }
            else
            {
                setSelectedProtocol(QStringLiteral("https"));
            }
            fetchActiveProfileRepos();
        }
        else
        {
            // No provider ("url" tab...): drop a token still being decrypted for the previous one.
            m_tokenRequest.invalidate();
            m_repoModel->clear();
            if (m_isFetching)
            {
                m_isFetching = false;
                emit isFetchingChanged();
            }
        }
    }

    namespace
    {
        // Provider token from the environment (GITHUB_TOKEN...), used when none is stored.
        std::optional<std::string> environmentToken(core::git::bo::GitProviderType type)
        {
            std::optional<std::string> token;
            switch (type)
            {
            case core::git::bo::GitProviderType::GitHub:
                token = gf::core::getEnvVar("GITHUB_TOKEN");
                if (!token) token = gf::core::getEnvVar("GH_TOKEN");
                break;
            case core::git::bo::GitProviderType::GitLab:
                token = gf::core::getEnvVar("GITLAB_TOKEN");
                break;
            case core::git::bo::GitProviderType::Bitbucket:
                token = gf::core::getEnvVar("BITBUCKET_TOKEN");
                if (!token) token = gf::core::getEnvVar("BITBUCKET_APP_PASSWORD");
                break;
            case core::git::bo::GitProviderType::Custom:
                break;
            }
            if (token && token->empty()) token.reset();
            return token;
        }

        // Token to use for the API request, resolved on the crypto thread.
        struct TokenResolution
        {
            QString token;
            QString decryptError;                               // the stored token could not be decrypted
            std::optional<core::git::bo::EncryptedSecret> envSecret; // env token encrypted, to store
        };
    } // namespace

    void GitProfilesViewModel::fetchActiveProfileRepos()
    {
        if (m_activeProfileId == QStringLiteral("url"))
        {
            m_tokenRequest.invalidate();
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

        const auto requestId = m_tokenRequest.next();
        const auto isCurrent = m_tokenRequest.guard(requestId);
        const auto profileId = m_activeProfileId;
        const auto secret = p->hasToken ? p->tokenSecret : core::git::bo::EncryptedSecret{};
        const bool hasStoredToken = p->hasToken;
        const auto providerType = p->providerType;
        auto& vault = m_vault;

        runCrypto(
            [&vault, isCurrent, secret, hasStoredToken, providerType]()
            {
                TokenResolution resolution;
                if (!isCurrent())
                {
                    return resolution; // another profile was selected meanwhile: skip the derivation
                }
                if (!secret.empty())
                {
                    if (auto decRes = vault.decrypt(secret))
                    {
                        resolution.token = QString::fromStdString(*decRes);
                    }
                    else
                    {
                        resolution.decryptError = QString::fromStdString(decRes.error());
                    }
                }
                if (resolution.token.isEmpty())
                {
                    if (const auto envTok = environmentToken(providerType))
                    {
                        resolution.token = QString::fromStdString(*envTok);
                        if (!hasStoredToken)
                        {
                            if (auto encRes = vault.encrypt(*envTok))
                            {
                                resolution.envSecret = std::move(*encRes);
                            }
                        }
                    }
                }
                return resolution;
            },
            [this, requestId, profileId](TokenResolution resolution)
            {
                if (!m_tokenRequest.isCurrent(requestId))
                {
                    return;
                }
                if (!resolution.decryptError.isEmpty())
                {
                    m_fetchError = QStringLiteral("Errore decrittazione token Argon2id: %1").arg(resolution.decryptError);
                    emit fetchErrorChanged();
                }
                auto* profile = findProfileMut(profileId);
                if (!profile)
                {
                    return;
                }
                if (resolution.envSecret && !profile->hasToken)
                {
                    profile->tokenSecret = std::move(*resolution.envSecret);
                    profile->hasToken = true;
                    (void)m_store.saveProfile(*profile);
                    emit profilesChanged();
                    emit activeProfileChanged();
                }
                m_apiClient->fetchRepositories(profileId, *profile, resolution.token);
            });
    }

    void GitProfilesViewModel::refreshRepositories()
    {
        fetchActiveProfileRepos();
    }

    void GitProfilesViewModel::clearCache()
    {
        if (m_repoModel)
        {
            m_repoModel->clear();
        }
        m_fetchError.clear();
        emit fetchErrorChanged();
    }

    void GitProfilesViewModel::saveProfileToken(const QString& profileId, const QString& token, const QString& username)
    {
        auto* p = findProfileMut(profileId);
        if (!p) return;

        if (!username.trimmed().isEmpty())
        {
            p->username = username.trimmed().toStdString();
        }

        // Stores the profile (with the new secret, if any) and reloads its repositories.
        auto store = [this, profileId](std::optional<core::git::bo::EncryptedSecret> secret)
        {
            auto* profile = findProfileMut(profileId);
            if (!profile) return;
            if (secret)
            {
                profile->tokenSecret = std::move(*secret);
                profile->hasToken = true;
            }

            auto saveRes = m_store.saveProfile(*profile);
            if (!saveRes)
            {
                m_fetchError = QStringLiteral("Errore durante il salvataggio su SQLite: %1").arg(
                    QString::fromStdString(saveRes.error().message));
                emit fetchErrorChanged();
                return;
            }

            emit profilesChanged();
            emit activeProfileChanged();

            fetchActiveProfileRepos();
        };

        const auto trimmedToken = token.trimmed().toStdString();
        if (trimmedToken.empty())
        {
            store(std::nullopt);
            return;
        }

        auto& vault = m_vault;
        runCrypto([&vault, trimmedToken]() { return vault.encrypt(trimmedToken); },
                  [this, store](auto encRes) mutable
                  {
                      if (!encRes)
                      {
                          m_fetchError = QStringLiteral("Errore durante la crittografia con Argon2id: %1").arg(
                              QString::fromStdString(encRes.error()));
                          emit fetchErrorChanged();
                          return;
                      }
                      store(std::move(*encRes));
                  });
    }

    void GitProfilesViewModel::clearProfileToken(const QString& profileId)
    {
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
    )
    {
        if (host.trimmed().isEmpty()) return;

        core::git::bo::GitProviderProfile p;
        p.id = "custom_" + host.trimmed().toStdString();
        p.providerType = core::git::bo::GitProviderType::Custom;
        p.displayName = displayName.trimmed().isEmpty()
                            ? host.trimmed().toStdString()
                            : displayName.trimmed().toStdString();
        p.host = host.trimmed().toStdString();
        p.customApiUrl = apiUrl.trimmed().toStdString();
        p.preferredProtocol = "ssh";

        const auto trimmedToken = token.trimmed().toStdString();
        if (trimmedToken.empty())
        {
            addProfile(std::move(p));
            return;
        }

        auto& vault = m_vault;
        runCrypto([&vault, trimmedToken]() { return vault.encrypt(trimmedToken); },
                  [this, p = std::move(p)](auto encRes) mutable
                  {
                      if (encRes)
                      {
                          p.tokenSecret = std::move(*encRes);
                          p.hasToken = true;
                      }
                      addProfile(std::move(p));
                  });
    }

    void GitProfilesViewModel::addProfile(core::git::bo::GitProviderProfile profile)
    {
        (void)m_store.saveProfile(profile);
        const auto id = QString::fromStdString(profile.id);
        m_profiles.push_back(std::move(profile));
        emit profilesChanged();

        selectProfile(id);
    }
} // namespace gf::desktop::viewmodels
