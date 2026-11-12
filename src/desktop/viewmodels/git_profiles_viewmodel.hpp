#pragma once

#include "core/modules/git/bo/git_provider_profile.hpp"
#include "infrastructure/persistence/profiles/sqlite_profile_store.hpp"
#include "infrastructure/security/crypto_vault.hpp"
#include "infrastructure/git/credentials/system_credential_detector.hpp"
#include "desktop/services/git_provider_api_client.hpp"
#include "desktop/services/serial_git_executor.hpp"
#include "desktop/viewmodels/remote_repo_list_model.hpp"

#include <QObject>
#include <QString>
#include <QVariantList>
#include <vector>
#include <memory>

namespace gf::desktop::viewmodels
{
    class GitProfilesViewModel : public QObject
    {
        Q_OBJECT

        Q_PROPERTY(QVariantList profiles READ profiles NOTIFY profilesChanged)
        Q_PROPERTY(QString activeProfileId READ activeProfileId WRITE selectProfile NOTIFY activeProfileChanged)
        Q_PROPERTY(QString activeProfileDisplayName READ activeProfileDisplayName NOTIFY activeProfileChanged)
        Q_PROPERTY(QString activeProfileHost READ activeProfileHost NOTIFY activeProfileChanged)
        Q_PROPERTY(QString activeProfileType READ activeProfileType NOTIFY activeProfileChanged)
        Q_PROPERTY(QString activeProfileUsername READ activeProfileUsername NOTIFY activeProfileChanged)
        Q_PROPERTY(bool activeProfileHasSsh READ activeProfileHasSsh NOTIFY activeProfileChanged)
        Q_PROPERTY(QString activeProfileSshKeyPath READ activeProfileSshKeyPath NOTIFY activeProfileChanged)
        Q_PROPERTY(bool activeProfileHasToken READ activeProfileHasToken NOTIFY activeProfileChanged)
        Q_PROPERTY(
            QString selectedProtocol READ selectedProtocol WRITE setSelectedProtocol NOTIFY selectedProtocolChanged)
        Q_PROPERTY(bool isFetching READ isFetching NOTIFY isFetchingChanged)
        Q_PROPERTY(QString fetchError READ fetchError NOTIFY fetchErrorChanged)
        Q_PROPERTY(RemoteRepoListModel* remoteReposModel READ remoteReposModel CONSTANT)

    public:
        explicit GitProfilesViewModel(
            infra::persistence::profiles::SqliteProfileStore& store,
            infra::security::CryptoVault& vault,
            infra::git::credentials::SystemCredentialDetector& detector,
            services::GitProviderApiClient* apiClient,
            QObject* parent = nullptr
        );
        ~GitProfilesViewModel() override;

        void initialize();

        [[nodiscard]] QVariantList profiles() const;
        [[nodiscard]] QString activeProfileId() const noexcept { return m_activeProfileId; }
        [[nodiscard]] QString activeProfileDisplayName() const;
        [[nodiscard]] QString activeProfileHost() const;
        [[nodiscard]] QString activeProfileType() const;
        [[nodiscard]] QString activeProfileUsername() const;
        [[nodiscard]] bool activeProfileHasSsh() const;
        [[nodiscard]] QString activeProfileSshKeyPath() const;
        [[nodiscard]] bool activeProfileHasToken() const;

        [[nodiscard]] QString selectedProtocol() const noexcept { return m_selectedProtocol; }
    Q_INVOKABLE void setSelectedProtocol(const QString& proto);

        [[nodiscard]] bool isFetching() const noexcept { return m_isFetching; }
        [[nodiscard]] QString fetchError() const noexcept { return m_fetchError; }

        [[nodiscard]] RemoteRepoListModel* remoteReposModel() const noexcept { return m_repoModel; }

        /// Test helper: processes events until the pending encryptions/decryptions are applied.
        bool waitForIdle(int timeoutMs = 10000);

    Q_INVOKABLE void selectProfile(const QString& profileId);
    // Encrypting or decrypting a token runs the key derivation (PBKDF2 / Argon2id, ~100 ms by
    // design) on a background thread: these calls return at once, the outcome is applied later.
    Q_INVOKABLE void saveProfileToken(const QString& profileId, const QString& token, const QString& username);
    Q_INVOKABLE void clearProfileToken(const QString& profileId);
    Q_INVOKABLE void refreshRepositories();
    Q_INVOKABLE void clearCache();
    Q_INVOKABLE void addCustomProfile(const QString& host, const QString& displayName, const QString& token,
                                      const QString& apiUrl);

        signals :

        void profilesChanged();
        void activeProfileChanged();
        void selectedProtocolChanged();
        void isFetchingChanged();
        void fetchErrorChanged();

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

        [[nodiscard]] const core::git::bo::GitProviderProfile* findProfile(const QString& id) const;
        [[nodiscard]] core::git::bo::GitProviderProfile* findProfileMut(const QString& id);
        void fetchActiveProfileRepos();
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
