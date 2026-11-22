#include <catch2/catch_test_macros.hpp>
#include "infrastructure/security/crypto_vault.hpp"
#include "infrastructure/persistence/profiles/sqlite_profile_store.hpp"
#include "infrastructure/persistence/sqlite/connection/sqlite_connection.hpp"
#include "infrastructure/persistence/sqlite/migrations/schema_migrator.hpp"
#include "infrastructure/git/credentials/system_credential_detector.hpp"

#include <filesystem>

TEST_CASE (
"CryptoVault: Argon2id KDF + AES-256-GCM encryption and decryption"
,
"[crypto][security]"
)
 {
    const std::string masterSecret = "super-secret-machine-key-123456789";
    const std::string plaintext = "ghp_PersonalAccessTokenWithHighEntropySecret999";

    auto encRes = gf::infra::security::CryptoVault::encryptWithMasterSecret(plaintext, masterSecret);
    REQUIRE(encRes.has_value());

    const auto& secret = *encRes;
    CHECK(!secret.ciphertextHex.empty());
    CHECK(!secret.saltHex.empty());
    CHECK(!secret.ivHex.empty());
    CHECK(!secret.tagHex.empty());

    // Valid decryption with correct master secret
    auto decRes = gf::infra::security::CryptoVault::decryptWithMasterSecret(secret, masterSecret);
    REQUIRE(decRes.has_value());
    CHECK(*decRes == plaintext);

    // Decryption with wrong master secret must fail authentication
    auto wrongKeyRes = gf::infra::security::CryptoVault::decryptWithMasterSecret(secret, "wrong-master-secret");
    CHECK(!wrongKeyRes.has_value());

    // Tampered ciphertext must fail authentication
    auto tamperedSecret = secret;
    tamperedSecret.ciphertextHex[0] = (tamperedSecret.ciphertextHex[0] == 'a') ? 'b' : 'a';
    auto tamperedRes = gf::infra::security::CryptoVault::decryptWithMasterSecret(tamperedSecret, masterSecret);
    CHECK(!tamperedRes.has_value());
}

TEST_CASE (
"CryptoVault: Empty string handling and Hex conversion"
,
"[crypto][security]"
)
 {
    const std::string masterSecret = "vault-key-abc";
    auto encRes = gf::infra::security::CryptoVault::encryptWithMasterSecret("", masterSecret);
    REQUIRE(encRes.has_value());
    CHECK(encRes->empty());

    auto decRes = gf::infra::security::CryptoVault::decryptWithMasterSecret(*encRes, masterSecret);
    REQUIRE(decRes.has_value());
    CHECK(decRes->empty());

    // Hex conversion
    std::vector<unsigned char> data = {0x00, 0x0F, 0x1A, 0xFF};
    auto hex = gf::infra::security::CryptoVault::toHex(data);
    CHECK(hex == "000f1aff");
    auto roundtrip = gf::infra::security::CryptoVault::fromHex(hex);
    CHECK(roundtrip == data);
}

TEST_CASE (
"SqliteProfileStore: Schema Migration V2 and Profile CRUD"
,
"[persistence][profiles]"
)
 {
    auto connRes = gf::infra::persistence::sqlite::SqliteConnection::openInMemory();
    REQUIRE(connRes.has_value());
    auto& conn = *connRes;

    auto migRes = gf::infra::persistence::sqlite::SchemaMigrator::migrateToLatest(conn);
    REQUIRE(migRes.has_value());

    auto verRes = gf::infra::persistence::sqlite::SchemaMigrator::getCurrentVersion(conn);
    REQUIRE(verRes.has_value());
    CHECK(*verRes >= 2);

    gf::infra::persistence::profiles::SqliteProfileStore store(conn);

    gf::core::git::bo::GitProviderProfile profile;
    profile.id = "github";
    profile.providerType = gf::core::git::bo::GitProviderType::GitHub;
    profile.displayName = "GitHub";
    profile.host = "github.com";
    profile.username = "Dokjolly0";
    profile.tokenSecret.ciphertextHex = "abcdef123456";
    profile.tokenSecret.saltHex = "112233445566";
    profile.tokenSecret.ivHex = "aabbccddeeff";
    profile.tokenSecret.tagHex = "001122334455";
    profile.preferredProtocol = "ssh";

    // 1. Save profile
    auto saveRes = store.saveProfile(profile);
    REQUIRE(saveRes.has_value());

    // 2. Retrieve profile
    auto getRes = store.getProfile("github");
    REQUIRE(getRes.has_value());
    REQUIRE(getRes->has_value());
    const auto& loaded = **getRes;
    CHECK(loaded.id == "github");
    CHECK(loaded.displayName == "GitHub");
    CHECK(loaded.host == "github.com");
    CHECK(loaded.username == "Dokjolly0");
    CHECK(loaded.tokenSecret.ciphertextHex == "abcdef123456");
    CHECK(loaded.hasToken == true);
    CHECK(loaded.preferredProtocol == "ssh");

    // 3. Update profile
    profile.preferredProtocol = "https";
    profile.username = "DokjollyUpdated";
    REQUIRE(store.saveProfile(profile).has_value());

    auto getUpdated = store.getProfile("github");
    REQUIRE(getUpdated.has_value());
    REQUIRE(getUpdated->has_value());
    CHECK((*getUpdated)->username == "DokjollyUpdated");
    CHECK((*getUpdated)->preferredProtocol == "https");

    // 4. Retrieve all profiles
    auto allRes = store.getAllProfiles();
    REQUIRE(allRes.has_value());
    CHECK(allRes->size() == 1);

    // 5. Delete profile
    auto delRes = store.deleteProfile("github");
    REQUIRE(delRes.has_value());
    CHECK(*delRes == true);

    auto getDeleted = store.getProfile("github");
    REQUIRE(getDeleted.has_value());
    CHECK(!getDeleted->has_value());
}

TEST_CASE (
"SystemCredentialDetector: Default Profiles and SSH config"
,
"[git][credentials]"
)
 {
    gf::infra::git::credentials::SystemCredentialDetector detector;
    auto profiles = detector.detectProfiles();

    REQUIRE(!profiles.empty());

    // GitHub, Bitbucket, and GitLab standard profiles should always be present
    auto hasGitHub = std::any_of(profiles.begin(), profiles.end(), [](const auto& p) {
        return p.id == "github" && p.host == "github.com";
    });
    auto hasBitbucket = std::any_of(profiles.begin(), profiles.end(), [](const auto& p) {
        return p.id == "bitbucket" && p.host == "bitbucket.org";
    });
    auto hasGitLab = std::any_of(profiles.begin(), profiles.end(), [](const auto& p) {
        return p.id == "gitlab" && p.host == "gitlab.com";
    });

    CHECK(hasGitHub);
    CHECK(hasBitbucket);
    CHECK(hasGitLab);

    // If git.alexviolatto.dev exists in SSH config, verify API URL is on https:// without SSH port 2222
    for (const auto& p : profiles) {
        if (p.host == "git.alexviolatto.dev") {
            CHECK(p.customApiUrl == "https://git.alexviolatto.dev");
            CHECK(p.customApiUrl.find(":2222") == std::string::npos);
        }
    }
}

#include <algorithm>
#include <fstream>

namespace
{
    const gf::core::git::bo::GitProviderProfile* findById(
        const std::vector<gf::core::git::bo::GitProviderProfile>& profiles, const std::string& id)
    {
        const auto it = std::find_if(profiles.begin(), profiles.end(), [&](const auto& p) { return p.id == id; });
        return it == profiles.end() ? nullptr : &*it;
    }
} // namespace

TEST_CASE (
"SystemCredentialDetector: ssh config Port, multiple Host aliases and patterns"
,
"[git][credentials]"
)
 {
    const auto home = std::filesystem::temp_directory_path() / "gf-ssh-config-parse";
    std::filesystem::remove_all(home);
    std::filesystem::create_directories(home / ".ssh");
    { std::ofstream(home / ".ssh" / "id_ed25519_gitea") << "dummy"; }
    {
        std::ofstream cfg(home / ".ssh" / "config");
        cfg << "Host *\n"
               "    ServerAliveInterval 60\n"
               "\n"
               "# Gitea in Docker: SSH on 2222, two aliases on one line\n"
               "Host mygitea git.example.dev\n"
               "    HostName git.example.dev\n"
               "    User git\n"
               "    Port 2222\n"
               "    Port 2200\n" // ssh keeps the first value
               "    IdentityFile ~/.ssh/id_ed25519_gitea\n"
               "\n"
               "# Only the second alias looks like a forge\n"
               "Host box forgejo-box\n"
               "    HostName 10.0.0.5\n"
               "    Port=3022\n"
               "\n"
               "Host gh-work\n"
               "    HostName github.com\n"
               "    IdentityFile ~/.ssh/id_ed25519_gitea\n"
               "\n"
               "Host raspberry\n"
               "    HostName 192.168.1.30\n"
               "    User pi\n";
    }

    gf::infra::git::credentials::SystemCredentialDetector detector(home);
    const auto profiles = detector.detectProfiles();

    SECTION("first alias names the profile, Port and key are read") {
        const auto* p = findById(profiles, "custom_mygitea");
        REQUIRE(p != nullptr);
        CHECK(p->host == "git.example.dev");
        CHECK(p->sshHostAlias == "mygitea");
        CHECK(p->sshPort == 2222);
        CHECK(p->hasSshKey);
        CHECK(p->customApiUrl == "https://git.example.dev"); // SSH port never leaks into the API URL
        CHECK(findById(profiles, "custom_git.example.dev") == nullptr); // one profile per block
    }

    SECTION("any alias of the block can mark it as a git host, 'Key=Value' works") {
        const auto* p = findById(profiles, "custom_box");
        REQUIRE(p != nullptr);
        CHECK(p->host == "10.0.0.5");
        CHECK(p->sshPort == 3022);
        CHECK_FALSE(p->hasSshKey);
    }

    SECTION("an alias for a built-in provider augments it") {
        const auto* gh = findById(profiles, "github");
        REQUIRE(gh != nullptr);
        CHECK(gh->sshHostAlias == "gh-work");
        CHECK(gh->hasSshKey);
        CHECK(findById(profiles, "custom_gh-work") == nullptr);
    }

    SECTION("patterns and non-git hosts are ignored") {
        CHECK(findById(profiles, "custom_*") == nullptr);
        CHECK(findById(profiles, "custom_raspberry") == nullptr);
    }

    std::error_code ec;
    std::filesystem::remove_all(home, ec);
}

TEST_CASE (
"applySshConfigToCloneUrl adapts clone URLs to the ssh config block"
,
"[git][credentials]"
)
 {
    using gf::core::git::bo::applySshConfigToCloneUrl;
    gf::core::git::bo::GitProviderProfile p;
    p.host = "git.example.dev";

    SECTION("no alias and no port: unchanged") {
        CHECK(applySshConfigToCloneUrl("git@git.example.dev:a/b.git", p) == "git@git.example.dev:a/b.git");
    }

    SECTION("port only: added when the server advertises none") {
        p.sshHostAlias = "git.example.dev"; // alias equal to the host isn't an alias
        p.sshPort = 2222;
        CHECK(applySshConfigToCloneUrl("git@git.example.dev:a/b.git", p) == "ssh://git@git.example.dev:2222/a/b.git");
        CHECK(applySshConfigToCloneUrl("ssh://git@git.example.dev/a/b.git", p) ==
            "ssh://git@git.example.dev:2222/a/b.git");
        CHECK(applySshConfigToCloneUrl("ssh://git@git.example.dev:2200/a/b.git", p) ==
            "ssh://git@git.example.dev:2200/a/b.git");
    }

    SECTION("alias: the block supplies HostName, Port and key") {
        p.sshHostAlias = "mygitea";
        p.sshPort = 2222;
        CHECK(applySshConfigToCloneUrl("git@git.example.dev:a/b.git", p) == "git@mygitea:a/b.git");
        CHECK(applySshConfigToCloneUrl("ssh://git@Git.Example.Dev:2222/a/b.git", p) == "git@mygitea:a/b.git");
    }

    SECTION("alias without Port keeps the server's port") {
        p.sshHostAlias = "mygitea";
        CHECK(applySshConfigToCloneUrl("ssh://git@git.example.dev:2222/a/b.git", p) == "ssh://git@mygitea:2222/a/b.git");
    }

    SECTION("other hosts and non-SSH URLs are untouched") {
        p.sshHostAlias = "mygitea";
        p.sshPort = 2222;
        CHECK(applySshConfigToCloneUrl("https://git.example.dev/a/b.git", p) == "https://git.example.dev/a/b.git");
        CHECK(applySshConfigToCloneUrl("git@other.host:a/b.git", p) == "git@other.host:a/b.git");
        CHECK(applySshConfigToCloneUrl("", p).empty());
    }
}

#include "core/modules/git/bl/remote_access_diagnostics.hpp"

TEST_CASE (
"remote access diagnostics classify ssh and git output"
,
"[git][credentials][diagnostics]"
)
 {
    using namespace gf::core::git::bl;

    SECTION("ssh -T greetings are successes with the account name") {
        const std::string github = "Hi Dokjolly0! You've successfully authenticated, but GitHub does not provide shell access.";
        CHECK(classifyRemoteAccessOutput(github) == RemoteAccessIssue::None);
        CHECK(extractSshGreetingAccount(github) == "Dokjolly0");

        const std::string forgejo = "Hi there, Dokjolly! You've successfully authenticated with the key named pc, "
                                    "but Forgejo does not provide shell access.";
        CHECK(classifyRemoteAccessOutput(forgejo) == RemoteAccessIssue::None);
        CHECK(extractSshGreetingAccount(forgejo) == "Dokjolly");

        const std::string gitlab = "Welcome to GitLab, @jane.doe!";
        CHECK(classifyRemoteAccessOutput(gitlab) == RemoteAccessIssue::None);
        CHECK(extractSshGreetingAccount(gitlab) == "jane.doe");
    }

    SECTION("ssh failures") {
        CHECK(classifyRemoteAccessOutput("git@github.com: Permission denied (publickey).\r\nfatal: Could not read from "
            "remote repository.") == RemoteAccessIssue::SshKeyRejected);
        CHECK(classifyRemoteAccessOutput("No ED25519 host key is known for github.com and you have requested strict "
            "checking.\nHost key verification failed.") == RemoteAccessIssue::SshHostKeyUnknown);
        CHECK(classifyRemoteAccessOutput("@@@ WARNING: REMOTE HOST IDENTIFICATION HAS CHANGED! @@@\nHost key "
            "verification failed.") == RemoteAccessIssue::SshHostKeyChanged);
        CHECK(classifyRemoteAccessOutput("ssh: Could not resolve hostname gitt.example.dev: Name or service not known")
            == RemoteAccessIssue::HostNotFound);
        CHECK(classifyRemoteAccessOutput("ssh: connect to host 192.168.1.122 port 22: Connection refused")
            == RemoteAccessIssue::ConnectionFailed);
    }

    SECTION("https and repository failures") {
        CHECK(classifyRemoteAccessOutput("fatal: could not read Username for 'https://github.com': terminal prompts "
            "disabled") == RemoteAccessIssue::HttpsAuthFailed);
        CHECK(classifyRemoteAccessOutput("remote: Repository not found.\nfatal: repository "
            "'https://github.com/a/b.git/' not found") == RemoteAccessIssue::RepositoryNotFound);
        CHECK(classifyRemoteAccessOutput("something else entirely") == RemoteAccessIssue::Unknown);
    }

    SECTION("remote URL helpers") {
        CHECK(extractRemoteHost("git@GitHub.com:Dokjolly0/Git-Flux.git") == "github.com");
        CHECK(extractRemoteHost("ssh://git@git.example.dev:2222/a/b.git") == "git.example.dev");
        CHECK(extractRemoteHost("https://user:tok@gitlab.com/a/b.git") == "gitlab.com");
        CHECK(extractRemoteHost("mygitea:a/b.git") == "mygitea");
        CHECK(extractRemoteHost("/home/me/repo").empty());
        CHECK(isSshRemoteUrl("git@github.com:a/b.git"));
        CHECK(isSshRemoteUrl("ssh://git@host/a.git"));
        CHECK_FALSE(isSshRemoteUrl("https://github.com/a/b.git"));
        CHECK_FALSE(isSshRemoteUrl("/home/me/repo"));
        CHECK(remoteAccessIssueToString(RemoteAccessIssue::SshKeyRejected) == "sshKeyRejected");
    }
}
