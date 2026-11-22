#include "desktop/viewmodels/git_profiles_viewmodel.hpp"
#include "core/shared/env_var.hpp"
#include <QCoreApplication>
#include <QEventLoop>
#include <QThread>
#include <QElapsedTimer>

#include <QUrl>

#include <algorithm>
#include <cctype>
#include <optional>

namespace gf::desktop::viewmodels
{
    namespace
    {
        // Ids of the custom servers the user added by hand (as opposed to "custom_" ids, which
        // are detected from ~/.ssh/config and filtered by the git-host heuristic).
        constexpr std::string_view kManualIdPrefix = "manual_";

        bool isManualProfileId(const std::string& id)
        {
            return id.starts_with(kManualIdPrefix);
        }

        bool equalsIgnoreCase(const std::string& a, const std::string& b)
        {
            return std::ranges::equal(a, b, [](unsigned char x, unsigned char y)
            {
                return std::tolower(x) == std::tolower(y);
            });
        }

        // Accepts "gitea.example.com", "https://gitea.example.com/", "git@host:2222"...
        // and returns the bare host name ("gitea.example.com"), lower-cased.
        QString normalizeHost(const QString& input)
        {
            QString h = input.trimmed();
            if (h.contains(QStringLiteral("://")))
            {
                h = QUrl(h).host();
            }
            else
            {
                if (const auto at = h.indexOf(QLatin1Char('@')); at >= 0) h = h.mid(at + 1);
                if (const auto slash = h.indexOf(QLatin1Char('/')); slash >= 0) h = h.left(slash);
                if (const auto colon = h.indexOf(QLatin1Char(':')); colon >= 0) h = h.left(colon);
            }
            return h.trimmed().toLower();
        }
    } // namespace

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
                            // Adapt SSH clone URLs to ~/.ssh/config (Host alias, Port)
                            auto adapted = repos;
                            if (const auto* p = findProfile(profileId))
                            {
                                for (auto& r : adapted)
                                {
                                    r.sshUrl = core::git::bo::applySshConfigToCloneUrl(r.sshUrl, *p);
                                }
                            }
                            m_repoModel->setRepositories(adapted);
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

        // Purge any stored *detected* custom profiles that are NOT git hosts (cleans up any old
        // non-git VPS servers picked up from ~/.ssh/config). Profiles the user added by hand
        // ("manual_" ids) are never touched: their host can be anything (IP, code.example.com...).
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

        // 3. Merge: augment stored with detected SSH keys, or add newly detected.
        //    A detected custom host that the user already added by hand augments that profile
        //    instead of showing up twice.
        for (const auto& det : detected)
        {
            auto it = std::find_if(stored.begin(), stored.end(), [&](const auto& s)
            {
                return s.id == det.id ||
                    (det.providerType == core::git::bo::GitProviderType::Custom && isManualProfileId(s.id) &&
                        equalsIgnoreCase(s.host, det.host));
            });

            if (it != stored.end())
            {
                if (det.hasSshKey && !it->hasSshKey)
                {
                    it->hasSshKey = true;
                    it->sshKeyPath = det.sshKeyPath;
                }
                if (it->sshHostAlias.empty() && !det.sshHostAlias.empty())
                {
                    it->sshHostAlias = det.sshHostAlias;
                    it->sshPort = det.sshPort;
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
            map[QStringLiteral("removable")] = isManualProfileId(p.id);

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
        const QString& apiUrl,
        const QString& username
    )
    {
        const QString cleanHost = normalizeHost(host);
        if (cleanHost.isEmpty()) return;

        core::git::bo::GitProviderProfile p;
        p.id = std::string(kManualIdPrefix) + cleanHost.toStdString();
        p.providerType = core::git::bo::GitProviderType::Custom;
        p.displayName = displayName.trimmed().isEmpty()
                            ? cleanHost.toStdString()
                            : displayName.trimmed().toStdString();
        p.host = cleanHost.toStdString();
        p.username = username.trimmed().toStdString();

        // Default API endpoint: https://<host>  (fetchCustom appends /api/v1 for Gitea/Forgejo)
        QString api = apiUrl.trimmed();
        if (api.isEmpty())
        {
            api = QStringLiteral("https://") + cleanHost;
        }
        else if (!api.contains(QStringLiteral("://")))
        {
            api.prepend(QStringLiteral("https://"));
        }
        p.customApiUrl = api.toStdString();
        p.preferredProtocol = "https";

        // Re-adding the same server keeps the SSH key detected for it (if any).
        if (const auto* existing = findProfile(QString::fromStdString(p.id)))
        {
            p.hasSshKey = existing->hasSshKey;
            p.sshKeyPath = existing->sshKeyPath;
            p.sshHostAlias = existing->sshHostAlias;
            p.sshPort = existing->sshPort;
            p.createdAtMs = existing->createdAtMs;
            if (p.username.empty()) p.username = existing->username;
        }
        if (!p.hasSshKey || p.sshHostAlias.empty())
        {
            // Pick up the ~/.ssh/config block of this host, if any (key, Host alias, Port).
            const auto det = m_detector.detectProfiles();
            const auto d = std::find_if(det.begin(), det.end(), [&](const auto& x)
            {
                return (x.hasSshKey || !x.sshHostAlias.empty()) && equalsIgnoreCase(x.host, p.host);
            });
            if (d != det.end())
            {
                if (d->hasSshKey && !p.hasSshKey)
                {
                    p.hasSshKey = true;
                    p.sshKeyPath = d->sshKeyPath;
                    p.preferredProtocol = "ssh";
                }
                if (p.sshHostAlias.empty())
                {
                    p.sshHostAlias = d->sshHostAlias;
                    p.sshPort = d->sshPort;
                }
                if (p.username.empty()) p.username = d->username;
            }
        }

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
        if (auto* existing = findProfileMut(id))
        {
            *existing = std::move(profile);
        }
        else
        {
            m_profiles.push_back(std::move(profile));
        }
        emit profilesChanged();

        selectProfile(id);
    }

    void GitProfilesViewModel::removeCustomProfile(const QString& profileId)
    {
        const auto id = profileId.toStdString();
        if (!isManualProfileId(id)) return; // built-in and detected profiles can't be removed

        const auto it = std::find_if(m_profiles.begin(), m_profiles.end(), [&](const auto& p)
        {
            return p.id == id;
        });
        if (it == m_profiles.end()) return;

        (void)m_store.deleteProfile(id);
        m_profiles.erase(it);
        emit profilesChanged();

        if (m_activeProfileId == profileId)
        {
            selectProfile(QStringLiteral("url"));
        }
    }
} // namespace gf::desktop::viewmodels
