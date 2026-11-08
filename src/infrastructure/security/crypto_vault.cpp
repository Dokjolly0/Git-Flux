#include "infrastructure/security/crypto_vault.hpp"

#include <openssl/evp.h>
#include <openssl/kdf.h>
#include <openssl/core_names.h>
#include <openssl/params.h>
#include <openssl/rand.h>

#include <fstream>
#include <sstream>
#include <iomanip>
#include <array>
#include <cstdlib>

namespace gf::infra::security {

namespace {

constexpr size_t kSaltBytes = 16;
constexpr size_t kIvBytes = 12;
constexpr size_t kTagBytes = 16;
constexpr size_t kKeyBytes = 32; // 256 bits for AES-256

// Argon2id KDF parameters (balanced for desktop security and responsiveness: ~50ms execution)
constexpr uint32_t kArgon2Lanes = 1;
constexpr uint32_t kArgon2Threads = 1;
constexpr uint32_t kArgon2MemCost = 65536; // 64 MB
constexpr uint32_t kArgon2Iterations = 3;

std::expected<std::vector<unsigned char>, std::string>
deriveArgon2idKey(const std::string& masterSecret, const std::vector<unsigned char>& salt) {
    if (salt.size() < 8) {
        return std::unexpected("Salt too short for Argon2id");
    }

    EVP_KDF* kdf = EVP_KDF_fetch(nullptr, "ARGON2ID", nullptr);
    if (!kdf) {
        return std::unexpected("Failed to fetch OpenSSL ARGON2ID provider");
    }

    EVP_KDF_CTX* kctx = EVP_KDF_CTX_new(kdf);
    EVP_KDF_free(kdf);
    if (!kctx) {
        return std::unexpected("Failed to create OpenSSL KDF context");
    }

    uint32_t lanes = kArgon2Lanes;
    uint32_t threads = kArgon2Threads;
    uint32_t mem_cost = kArgon2MemCost;
    uint32_t iterations = kArgon2Iterations;

    std::array<OSSL_PARAM, 7> params{};
    params[0] = OSSL_PARAM_construct_octet_string(OSSL_KDF_PARAM_PASSWORD,
                                                  const_cast<char*>(masterSecret.data()), masterSecret.size());
    params[1] = OSSL_PARAM_construct_octet_string(OSSL_KDF_PARAM_SALT,
                                                  const_cast<unsigned char*>(salt.data()), salt.size());
    params[2] = OSSL_PARAM_construct_uint32(OSSL_KDF_PARAM_THREADS, &threads);
    params[3] = OSSL_PARAM_construct_uint32(OSSL_KDF_PARAM_ARGON2_LANES, &lanes);
    params[4] = OSSL_PARAM_construct_uint32(OSSL_KDF_PARAM_ARGON2_MEMCOST, &mem_cost);
    params[5] = OSSL_PARAM_construct_uint32(OSSL_KDF_PARAM_ITER, &iterations);
    params[6] = OSSL_PARAM_construct_end();

    std::vector<unsigned char> key(kKeyBytes);
    if (EVP_KDF_derive(kctx, key.data(), key.size(), params.data()) <= 0) {
        EVP_KDF_CTX_free(kctx);
        return std::unexpected("Argon2id key derivation execution failed");
    }

    EVP_KDF_CTX_free(kctx);
    return key;
}

std::filesystem::path getDefaultVaultDir() {
    const char* home = std::getenv("HOME");
    if (!home) {
        home = std::getenv("USERPROFILE");
    }
    if (home) {
        return std::filesystem::path(home) / ".local" / "share" / "GitFlux";
    }
    return std::filesystem::current_path() / ".gitflux_vault";
}

} // namespace

CryptoVault::CryptoVault(std::filesystem::path keyStorageDir)
    : m_keyStorageDir(keyStorageDir.empty() ? getDefaultVaultDir() : std::move(keyStorageDir)) {}

std::string CryptoVault::toHex(const unsigned char* data, size_t length) {
    std::ostringstream oss;
    for (size_t i = 0; i < length; ++i) {
        oss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(data[i]);
    }
    return oss.str();
}

std::string CryptoVault::toHex(const std::vector<unsigned char>& data) {
    return toHex(data.data(), data.size());
}

std::vector<unsigned char> CryptoVault::fromHex(std::string_view hex) {
    std::vector<unsigned char> bytes;
    if (hex.size() % 2 != 0) return bytes;
    bytes.reserve(hex.size() / 2);

    for (size_t i = 0; i < hex.size(); i += 2) {
        const auto byteString = std::string(hex.substr(i, 2));
        char* end = nullptr;
        auto val = static_cast<unsigned char>(std::strtoul(byteString.c_str(), &end, 16));
        if (end != byteString.c_str() + 2) {
            return {};
        }
        bytes.push_back(val);
    }
    return bytes;
}

std::string CryptoVault::getOrCreateMasterSecret() const {
    if (!m_cachedMasterSecret.empty()) {
        return m_cachedMasterSecret;
    }

    std::error_code ec;
    std::filesystem::create_directories(m_keyStorageDir, ec);
    const auto keyPath = m_keyStorageDir / ".vault_key";

    if (std::filesystem::exists(keyPath, ec)) {
        std::ifstream file(keyPath, std::ios::in | std::ios::binary);
        if (file) {
            std::string content;
            file >> content;
            if (content.length() >= 32) {
                m_cachedMasterSecret = content;
                return m_cachedMasterSecret;
            }
        }
    }

    // Generate random 256-bit machine master secret
    std::vector<unsigned char> rawKey(32);
    if (RAND_bytes(rawKey.data(), static_cast<int>(rawKey.size())) <= 0) {
        // Fallback pseudorandom if OpenSSL RAND fails
        for (auto& b : rawKey) b = static_cast<unsigned char>(std::rand() % 256);
    }
    const std::string newSecret = toHex(rawKey);

    std::ofstream out(keyPath, std::ios::out | std::ios::trunc | std::ios::binary);
    if (out) {
        out << newSecret;
        out.close();
        // Restrict file permissions to owner read/write only (chmod 0600)
        std::filesystem::permissions(
            keyPath,
            std::filesystem::perms::owner_read | std::filesystem::perms::owner_write,
            std::filesystem::perm_options::replace,
            ec
        );
    }

    m_cachedMasterSecret = newSecret;
    return m_cachedMasterSecret;
}

std::expected<core::git::bo::EncryptedSecret, std::string>
CryptoVault::encryptWithMasterSecret(const std::string& plaintext, const std::string& masterSecret) {
    if (plaintext.empty()) {
        return core::git::bo::EncryptedSecret{};
    }

    // 1. Generate random Salt (16 bytes)
    std::vector<unsigned char> salt(kSaltBytes);
    if (RAND_bytes(salt.data(), static_cast<int>(salt.size())) <= 0) {
        return std::unexpected("Failed to generate random salt");
    }

    // 2. Derive 256-bit AES key via Argon2id
    auto keyRes = deriveArgon2idKey(masterSecret, salt);
    if (!keyRes) {
        return std::unexpected(keyRes.error());
    }
    const auto& key = *keyRes;

    // 3. Generate random IV (12 bytes for GCM)
    std::vector<unsigned char> iv(kIvBytes);
    if (RAND_bytes(iv.data(), static_cast<int>(iv.size())) <= 0) {
        return std::unexpected("Failed to generate random IV");
    }

    // 4. Encrypt using AES-256-GCM
    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx) {
        return std::unexpected("Failed to create OpenSSL cipher context");
    }

    if (EVP_EncryptInit_ex(ctx, EVP_aes_256_gcm(), nullptr, nullptr, nullptr) <= 0 ||
        EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, static_cast<int>(iv.size()), nullptr) <= 0 ||
        EVP_EncryptInit_ex(ctx, nullptr, nullptr, key.data(), iv.data()) <= 0) {
        EVP_CIPHER_CTX_free(ctx);
        return std::unexpected("Failed to initialize AES-256-GCM cipher");
    }

    std::vector<unsigned char> ciphertext(plaintext.size());
    int len = 0;
    if (EVP_EncryptUpdate(ctx, ciphertext.data(), &len,
                           reinterpret_cast<const unsigned char*>(plaintext.data()),
                           static_cast<int>(plaintext.size())) <= 0) {
        EVP_CIPHER_CTX_free(ctx);
        return std::unexpected("AES-256-GCM encryption update failed");
    }
    int ciphertextLen = len;

    if (EVP_EncryptFinal_ex(ctx, ciphertext.data() + len, &len) <= 0) {
        EVP_CIPHER_CTX_free(ctx);
        return std::unexpected("AES-256-GCM encryption finalization failed");
    }
    ciphertextLen += len;
    ciphertext.resize(ciphertextLen);

    // 5. Get 16-byte authentication tag
    std::vector<unsigned char> tag(kTagBytes);
    if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_GET_TAG, static_cast<int>(tag.size()), tag.data()) <= 0) {
        EVP_CIPHER_CTX_free(ctx);
        return std::unexpected("Failed to get AES-256-GCM authentication tag");
    }
    EVP_CIPHER_CTX_free(ctx);

    core::git::bo::EncryptedSecret secret;
    secret.ciphertextHex = toHex(ciphertext);
    secret.saltHex = toHex(salt);
    secret.ivHex = toHex(iv);
    secret.tagHex = toHex(tag);
    return secret;
}

std::expected<std::string, std::string>
CryptoVault::decryptWithMasterSecret(const core::git::bo::EncryptedSecret& secret, const std::string& masterSecret) {
    if (secret.empty()) {
        return "";
    }

    const auto salt = fromHex(secret.saltHex);
    const auto iv = fromHex(secret.ivHex);
    const auto tag = fromHex(secret.tagHex);
    const auto ciphertext = fromHex(secret.ciphertextHex);

    if (salt.empty() || iv.empty() || tag.empty()) {
        return std::unexpected("Invalid or corrupted encrypted secret components");
    }

    // 1. Derive 256-bit AES key via Argon2id using stored salt
    auto keyRes = deriveArgon2idKey(masterSecret, salt);
    if (!keyRes) {
        return std::unexpected(keyRes.error());
    }
    const auto& key = *keyRes;

    // 2. Decrypt using AES-256-GCM
    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx) {
        return std::unexpected("Failed to create OpenSSL cipher context for decryption");
    }

    if (EVP_DecryptInit_ex(ctx, EVP_aes_256_gcm(), nullptr, nullptr, nullptr) <= 0 ||
        EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, static_cast<int>(iv.size()), nullptr) <= 0 ||
        EVP_DecryptInit_ex(ctx, nullptr, nullptr, key.data(), iv.data()) <= 0) {
        EVP_CIPHER_CTX_free(ctx);
        return std::unexpected("Failed to initialize AES-256-GCM decryption");
    }

    std::vector<unsigned char> plaintext(ciphertext.size());
    int len = 0;
    if (EVP_DecryptUpdate(ctx, plaintext.data(), &len, ciphertext.data(), static_cast<int>(ciphertext.size())) <= 0) {
        EVP_CIPHER_CTX_free(ctx);
        return std::unexpected("AES-256-GCM decryption update failed");
    }
    int plaintextLen = len;

    // Set expected verification tag
    if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_TAG, static_cast<int>(tag.size()),
                            const_cast<unsigned char*>(tag.data())) <= 0) {
        EVP_CIPHER_CTX_free(ctx);
        return std::unexpected("Failed to set verification tag");
    }

    const int finalRes = EVP_DecryptFinal_ex(ctx, plaintext.data() + len, &len);
    EVP_CIPHER_CTX_free(ctx);

    if (finalRes <= 0) {
        return std::unexpected("Authentication tag verification failed: secret was tampered with or corrupted");
    }
    plaintextLen += len;
    plaintext.resize(plaintextLen);

    return std::string(reinterpret_cast<char*>(plaintext.data()), plaintext.size());
}

std::expected<core::git::bo::EncryptedSecret, std::string>
CryptoVault::encrypt(const std::string& plaintext) const {
    const auto masterSecret = getOrCreateMasterSecret();
    return encryptWithMasterSecret(plaintext, masterSecret);
}

std::expected<std::string, std::string>
CryptoVault::decrypt(const core::git::bo::EncryptedSecret& secret) const {
    const auto masterSecret = getOrCreateMasterSecret();
    return decryptWithMasterSecret(secret, masterSecret);
}

} // namespace gf::infra::security
