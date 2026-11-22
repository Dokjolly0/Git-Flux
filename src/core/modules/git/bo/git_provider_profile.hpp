#pragma once

#include <string>
#include <string_view>
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
        // From the matching ~/.ssh/config block (re-detected at every start, never persisted):
        std::string sshHostAlias; // e.g. "mygitea" for "Host mygitea" (empty if none)
        int sshPort{0}; // "Port" of that block, 0 when not set (ssh default 22)
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

    /// Adapts an SSH clone URL returned by a provider API to the user's ~/.ssh/config:
    ///  - if the server is reached through a "Host" alias different from its real name, the URL
    ///    uses that alias, so ssh applies the block's HostName, Port, User and IdentityFile;
    ///  - otherwise, if the block sets a non-default Port and the URL carries none, the port is
    ///    added (ssh://user@host:port/path), e.g. Gitea in Docker on 2222 advertising port 22.
    /// URLs that are not SSH, or for another host, are returned unchanged.
    [[nodiscard]] inline std::string applySshConfigToCloneUrl(const std::string& url,
                                                             const GitProviderProfile& profile)
    {
        if (url.empty()) return url;
        const bool hasAlias = !profile.sshHostAlias.empty() && profile.sshHostAlias != profile.host;
        const bool hasPort = profile.sshPort > 0 && profile.sshPort != 22;
        if (!hasAlias && !hasPort) return url;

        // Split into user, host, port and path for both "ssh://user@host:port/path" and the
        // scp-like "user@host:path" forms.
        std::string user;
        std::string host;
        std::string port;
        std::string path; // without the leading '/'
        constexpr std::string_view kScheme = "ssh://";
        if (url.starts_with(kScheme))
        {
            const auto rest = url.substr(kScheme.size());
            const auto slash = rest.find('/');
            if (slash == std::string::npos) return url;
            auto authority = rest.substr(0, slash);
            path = rest.substr(slash + 1);
            if (const auto at = authority.find('@'); at != std::string::npos)
            {
                user = authority.substr(0, at);
                authority = authority.substr(at + 1);
            }
            if (const auto colon = authority.rfind(':'); colon != std::string::npos)
            {
                port = authority.substr(colon + 1);
                authority = authority.substr(0, colon);
            }
            host = authority;
        }
        else
        {
            if (url.find("://") != std::string::npos) return url; // https://, git://...
            const auto colon = url.find(':');
            if (colon == std::string::npos) return url;
            auto authority = url.substr(0, colon);
            path = url.substr(colon + 1);
            if (const auto at = authority.find('@'); at != std::string::npos)
            {
                user = authority.substr(0, at);
                authority = authority.substr(at + 1);
            }
            host = authority;
            if (path.starts_with('/')) path.erase(0, 1);
        }

        auto lower = [](std::string s)
        {
            for (auto& c : s) c = static_cast<char>((c >= 'A' && c <= 'Z') ? c - 'A' + 'a' : c);
            return s;
        };
        if (host.empty() || lower(host) != lower(profile.host)) return url;

        const std::string userPart = user.empty() ? std::string{} : user + "@";
        if (hasAlias)
        {
            // The alias block supplies HostName/Port/IdentityFile; keep the server's user, and
            // the server's port only when the block doesn't set one.
            if (!hasPort && !port.empty())
            {
                return std::string(kScheme) + userPart + profile.sshHostAlias + ":" + port + "/" + path;
            }
            return userPart + profile.sshHostAlias + ":" + path;
        }
        if (!port.empty()) return url; // the server already advertises an explicit port
        return std::string(kScheme) + userPart + host + ":" + std::to_string(profile.sshPort) + "/" + path;
    }
} // namespace gf::core::git::bo
