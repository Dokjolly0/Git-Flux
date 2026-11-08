#include <catch2/catch_test_macros.hpp>
#include "infrastructure/security/crypto_vault.hpp"
#include "infrastructure/persistence/profiles/sqlite_profile_store.hpp"
#include "infrastructure/persistence/sqlite/connection/sqlite_connection.hpp"
#include "infrastructure/persistence/sqlite/migrations/schema_migrator.hpp"
#include "infrastructure/git/credentials/system_credential_detector.hpp"

#include <filesystem>

TEST_CASE("CryptoVault: Argon2id KDF + AES-256-GCM encryption and decryption", "[crypto][security]") {
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

TEST_CASE("CryptoVault: Empty string handling and Hex conversion", "[crypto][security]") {
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

TEST_CASE("SqliteProfileStore: Schema Migration V2 and Profile CRUD", "[persistence][profiles]") {
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

TEST_CASE("SystemCredentialDetector: Default Profiles and SSH config", "[git][credentials]") {
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
