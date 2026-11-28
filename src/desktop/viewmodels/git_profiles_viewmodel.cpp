#include "desktop/viewmodels/git_profiles_viewmodel.hpp"
#include "desktop/services/desktop_viewmodel_utils.hpp"
#include "core/shared/env_var.hpp"

#include <QUrl>
#include <QDir>
#include <QFile>
#include <QFileInfo>

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

        // True when `host` names the server of a detected ~/.ssh/config profile, either by its
        // HostName or by its Host alias ("Host git.example.dev" + "HostName 192.168.1.10").
        bool isSameServer(const std::string& host, const core::git::bo::GitProviderProfile& detected)
        {
            return equalsIgnoreCase(host, detected.host) ||
                (!detected.sshHostAlias.empty() && equalsIgnoreCase(host, detected.sshHostAlias));
        }

        // True when `profile` is a ~/.ssh/config detection ("custom_<alias>") of the same server
        // as a profile the user added by hand for `manualHost`.
        bool isDetectedDuplicateOf(const core::git::bo::GitProviderProfile& profile, const std::string& manualHost)
        {
            return profile.id.starts_with("custom_") &&
                (equalsIgnoreCase(profile.id, "custom_" + manualHost) || isSameServer(manualHost, profile));
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
        QObject* parent,
        services::SshConnectionProbe* sshProbe
    )
        : QObject(parent),
          m_store(store),
          m_vault(vault),
          m_detector(detector),
          m_apiClient(apiClient),
          m_repoModel(new RemoteRepoListModel(this)),
          m_cryptoExecutor(std::make_unique<SerialGitExecutor>())
    {
        m_sshProbe = sshProbe;
        if (m_sshProbe)
        {
            connect(m_sshProbe, &services::SshConnectionProbe::probeFinished, this,
                    [this](const QString& profileId, core::git::bl::RemoteAccessIssue issue, const QString& account,
                           const QString& output)
                    {
                        m_sshProbesInFlight.remove(profileId);
                        applySshCheckResult(profileId,
                                            QString::fromUtf8(core::git::bl::remoteAccessIssueToString(issue).data()),
                                            account, output);
                    });
        }

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
        return gf::desktop::waitUntilIdle(timeoutMs, [this]() { return m_pendingCrypto <= 0; });
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

        // A server added by hand replaces its ~/.ssh/config detection saved earlier.
        for (const auto& manual : std::vector(stored))
        {
            if (!isManualProfileId(manual.id)) continue;
            std::erase_if(stored, [&](const auto& s)
            {
                if (!isDetectedDuplicateOf(s, manual.host)) return false;
                (void)m_store.deleteProfile(s.id);
                return true;
            });
        }

        // 3. Merge: augment stored with detected SSH keys, or add newly detected.
        //    A detected custom host that the user already added by hand augments that profile
        //    instead of showing up twice.
        for (const auto& det : detected)
        {
            auto it = stored.end();
            if (det.providerType == core::git::bo::GitProviderType::Custom)
            {
                it = std::find_if(stored.begin(), stored.end(), [&](const auto& s)
                {
                    return isManualProfileId(s.id) && isSameServer(s.host, det);
                });
            }
            if (it == stored.end())
            {
                it = std::find_if(stored.begin(), stored.end(), [&](const auto& s) { return s.id == det.id; });
            }

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

    namespace
    {
        QString providerIconUrl(const QString& iconName)
        {
            return QStringLiteral("qrc:/qt/qml/GitFlux/Desktop/assets/icons/providers/") + iconName +
                QStringLiteral(".png");
        }

        QString resolveProviderIconSource(const core::git::bo::GitProviderProfile& p)
        {
            switch (p.providerType)
            {
            case core::git::bo::GitProviderType::GitHub:
                return providerIconUrl(QStringLiteral("github"));
            case core::git::bo::GitProviderType::Bitbucket:
                return providerIconUrl(QStringLiteral("bitbucket"));
            case core::git::bo::GitProviderType::GitLab:
                return providerIconUrl(QStringLiteral("gitlab"));
            case core::git::bo::GitProviderType::Custom:
                break;
            }

            const QString hint = (QString::fromStdString(p.id) + QLatin1Char(' ') +
                QString::fromStdString(p.host) + QLatin1Char(' ') +
                QString::fromStdString(p.displayName) + QLatin1Char(' ') +
                QString::fromStdString(p.customApiUrl)).toLower();

            if (hint.contains(QStringLiteral("gitea"))) return providerIconUrl(QStringLiteral("gitea"));
            if (hint.contains(QStringLiteral("forgejo"))) return providerIconUrl(QStringLiteral("forgejo"));
            if (hint.contains(QStringLiteral("codeberg"))) return providerIconUrl(QStringLiteral("codeberg"));
            if (hint.contains(QStringLiteral("github"))) return providerIconUrl(QStringLiteral("github"));
            if (hint.contains(QStringLiteral("gitlab"))) return providerIconUrl(QStringLiteral("gitlab"));
            if (hint.contains(QStringLiteral("bitbucket"))) return providerIconUrl(QStringLiteral("bitbucket"));
            return providerIconUrl(QStringLiteral("server"));
        }
    } // namespace

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
            urlMap[QStringLiteral("iconSource")] = providerIconUrl(QStringLiteral("url"));
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
            map[QStringLiteral("iconSource")] = resolveProviderIconSource(p);
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

    QString GitProfilesViewModel::activeProfileIconSource() const
    {
        if (m_activeProfileId == QStringLiteral("url")) return providerIconUrl(QStringLiteral("url"));
        const auto* p = findProfile(m_activeProfileId);
        return p ? resolveProviderIconSource(*p) : QStringLiteral("");
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
            checkActiveProfileSsh(false);
            fetchActiveProfileRepos();
        }
        else
        {
            applySshCheckResult(profileId, QString(), QString(), QString());
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
        recheckSshIfFailing();
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
                return (x.hasSshKey || !x.sshHostAlias.empty()) && isSameServer(p.host, x);
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
        if (isManualProfileId(profile.id))
        {
            std::erase_if(m_profiles, [&](const auto& p)
            {
                if (!isDetectedDuplicateOf(p, profile.host)) return false;
                (void)m_store.deleteProfile(p.id);
                return true;
            });
        }

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

    // ---------------------------------------------------------------------------------------------
    // SSH key check & troubleshooting guide
    // ---------------------------------------------------------------------------------------------

    services::SshConnectionProbe::Request GitProfilesViewModel::sshRequestFor(
        const core::git::bo::GitProviderProfile& profile)
    {
        services::SshConnectionProbe::Request req;
        if (!profile.sshHostAlias.empty())
        {
            // The ~/.ssh/config block supplies HostName, Port and IdentityFile.
            req.target = QString::fromStdString(profile.sshHostAlias);
        }
        else
        {
            req.target = QString::fromStdString(profile.host);
            req.port = profile.sshPort;
            req.identityFile = QString::fromStdString(profile.sshKeyPath);
        }
        return req;
    }

    void GitProfilesViewModel::checkActiveProfileSsh(bool force, bool quiet)
    {
        const auto* p = findProfile(m_activeProfileId);
        if (!p || !p->hasSshKey || !m_sshProbe)
        {
            applySshCheckResult(m_activeProfileId, QString(), QString(), QString());
            return;
        }
        // Only successes are cached: a failure is checked again every time, since the user may
        // have fixed it in the meantime (added the key on the website, edited ~/.ssh/config...).
        if (!force)
        {
            if (const auto it = m_sshChecks.constFind(m_activeProfileId); it != m_sshChecks.cend())
            {
                applySshCheckResult(m_activeProfileId, it->state, it->account, it->output);
                return;
            }
        }
        m_sshChecks.remove(m_activeProfileId);
        if (m_sshProbesInFlight.contains(m_activeProfileId)) return; // its result is on the way
        m_sshProbesInFlight.insert(m_activeProfileId);
        // A quiet (background) re-check keeps the current message instead of flashing "checking".
        if (!quiet || m_sshCheckState.isEmpty())
        {
            applySshCheckResult(m_activeProfileId, QStringLiteral("checking"), QString(), QString());
        }
        m_sshProbe->probe(m_activeProfileId, sshRequestFor(*p));
    }

    void GitProfilesViewModel::applySshCheckResult(const QString& profileId, const QString& state,
                                                   const QString& account, const QString& output)
    {
        const bool isFinal = !state.isEmpty() && state != QStringLiteral("checking");
        const bool isOk = state == QStringLiteral("ok");
        if (isOk)
        {
            m_sshChecks.insert(profileId, SshCheckResult{state, account, output});
        }
        if (profileId != m_activeProfileId) return; // the user moved on: keep the result cached

        const bool changed = m_sshCheckState != state || m_sshCheckAccount != account || m_sshCheckOutput != output;
        m_sshCheckState = state;
        m_sshCheckAccount = account;
        m_sshCheckOutput = output;

        bool switched = m_sshAutoSwitched;
        if (isFinal && !isOk && m_selectedProtocol == QStringLiteral("ssh"))
        {
            // Cloning over SSH would fail: fall back to HTTPS, the guide explains how to fix SSH.
            setSelectedProtocol(QStringLiteral("https"));
            switched = true;
        }
        else if (isOk)
        {
            // Fixed: go back to SSH if it was us who moved away from it.
            if (m_sshAutoSwitched && m_selectedProtocol == QStringLiteral("https"))
            {
                setSelectedProtocol(QStringLiteral("ssh"));
            }
            switched = false;
        }
        else if (state.isEmpty())
        {
            switched = false;
        }
        const bool switchChanged = m_sshAutoSwitched != switched;
        m_sshAutoSwitched = switched;
        if (changed || switchChanged) emit sshCheckChanged();
    }

    void GitProfilesViewModel::recheckSshIfFailing()
    {
        const auto& st = m_sshCheckState;
        if (st.isEmpty() || st == QStringLiteral("ok") || st == QStringLiteral("checking")) return;
        refreshSshDetection();
        checkActiveProfileSsh(true, true);
    }

    void GitProfilesViewModel::recheckSsh()
    {
        refreshSshDetection();
        checkActiveProfileSsh(true);
    }

    void GitProfilesViewModel::refreshSshDetection()
    {
        // Pick up what the user may have just fixed: a new key, a ~/.ssh/config block...
        if (auto* p = findProfileMut(m_activeProfileId))
        {
            const auto detected = m_detector.detectProfiles();
            const auto d = std::find_if(detected.begin(), detected.end(), [&](const auto& x)
            {
                return x.id == p->id || isSameServer(p->host, x);
            });
            if (d != detected.end())
            {
                const bool gainedKey = d->hasSshKey && !p->hasSshKey;
                if (d->hasSshKey)
                {
                    p->hasSshKey = true;
                    p->sshKeyPath = d->sshKeyPath;
                }
                p->sshHostAlias = d->sshHostAlias;
                p->sshPort = d->sshPort;
                if (gainedKey)
                {
                    emit profilesChanged();
                    emit activeProfileChanged();
                }
            }
        }
    }

    const core::git::bo::GitProviderProfile* GitProfilesViewModel::findProfileByHost(const QString& host) const
    {
        if (host.isEmpty()) return findProfile(m_activeProfileId);
        const auto h = host.toLower().toStdString();
        for (const auto& p : m_profiles)
        {
            if (equalsIgnoreCase(p.host, h) || (!p.sshHostAlias.empty() && equalsIgnoreCase(p.sshHostAlias, h)))
            {
                return &p;
            }
        }
        return nullptr;
    }

    QString GitProfilesViewModel::publicKeyPathFor(const core::git::bo::GitProviderProfile* profile)
    {
        if (profile && !profile->sshKeyPath.empty())
        {
            const auto pub = QString::fromStdString(profile->sshKeyPath) + QStringLiteral(".pub");
            if (QFileInfo::exists(pub)) return pub;
        }
        const QDir ssh(QDir::homePath() + QStringLiteral("/.ssh"));
        for (const auto* name : {"id_ed25519.pub", "id_ecdsa.pub", "id_rsa.pub"})
        {
            const auto path = ssh.filePath(QString::fromLatin1(name));
            if (QFileInfo::exists(path)) return path;
        }
        return {};
    }

    bool GitProfilesViewModel::copyPublicKey(const QString& host)
    {
        const auto path = publicKeyPathFor(findProfileByHost(host));
        if (path.isEmpty()) return false;
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) return false;
        const auto key = QString::fromUtf8(file.readAll()).trimmed();
        // Never hand out a private key by mistake.
        if (key.isEmpty() || key.contains(QStringLiteral("PRIVATE KEY"))) return false;
        copyToClipboard(key);
        return true;
    }

    void GitProfilesViewModel::copyToClipboard(const QString& text)
    {
        gf::desktop::copyTextToClipboard(text);
    }

    namespace
    {
        QVariantMap makeStep(const QString& title, const QString& detail,
                             const QString& command = {}, const QString& url = {}, const QString& urlLabel = {},
                             const QString& action = {}, const QString& actionLabel = {})
        {
            QVariantMap step;
            step[QStringLiteral("title")] = title;
            step[QStringLiteral("detail")] = detail;
            step[QStringLiteral("command")] = command;
            step[QStringLiteral("url")] = url;
            step[QStringLiteral("urlLabel")] = urlLabel;
            step[QStringLiteral("action")] = action;
            step[QStringLiteral("actionLabel")] = actionLabel;
            return step;
        }

        // Web base of a self-hosted server: its API URL without "/api/v1", else https://<host>.
        QString webBaseUrl(const core::git::bo::GitProviderProfile* p, const QString& host)
        {
            if (p && !p->customApiUrl.empty())
            {
                QString base = QString::fromStdString(p->customApiUrl);
                if (const auto api = base.indexOf(QStringLiteral("/api/v")); api >= 0) base.truncate(api);
                while (base.endsWith(QLatin1Char('/'))) base.chop(1);
                return base;
            }
            return QStringLiteral("https://") + host;
        }

        QString sshKeysUrl(const core::git::bo::GitProviderProfile* p, const QString& host)
        {
            using T = core::git::bo::GitProviderType;
            if (p && p->providerType == T::GitHub) return QStringLiteral("https://github.com/settings/keys");
            if (p && p->providerType == T::GitLab) return QStringLiteral("https://gitlab.com/-/user_settings/ssh_keys");
            if (p && p->providerType == T::Bitbucket)
                return QStringLiteral("https://bitbucket.org/account/settings/ssh-keys/");
            return webBaseUrl(p, host) + QStringLiteral("/user/settings/keys"); // Gitea / Forgejo
        }

        QString tokenUrl(const core::git::bo::GitProviderProfile* p, const QString& host)
        {
            using T = core::git::bo::GitProviderType;
            if (p && p->providerType == T::GitHub) return QStringLiteral("https://github.com/settings/tokens");
            if (p && p->providerType == T::GitLab)
                return QStringLiteral("https://gitlab.com/-/user_settings/personal_access_tokens");
            if (p && p->providerType == T::Bitbucket)
                return QStringLiteral("https://id.atlassian.com/manage-profile/security/api-tokens");
            return webBaseUrl(p, host) + QStringLiteral("/user/settings/applications");
        }

        QString hostKeyFingerprintsUrl(const core::git::bo::GitProviderProfile* p)
        {
            using T = core::git::bo::GitProviderType;
            if (p && p->providerType == T::GitHub)
                return QStringLiteral(
                    "https://docs.github.com/en/authentication/keeping-your-account-and-data-secure/githubs-ssh-key-fingerprints");
            if (p && p->providerType == T::GitLab)
                return QStringLiteral("https://docs.gitlab.com/user/gitlab_com/#ssh-host-keys-fingerprints");
            return {};
        }
    } // namespace

    QVariantMap GitProfilesViewModel::troubleshootingGuide(const QString& issue, const QString& host) const
    {
        const auto* p = findProfileByHost(host);
        const QString serverHost = p ? QString::fromStdString(p->host) : host;
        const QString providerName = p ? QString::fromStdString(p->displayName) : serverHost;
        services::SshConnectionProbe::Request req;
        if (p)
        {
            req = sshRequestFor(*p);
        }
        else
        {
            req.target = serverHost;
        }
        const QString testCommand = services::SshConnectionProbe::commandLine(req);
        const QString pubKey = publicKeyPathFor(p);
        const QString knownHostsEntry = req.port > 0 && req.port != 22
                                            ? QStringLiteral("\"[%1]:%2\"").arg(req.target).arg(req.port)
                                            : req.target;

        QVariantList steps;
        QString title;
        QString summary;

        if (issue == QStringLiteral("sshKeyRejected"))
        {
            title = QStringLiteral("La chiave SSH non è accettata da %1").arg(providerName);
            summary = QStringLiteral(
                "Il server ha rifiutato le chiavi SSH di questo computer: probabilmente la chiave non è ancora "
                "registrata sul tuo account (ad esempio è registrata solo quella di un altro PC).");
            if (pubKey.isEmpty())
            {
                steps << makeStep(QStringLiteral("Crea una chiave SSH"),
                                  QStringLiteral("Su questo computer non c'è una chiave pubblica. Creane una nuova "
                                      "(premi Invio per accettare il percorso proposto)."),
                                  QStringLiteral("ssh-keygen -t ed25519 -C \"$(whoami)@$(hostname)\""));
                steps << makeStep(QStringLiteral("Copia la chiave pubblica"),
                                  QStringLiteral("Poi copia la chiave pubblica appena creata."),
                                  QStringLiteral("cat ~/.ssh/id_ed25519.pub"), {}, {},
                                  QStringLiteral("copyPublicKey"), QStringLiteral("Copia chiave pubblica"));
            }
            else
            {
                steps << makeStep(QStringLiteral("Copia la chiave pubblica"),
                                  QStringLiteral("Chiave trovata: %1").arg(pubKey),
                                  QStringLiteral("cat %1").arg(pubKey), {}, {},
                                  QStringLiteral("copyPublicKey"), QStringLiteral("Copia chiave pubblica"));
            }
            steps << makeStep(QStringLiteral("Aggiungila al tuo account %1").arg(providerName),
                              QStringLiteral("Apri le impostazioni delle chiavi SSH, scegli \"Aggiungi chiave\" e "
                                  "incolla la chiave pubblica."),
                              {}, sshKeysUrl(p, serverHost), QStringLiteral("Apri impostazioni chiavi SSH"));
            steps << makeStep(QStringLiteral("Verifica la connessione"),
                              QStringLiteral("Dovresti ricevere un messaggio di benvenuto con il tuo nome utente."),
                              testCommand, {}, {}, QStringLiteral("recheckSsh"), QStringLiteral("Verifica di nuovo"));
            steps << makeStep(QStringLiteral("In alternativa usa HTTPS"),
                              QStringLiteral("Funziona subito per i repository pubblici; per quelli privati serve "
                                  "un token di accesso."),
                              {}, {}, {}, QStringLiteral("useHttps"), QStringLiteral("Usa HTTPS"));
        }
        else if (issue == QStringLiteral("sshHostKeyUnknown"))
        {
            title = QStringLiteral("%1 non è ancora tra gli host SSH conosciuti").arg(serverHost);
            summary = QStringLiteral(
                "È la prima connessione SSH a questo server da questo computer: ssh deve memorizzarne "
                "l'impronta (fingerprint). Git Flux non la accetta al posto tuo, per sicurezza.");
            const auto fpUrl = hostKeyFingerprintsUrl(p);
            steps << makeStep(QStringLiteral("Connettiti una volta dal terminale"),
                              QStringLiteral("Controlla che l'impronta mostrata corrisponda a quella ufficiale del "
                                  "server, poi rispondi \"yes\"."),
                              testCommand, fpUrl, fpUrl.isEmpty() ? QString() : QStringLiteral("Impronte ufficiali"));
            steps << makeStep(QStringLiteral("Verifica di nuovo"), QString(), {}, {}, {},
                              QStringLiteral("recheckSsh"), QStringLiteral("Verifica di nuovo"));
        }
        else if (issue == QStringLiteral("sshHostKeyChanged"))
        {
            title = QStringLiteral("L'impronta SSH di %1 è cambiata").arg(serverHost);
            summary = QStringLiteral(
                "La chiave del server non corrisponde a quella memorizzata. Può succedere dopo una reinstallazione "
                "del server, ma può anche indicare un attacco: procedi solo se sai perché è cambiata.");
            const auto fpUrl = hostKeyFingerprintsUrl(p);
            steps << makeStep(QStringLiteral("Verifica la nuova impronta"),
                              QStringLiteral("Confrontala con quella ufficiale o chiedi all'amministratore del server."),
                              {}, fpUrl, fpUrl.isEmpty() ? QString() : QStringLiteral("Impronte ufficiali"));
            steps << makeStep(QStringLiteral("Rimuovi la vecchia impronta"), QString(),
                              QStringLiteral("ssh-keygen -R %1").arg(knownHostsEntry));
            steps << makeStep(QStringLiteral("Connettiti di nuovo e accetta la nuova impronta"), QString(),
                              testCommand, {}, {}, QStringLiteral("recheckSsh"), QStringLiteral("Verifica di nuovo"));
        }
        else if (issue == QStringLiteral("hostNotFound"))
        {
            title = QStringLiteral("Impossibile trovare il server %1").arg(serverHost);
            summary = QStringLiteral("Il nome del server non viene risolto: controlla la connessione a internet e "
                "che il nome sia scritto correttamente.");
            steps << makeStep(QStringLiteral("Controlla il nome del server"), QString(),
                              QStringLiteral("getent hosts %1").arg(serverHost));
            steps << makeStep(QStringLiteral("Verifica di nuovo"), QString(), {}, {}, {},
                              QStringLiteral("recheckSsh"), QStringLiteral("Verifica di nuovo"));
        }
        else if (issue == QStringLiteral("connectionFailed"))
        {
            title = QStringLiteral("Connessione SSH a %1 non riuscita").arg(serverHost);
            summary = QStringLiteral("Il server non risponde sulla porta SSH. Sui server Gitea/Forgejo in Docker "
                "SSH di solito è su una porta diversa (ad esempio 2222).");
            steps << makeStep(QStringLiteral("Diagnostica dettagliata"),
                              QStringLiteral("Mostra dove si ferma la connessione."),
                              QStringLiteral("ssh -vT %1").arg(testCommand.mid(QStringLiteral("ssh -T ").size())));
            steps << makeStep(QStringLiteral("Se SSH usa un'altra porta, aggiungila a ~/.ssh/config"),
                              QStringLiteral("Git Flux legge il blocco al prossimo avvio e lo usa per i clone."),
                              QStringLiteral("Host %1\n    HostName %1\n    Port 2222\n    User git").arg(serverHost));
            steps << makeStep(QStringLiteral("Verifica di nuovo"), QString(), {}, {}, {},
                              QStringLiteral("recheckSsh"), QStringLiteral("Verifica di nuovo"));
            steps << makeStep(QStringLiteral("In alternativa usa HTTPS"), QString(), {}, {}, {},
                              QStringLiteral("useHttps"), QStringLiteral("Usa HTTPS"));
        }
        else if (issue == QStringLiteral("httpsAuthFailed"))
        {
            title = QStringLiteral("Accesso HTTPS a %1 negato").arg(providerName);
            summary = QStringLiteral("Il repository richiede l'autenticazione: crea un token di accesso personale "
                "e inseriscilo in Git Flux.");
            steps << makeStep(QStringLiteral("Crea un token di accesso"),
                              QStringLiteral("Concedi almeno il permesso di lettura dei repository."),
                              {}, tokenUrl(p, serverHost), QStringLiteral("Apri pagina token"));
            steps << makeStep(QStringLiteral("Inseriscilo in Git Flux"), QString(), {}, {}, {},
                              QStringLiteral("openTokenConfig"), QStringLiteral("Inserisci token"));
        }
        else if (issue == QStringLiteral("repositoryNotFound"))
        {
            title = QStringLiteral("Repository non trovato");
            summary = QStringLiteral("Il server è raggiungibile ma il repository non esiste, oppure è privato e "
                "il tuo account non ha accesso.");
            steps << makeStep(QStringLiteral("Controlla l'URL del repository"), QString());
            steps << makeStep(QStringLiteral("Se è privato, accedi con un token"), QString(), {}, {}, {},
                              QStringLiteral("openTokenConfig"), QStringLiteral("Inserisci token"));
        }
        else
        {
            title = QStringLiteral("Errore di accesso al server");
            summary = QStringLiteral("Copia il log qui sotto per i dettagli. Puoi provare la connessione dal terminale:");
            steps << makeStep(QStringLiteral("Prova la connessione SSH"), QString(), testCommand, {}, {},
                              QStringLiteral("recheckSsh"), QStringLiteral("Verifica di nuovo"));
        }

        QVariantMap guide;
        guide[QStringLiteral("issue")] = issue;
        guide[QStringLiteral("title")] = title;
        guide[QStringLiteral("summary")] = summary;
        guide[QStringLiteral("steps")] = steps;
        guide[QStringLiteral("hasPublicKey")] = !pubKey.isEmpty();
        return guide;
    }

    // ---------------------------------------------------------------------------------------------
    // HTTPS credentials for git (clone...)
    // ---------------------------------------------------------------------------------------------

    GitProfilesViewModel::HttpsCredentialLoader GitProfilesViewModel::httpsCredentialLoader(
        const QString& remoteUrl) const
    {
        const auto url = remoteUrl.trimmed().toStdString();
        if (!gf::git::bl::isHttpRemoteUrl(url)) return {};
        const auto host = QString::fromStdString(core::git::bl::extractRemoteHost(url));
        const auto* p = findProfileByHost(host);
        if (!p) return {};
        const auto hostName = host.toStdString(); // the URL's host: what git sends to the helper

        // The account name git sends with the token. Providers check the token itself; these
        // are the names each one documents for token authentication.
        using T = core::git::bo::GitProviderType;
        std::string username;
        switch (p->providerType)
        {
        case T::GitHub:
            username = (p->username.empty() || p->username == "git") ? "x-access-token" : p->username;
            break;
        case T::GitLab:
            username = "oauth2";
            break;
        case T::Bitbucket:
            username = "x-bitbucket-api-token-auth"; // Atlassian API tokens (app passwords are retired)
            break;
        case T::Custom:
            username = (p->username.empty() || p->username == "git") ? "oauth2" : p->username;
            break;
        }

        if (!p->tokenSecret.empty())
        {
            auto& vault = m_vault;
            return [&vault, secret = p->tokenSecret, username, hostName]() -> std::optional<gf::git::bl::HttpsCredential>
            {
                auto plain = vault.decrypt(secret);
                if (!plain || plain->empty()) return std::nullopt;
                return gf::git::bl::HttpsCredential{.username = username, .password = std::move(*plain),
                                                    .host = hostName};
            };
        }
        if (auto envToken = environmentToken(p->providerType))
        {
            return [token = std::move(*envToken), username, hostName]() -> std::optional<gf::git::bl::HttpsCredential>
            {
                return gf::git::bl::HttpsCredential{.username = username, .password = token, .host = hostName};
            };
        }
        return {};
    }
} // namespace gf::desktop::viewmodels
