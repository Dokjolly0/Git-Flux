#pragma once

#include "core/modules/git/bo/git_provider_profile.hpp"
#include <string>
#include <vector>
#include <expected>
#include <filesystem>

namespace gf::infra::security {

class CryptoVault {
public:
    explicit CryptoVault(std::filesystem::path keyStorageDir = "");
    ~CryptoVault() = default;

    /// Encrypts a plaintext secret (such as a Personal Access Token) using Argon2id (KDF) + AES-256-GCM.
    [[nodiscard]] std::expected<core::git::bo::EncryptedSecret, std::string>
    encrypt(const std::string& plaintext) const;

    /// Decrypts an encrypted secret back into plaintext using Argon2id (KDF) + AES-256-GCM.
    [[nodiscard]] std::expected<std::string, std::string>
    decrypt(const core::git::bo::EncryptedSecret& secret) const;

    /// Encrypts with a specific master secret and salt (useful for deterministic testing).
    [[nodiscard]] static std::expected<core::git::bo::EncryptedSecret, std::string>
    encryptWithMasterSecret(const std::string& plaintext, const std::string& masterSecret);

    /// Decrypts with a specific master secret (useful for deterministic testing).
    [[nodiscard]] static std::expected<std::string, std::string>
    decryptWithMasterSecret(const core::git::bo::EncryptedSecret& secret, const std::string& masterSecret);

    /// Helper to convert binary bytes to hex string.
    [[nodiscard]] static std::string toHex(const unsigned char* data, size_t length);
    [[nodiscard]] static std::string toHex(const std::vector<unsigned char>& data);

    /// Helper to convert hex string to binary bytes.
    [[nodiscard]] static std::vector<unsigned char> fromHex(std::string_view hex);

private:
    std::filesystem::path m_keyStorageDir;
    mutable std::string m_cachedMasterSecret;

    [[nodiscard]] std::string getOrCreateMasterSecret() const;
};

} // namespace gf::infra::security
