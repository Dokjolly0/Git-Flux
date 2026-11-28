#pragma once

#include "core/shared/string_utils.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace gf::core::git::bo
{
    enum class GitProviderType
    {
        GitHub,
        Bitbucket,
        GitLab,
        Custom
    };

    [[nodiscard]] inline std::string gitProviderTypeToString(const GitProviderType type)
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

    struct ParsedRemoteUrl
    {
        std::string scheme;
        std::string user;
        std::string host;
        std::string port;
        std::string path;
        bool isScpLike{false};
    };

    [[nodiscard]] inline bool isSshSchemeOrScpUrl(const std::string_view url) noexcept
    {
        if (url.starts_with("ssh://") || url.starts_with("git+ssh://") || url.starts_with("ssh+git://"))
        {
            return true;
        }
        if (url.find("://") != std::string_view::npos) return false;
        // scp-like "user@host:path" / "alias:path": a ':' before any '/' (and not "C:\..." drives)
        const auto colon = url.find(':');
        if (colon == std::string_view::npos || colon < 2) return false;
        const auto slash = url.find('/');
        return slash == std::string_view::npos || colon < slash;
    }

    [[nodiscard]] inline std::optional<ParsedRemoteUrl> parseRemoteUrl(const std::string_view url)
    {
        if (url.empty()) return std::nullopt;
        ParsedRemoteUrl parsed;
        if (const auto schemePos = url.find("://"); schemePos != std::string_view::npos)
        {
            parsed.scheme = std::string(url.substr(0, schemePos));
            const auto rest = url.substr(schemePos + 3);
            const auto slash = rest.find('/');
            std::string_view authority = slash == std::string_view::npos ? rest : rest.substr(0, slash);
            if (slash != std::string_view::npos)
            {
                parsed.path = std::string(rest.substr(slash + 1));
            }
            if (const auto at = authority.rfind('@'); at != std::string_view::npos)
            {
                parsed.user = std::string(authority.substr(0, at));
                authority.remove_prefix(at + 1);
            }
            if (authority.starts_with('['))
            {
                const auto close = authority.find(']');
                if (close == std::string_view::npos) return std::nullopt;
                parsed.host = std::string(authority.substr(1, close - 1));
                const auto after = authority.substr(close + 1);
                if (after.starts_with(':')) parsed.port = std::string(after.substr(1));
                return parsed;
            }
            if (const auto colon = authority.rfind(':'); colon != std::string_view::npos)
            {
                parsed.port = std::string(authority.substr(colon + 1));
                authority = authority.substr(0, colon);
            }
            parsed.host = std::string(authority);
            return parsed;
        }

        if (!isSshSchemeOrScpUrl(url)) return std::nullopt;
        parsed.isScpLike = true;
        const auto colon = url.find(':');
        auto authority = url.substr(0, colon);
        parsed.path = std::string(url.substr(colon + 1));
        if (const auto at = authority.rfind('@'); at != std::string_view::npos)
        {
            parsed.user = std::string(authority.substr(0, at));
            authority.remove_prefix(at + 1);
        }
        parsed.host = std::string(authority);
        if (parsed.path.starts_with('/')) parsed.path.erase(0, 1);
        return parsed;
    }

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

        const auto parsed = parseRemoteUrl(url);
        if (!parsed) return url;
        if (!parsed->isScpLike && parsed->scheme != "ssh") return url;
        if (parsed->scheme == "ssh" && url.find('/', 6) == std::string::npos) return url;

        if (parsed->host.empty() || core::toLowerAscii(parsed->host) != core::toLowerAscii(profile.host))
        {
            return url;
        }

        constexpr std::string_view kScheme = "ssh://";
        const std::string userPart = parsed->user.empty() ? std::string{} : parsed->user + "@";
        if (hasAlias)
        {
            // The alias block supplies HostName/Port/IdentityFile; keep the server's user, and
            // the server's port only when the block doesn't set one.
            if (!hasPort && !parsed->port.empty())
            {
                return std::string(kScheme) + userPart + profile.sshHostAlias + ":" + parsed->port + "/" + parsed->path;
            }
            return userPart + profile.sshHostAlias + ":" + parsed->path;
        }
        if (!parsed->port.empty()) return url; // the server already advertises an explicit port
        return std::string(kScheme) + userPart + parsed->host + ":" + std::to_string(profile.sshPort) + "/" +
            parsed->path;
    }
} // namespace gf::core::git::bo
