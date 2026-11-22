#pragma once

#include <string>
#include <string_view>

namespace gf::core::git::bl
{
    /// Why talking to a Git remote failed (or that it worked), as recognised from the output of
    /// `ssh -T` or of a failed `git clone/fetch/push` (run with LC_ALL=C).
    enum class RemoteAccessIssue
    {
        None, // ssh -T authenticated fine
        SshKeyRejected, // "Permission denied (publickey)"
        SshHostKeyUnknown, // first connection: host key not in known_hosts
        SshHostKeyChanged, // "REMOTE HOST IDENTIFICATION HAS CHANGED"
        HostNotFound, // DNS: "Could not resolve hostname/host"
        ConnectionFailed, // refused / timed out / no route / unreachable
        HttpsAuthFailed, // HTTPS: wrong or missing credentials/token
        RepositoryNotFound, // remote reachable, repository missing or not visible
        Unknown
    };

    [[nodiscard]] std::string_view remoteAccessIssueToString(RemoteAccessIssue issue) noexcept;

    /// Classifies the stderr of a failed git network command or the output of `ssh -T`.
    /// `None` is only returned for an ssh greeting that confirms authentication.
    [[nodiscard]] RemoteAccessIssue classifyRemoteAccessOutput(std::string_view output) noexcept;

    /// Account name in an `ssh -T` greeting ("Hi octocat!", "Welcome to GitLab, @jane!",
    /// "Hi there, alex! You've successfully authenticated"...), empty if none.
    [[nodiscard]] std::string extractSshGreetingAccount(std::string_view output);

    /// Host of a remote URL: "git@host:path", "ssh://user@host:22/path", "https://host/path",
    /// "host:path" (ssh config alias). Empty for local paths or unparsable input.
    [[nodiscard]] std::string extractRemoteHost(std::string_view url);

    /// True for SSH remotes ("git@host:path", "ssh://..."), false for http(s)/file/local paths.
    [[nodiscard]] bool isSshRemoteUrl(std::string_view url) noexcept;
} // namespace gf::core::git::bl
