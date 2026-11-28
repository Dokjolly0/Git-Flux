#include "core/modules/git/bl/remote_access_diagnostics.hpp"
#include "core/modules/git/bo/git_provider_profile.hpp"
#include "core/shared/string_utils.hpp"

#include <algorithm>
#include <array>
#include <cctype>

namespace gf::core::git::bl
{
    namespace
    {
        bool containsNoCase(const std::string_view haystack, const std::string_view needle) noexcept
        {
            if (needle.empty()) return true;
            const auto match = std::ranges::search(haystack, needle,
                                                   [](const char a, const char b)
                                                   {
                                                       return std::tolower(static_cast<unsigned char>(a)) ==
                                                           std::tolower(static_cast<unsigned char>(b));
                                                   });
            return !match.empty();
        }

        template <std::size_t N>
        bool containsAny(const std::string_view haystack, const std::array<std::string_view, N>& needles) noexcept
        {
            return std::ranges::any_of(needles,
                                       [&](const std::string_view n) { return containsNoCase(haystack, n); });
        }

        bool isNameChar(const char c) noexcept
        {
            const auto u = static_cast<unsigned char>(c);
            return std::isalnum(u) || c == '-' || c == '_' || c == '.';
        }

        // Reads an account name starting at `pos` (skipping a leading '@').
        std::string readName(const std::string_view text, std::size_t pos)
        {
            if (pos < text.size() && text[pos] == '@') ++pos;
            std::size_t end = pos;
            while (end < text.size() && isNameChar(text[end])) ++end;
            auto name = std::string(text.substr(pos, end - pos));
            while (!name.empty() && name.back() == '.') name.pop_back();
            return name;
        }
    } // namespace

    std::string_view remoteAccessIssueToString(const RemoteAccessIssue issue) noexcept
    {
        switch (issue)
        {
        case RemoteAccessIssue::None: return "ok";
        case RemoteAccessIssue::SshKeyRejected: return "sshKeyRejected";
        case RemoteAccessIssue::SshHostKeyUnknown: return "sshHostKeyUnknown";
        case RemoteAccessIssue::SshHostKeyChanged: return "sshHostKeyChanged";
        case RemoteAccessIssue::HostNotFound: return "hostNotFound";
        case RemoteAccessIssue::ConnectionFailed: return "connectionFailed";
        case RemoteAccessIssue::HttpsAuthFailed: return "httpsAuthFailed";
        case RemoteAccessIssue::RepositoryNotFound: return "repositoryNotFound";
        case RemoteAccessIssue::Unknown: return "unknown";
        }
        return "unknown";
    }

    RemoteAccessIssue classifyRemoteAccessOutput(const std::string_view output) noexcept
    {
        // Order matters: a changed host key also prints "Host key verification failed".
        if (containsNoCase(output, "REMOTE HOST IDENTIFICATION HAS CHANGED"))
        {
            return RemoteAccessIssue::SshHostKeyChanged;
        }
        if (containsNoCase(output, "Host key verification failed") ||
            containsNoCase(output, "No ED25519 host key is known") ||
            containsNoCase(output, "No RSA host key is known") ||
            containsNoCase(output, "No ECDSA host key is known"))
        {
            return RemoteAccessIssue::SshHostKeyUnknown;
        }
        if (containsNoCase(output, "Permission denied (publickey") ||
            containsNoCase(output, "Permission denied, please try again") ||
            containsNoCase(output, "no mutual signature algorithm"))
        {
            return RemoteAccessIssue::SshKeyRejected;
        }
        constexpr std::array<std::string_view, 3> kDns = {
            "Could not resolve hostname", "Could not resolve host", "Name or service not known"
        };
        if (containsAny(output, kDns))
        {
            return RemoteAccessIssue::HostNotFound;
        }
        constexpr std::array<std::string_view, 7> kConnection = {
            "Connection refused", "Connection timed out", "Operation timed out", "No route to host",
            "Network is unreachable", "Connection closed by", "Failed to connect to"
        };
        if (containsAny(output, kConnection))
        {
            return RemoteAccessIssue::ConnectionFailed;
        }
        constexpr std::array<std::string_view, 6> kHttpsAuth = {
            "Authentication failed", "could not read Username", "could not read Password",
            "Invalid username or password", "The requested URL returned error: 401",
            "The requested URL returned error: 403"
        };
        if (containsAny(output, kHttpsAuth))
        {
            return RemoteAccessIssue::HttpsAuthFailed;
        }
        constexpr std::array<std::string_view, 5> kNotFound = {
            "Repository not found", "does not appear to be a git repository", "not found",
            "The requested URL returned error: 404", "does not exist"
        };
        if (containsAny(output, kNotFound))
        {
            return RemoteAccessIssue::RepositoryNotFound;
        }
        constexpr std::array<std::string_view, 5> kGreeting = {
            "successfully authenticated", "Welcome to GitLab", "authenticated via ssh key",
            "logged in as", "You can use git"
        };
        if (containsAny(output, kGreeting))
        {
            return RemoteAccessIssue::None;
        }
        return RemoteAccessIssue::Unknown;
    }

    std::string extractSshGreetingAccount(const std::string_view output)
    {
        // Gitea / Forgejo: "Hi there, alex! You've successfully authenticated..."
        constexpr std::string_view kHiThere = "Hi there, ";
        if (const auto pos = output.find(kHiThere); pos != std::string_view::npos)
        {
            return readName(output, pos + kHiThere.size());
        }
        // GitHub: "Hi octocat! You've successfully authenticated..."
        constexpr std::string_view kHi = "Hi ";
        if (const auto pos = output.find(kHi); pos != std::string_view::npos)
        {
            return readName(output, pos + kHi.size());
        }
        // GitLab: "Welcome to GitLab, @jane!"
        constexpr std::string_view kGitLab = "Welcome to GitLab, ";
        if (const auto pos = output.find(kGitLab); pos != std::string_view::npos)
        {
            return readName(output, pos + kGitLab.size());
        }
        // Bitbucket: "authenticated via ssh key.\n\nYou can use git to connect to Bitbucket." /
        // "logged in as jane."
        constexpr std::string_view kLoggedIn = "logged in as ";
        if (const auto pos = output.find(kLoggedIn); pos != std::string_view::npos)
        {
            return readName(output, pos + kLoggedIn.size());
        }
        return {};
    }

    bool isSshRemoteUrl(const std::string_view url) noexcept
    {
        return bo::isSshSchemeOrScpUrl(url);
    }

    std::string extractRemoteHost(const std::string_view url)
    {
        const auto parsed = bo::parseRemoteUrl(url);
        if (!parsed || parsed->host.empty()) return {};
        return core::toLowerAscii(parsed->host);
    }
} // namespace gf::core::git::bl
