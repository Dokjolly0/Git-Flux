#pragma once

#include <string>
#include <vector>
#include <cstdint>

namespace gf::core::git::bo
{
    enum class GitProviderType
    {
        GitHub,
        Bitbucket,
        GitLab,
        Custom
    };

    [[nodiscard]] inline std::string gitProviderTypeToString(GitProviderType type)
    {
        switch (type)
        {
        case GitProviderType::GitHub: return "github";
        case GitProviderType::Bitbucket: return "bitbucket";
        case GitProviderType::GitLab: return "gitlab";
        case GitProviderType::Custom: return "custom";
        }
        return "custom";
    }

    [[nodiscard]] inline GitProviderType gitProviderTypeFromString(const std::string& str)
    {
        if (str == "github") return GitProviderType::GitHub;
        if (str == "bitbucket") return GitProviderType::Bitbucket;
        if (str == "gitlab") return GitProviderType::GitLab;
        return GitProviderType::Custom;
    }

    struct EncryptedSecret
    {
        std::string ciphertextHex;
        std::string saltHex;
        std::string ivHex;
        std::string tagHex;

        [[nodiscard]] bool empty() const noexcept
        {
            return ciphertextHex.empty();
        }
    };

    struct GitProviderProfile
    {
        std::string id; // e.g. "github", "bitbucket", "gitlab", "custom_alexgit"
        GitProviderType providerType{GitProviderType::GitHub};
        std::string displayName; // e.g. "GitHub", "Bitbucket", "GitLab", "git.alexviolatto.dev"
        std::string host; // e.g. "github.com", "bitbucket.org", "gitlab.com", "git.alexviolatto.dev"
        std::string username; // e.g. "Dokjolly0"
        EncryptedSecret tokenSecret; // Argon2id + AES-256-GCM encrypted secret
        std::string preferredProtocol{"ssh"}; // "ssh" or "https"
        std::string customApiUrl; // e.g. "https://git.alexviolatto.dev/api/v1"
        std::string sshKeyPath; // detected ~/.ssh/id_ed25519_...
        bool hasSshKey{false};
        bool hasToken{false};
        int64_t createdAtMs{0};
        int64_t updatedAtMs{0};
    };

    struct RemoteRepositoryItem
    {
        std::string name; // e.g. "Git-Flux"
        std::string fullName; // e.g. "Dokjolly0/Git-Flux"
        std::string description; // repository description
        bool isPrivate{false};
        std::string defaultBranch{"main"};
        std::string httpsUrl; // e.g. "https://github.com/Dokjolly0/Git-Flux.git"
        std::string sshUrl; // e.g. "git@github.com:Dokjolly0/Git-Flux.git"
        std::string updatedAt; // formatted date / ISO string
        int starsCount{0};
        int forksCount{0};
    };
} // namespace gf::core::git::bo
