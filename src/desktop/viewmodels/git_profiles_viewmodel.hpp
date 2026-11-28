#pragma once

#include "core/modules/git/bo/git_provider_profile.hpp"
#include "infrastructure/persistence/profiles/sqlite_profile_store.hpp"
#include "infrastructure/security/crypto_vault.hpp"
#include "infrastructure/git/credentials/system_credential_detector.hpp"
#include "desktop/services/git_provider_api_client.hpp"
#include "desktop/services/ssh_connection_probe.hpp"
#include "core/modules/git/bl/https_credential.hpp"
#include "desktop/services/serial_git_executor.hpp"
#include "desktop/viewmodels/remote_repo_list_model.hpp"

#include <QObject>
#include <QString>
#include <QVariantList>
#include <QVariantMap>
#include <QHash>
#include <QSet>
#include <vector>
#include <memory>
#include <functional>
#include <optional>

namespace gf::desktop::viewmodels
{
    class GitProfilesViewModel : public QObject
    {
        Q_OBJECT

        Q_PROPERTY(QVariantList profiles READ profiles NOTIFY profilesChanged)
        Q_PROPERTY(QString activeProfileId READ activeProfileId WRITE selectProfile NOTIFY activeProfileChanged)
        Q_PROPERTY(QString activeProfileDisplayName READ activeProfileDisplayName NOTIFY activeProfileChanged)
        Q_PROPERTY(QString activeProfileHost READ activeProfileHost NOTIFY activeProfileChanged)
        Q_PROPERTY(QString activeProfileIconSource READ activeProfileIconSource NOTIFY activeProfileChanged)
        Q_PROPERTY(QString activeProfileUsername READ activeProfileUsername NOTIFY activeProfileChanged)
        Q_PROPERTY(bool activeProfileHasSsh READ activeProfileHasSsh NOTIFY activeProfileChanged)
        Q_PROPERTY(bool activeProfileHasToken READ activeProfileHasToken NOTIFY activeProfileChanged)
        Q_PROPERTY(
            QString selectedProtocol READ selectedProtocol WRITE setSelectedProtocol NOTIFY selectedProtocolChanged)
        Q_PROPERTY(bool isFetching READ isFetching NOTIFY isFetchingChanged)
        Q_PROPERTY(QString fetchError READ fetchError NOTIFY fetchErrorChanged)
        Q_PROPERTY(RemoteRepoListModel* remoteReposModel READ remoteReposModel CONSTANT)
        // SSH key check of the active profile ("ssh -T"): "" (not applicable), "checking", "ok",
        // or an issue code (see remoteAccessIssueToString: "sshKeyRejected", "sshHostKeyUnknown"...)
        Q_PROPERTY(QString sshCheckState READ sshCheckState NOTIFY sshCheckChanged)
        Q_PROPERTY(QString sshCheckAccount READ sshCheckAccount NOTIFY sshCheckChanged)
        Q_PROPERTY(QString sshCheckOutput READ sshCheckOutput NOTIFY sshCheckChanged)
        // True when the failed check switched the protocol from SSH to HTTPS on its own.
        Q_PROPERTY(bool sshAutoSwitchedToHttps READ sshAutoSwitchedToHttps NOTIFY sshCheckChanged)

    public:
        explicit GitProfilesViewModel(
            infra::persistence::profiles::SqliteProfileStore& store,
            infra::security::CryptoVault& vault,
            infra::git::credentials::SystemCredentialDetector& detector,
            services::GitProviderApiClient* apiClient,
            QObject* parent = nullptr,
            services::SshConnectionProbe* sshProbe = nullptr // nullptr: no automatic SSH key check
        );
        ~GitProfilesViewModel() override;

        void initialize();

        [[nodiscard]] QVariantList profiles() const;
        [[nodiscard]] QString activeProfileId() const noexcept { return m_activeProfileId; }
        [[nodiscard]] QString activeProfileDisplayName() const;
        [[nodiscard]] QString activeProfileHost() const;
        [[nodiscard]] QString activeProfileIconSource() const;
        [[nodiscard]] QString activeProfileUsername() const;
        [[nodiscard]] bool activeProfileHasSsh() const;
        [[nodiscard]] bool activeProfileHasToken() const;

        [[nodiscard]] QString selectedProtocol() const noexcept { return m_selectedProtocol; }
    Q_INVOKABLE void setSelectedProtocol(const QString& proto);

        [[nodiscard]] bool isFetching() const noexcept { return m_isFetching; }
        [[nodiscard]] QString fetchError() const noexcept { return m_fetchError; }

        [[nodiscard]] RemoteRepoListModel* remoteReposModel() const noexcept { return m_repoModel; }

        [[nodiscard]] QString sshCheckState() const noexcept { return m_sshCheckState; }
        [[nodiscard]] QString sshCheckAccount() const noexcept { return m_sshCheckAccount; }
        [[nodiscard]] QString sshCheckOutput() const noexcept { return m_sshCheckOutput; }
        [[nodiscard]] bool sshAutoSwitchedToHttps() const noexcept { return m_sshAutoSwitched; }

        /// For an HTTP(S) remote URL, returns a loader of the saved provider token for that host
        /// (or the provider's environment token, e.g. GITHUB_TOKEN), to be called off the GUI
        /// thread since decrypting runs the key derivation. Empty when no token is available.
        using HttpsCredentialLoader = std::function<std::optional<gf::git::bl::HttpsCredential>()>;
        [[nodiscard]] HttpsCredentialLoader httpsCredentialLoader(const QString& remoteUrl) const;

        /// Test helper: processes events until the pending encryptions/decryptions are applied.
        bool waitForIdle(int timeoutMs = 10000);

    Q_INVOKABLE void selectProfile(const QString& profileId);
    // Encrypting or decrypting a token runs the key derivation (PBKDF2 / Argon2id, ~100 ms by
    // design) on a background thread: these calls return at once, the outcome is applied later.
    Q_INVOKABLE void saveProfileToken(const QString& profileId, const QString& token, const QString& username);
    Q_INVOKABLE void clearProfileToken(const QString& profileId);
    Q_INVOKABLE void refreshRepositories();
    Q_INVOKABLE void clearCache();
    // Adds (or updates) a self-hosted Gitea/Forgejo server entered by the user. The host may be
    // given as a bare name or as a URL; an empty apiUrl defaults to https://<host>.
    Q_INVOKABLE void addCustomProfile(const QString& host, const QString& displayName, const QString& token,
                                      const QString& apiUrl, const QString& username = QString());
    // Removes a server added with addCustomProfile (built-in/detected profiles are ignored).
    Q_INVOKABLE void removeCustomProfile(const QString& profileId);

    // Runs the SSH key check of the active profile again (after the user fixed something).
    Q_INVOKABLE void recheckSsh();
    // Background re-check, only while the last check failed (no "checking" flash). Called when
    // the user comes back to the app and periodically while the warning is shown.
    Q_INVOKABLE void recheckSshIfFailing();
    // Step-by-step fix for a remote access problem, for the profile serving `host` (the active
    // profile when empty). Returns { issue, title, summary, steps: [ { title, detail, command,
    // url, urlLabel, action, actionLabel } ] }; actions: "copyPublicKey", "recheckSsh",
    // "useHttps", "openTokenConfig".
    Q_INVOKABLE QVariantMap troubleshootingGuide(const QString& issue, const QString& host = QString()) const;
    // Copies the public key matching the profile serving `host` (active profile when empty).
    Q_INVOKABLE bool copyPublicKey(const QString& host = QString());
    Q_INVOKABLE void copyToClipboard(const QString& text);

        signals :

        void profilesChanged();
        void activeProfileChanged();
        void selectedProtocolChanged();
        void isFetchingChanged();
        void fetchErrorChanged();
        void sshCheckChanged();

    private:
        infra::persistence::profiles::SqliteProfileStore& m_store;
        infra::security::CryptoVault& m_vault;
        infra::git::credentials::SystemCredentialDetector& m_detector;
        services::GitProviderApiClient* m_apiClient;
        RemoteRepoListModel* m_repoModel;

        std::vector<core::git::bo::GitProviderProfile> m_profiles;
        QString m_activeProfileId{QStringLiteral("github")};
        QString m_selectedProtocol{QStringLiteral("ssh")};
        bool m_isFetching{false};
        QString m_fetchError;

        services::SshConnectionProbe* m_sshProbe{nullptr};
        struct SshCheckResult
        {
            QString state;
            QString account;
            QString output;
        };
        QHash<QString, SshCheckResult> m_sshChecks; // successful checks per profile id
        QSet<QString> m_sshProbesInFlight;
        QString m_sshCheckState;
        QString m_sshCheckAccount;
        QString m_sshCheckOutput;
        bool m_sshAutoSwitched{false};

        [[nodiscard]] const core::git::bo::GitProviderProfile* findProfile(const QString& id) const;
        [[nodiscard]] core::git::bo::GitProviderProfile* findProfileMut(const QString& id);
        void fetchActiveProfileRepos();
        // Starts (or reuses the cached result of) the SSH key check of the active profile.
        void checkActiveProfileSsh(bool force, bool quiet = false);
        // Re-reads ~/.ssh for the active profile (new key, new config block...).
        void refreshSshDetection();
        void applySshCheckResult(const QString& profileId, const QString& state, const QString& account,
                                 const QString& output);
        [[nodiscard]] const core::git::bo::GitProviderProfile* findProfileByHost(const QString& host) const;
        [[nodiscard]] static services::SshConnectionProbe::Request sshRequestFor(
            const core::git::bo::GitProviderProfile& profile);
        [[nodiscard]] static QString publicKeyPathFor(const core::git::bo::GitProviderProfile* profile);
        // Stores and selects a new custom profile (its token, if any, already encrypted).
        void addProfile(core::git::bo::GitProviderProfile profile);
        // Runs `work` on the crypto thread and `done(result)` on the GUI thread.
        template <typename Work, typename Done>
        void runCrypto(Work work, Done done);

        int m_pendingCrypto{0};
        // Latest wins: switching profile while a token is being decrypted drops the old token.
        LatestRequest m_tokenRequest;
        // Declared last: destroyed first, waiting for its tasks while this object is alive.
        std::unique_ptr<SerialGitExecutor> m_cryptoExecutor;
    };
} // namespace gf::desktop::viewmodels
