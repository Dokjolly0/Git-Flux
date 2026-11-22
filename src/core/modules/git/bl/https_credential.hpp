#pragma once

#include "core/modules/git/bo/git_command.hpp"

#include <string>
#include <string_view>

namespace gf::git::bl
{
    /// Credentials for one git command over HTTP(S): `password` is a personal access token.
    struct HttpsCredential
    {
        std::string username;
        std::string password;
    };

    [[nodiscard]] inline bool isHttpRemoteUrl(std::string_view url) noexcept
    {
        return url.starts_with("https://") || url.starts_with("http://");
    }

    /// Makes `command` authenticate with `credential` without persisting or exposing it:
    ///  - `git -c credential.helper= -c credential.helper=!f...` (before the subcommand, so it is
    ///    NOT written to the cloned repository's config) disables other helpers for this run and
    ///    installs a one-shot helper;
    ///  - the helper reads the values from environment variables of this git process only, so
    ///    the token never appears on the command line (visible to every user through `ps`).
    inline void applyHttpsCredential(bo::GitCommand& command, const HttpsCredential& credential)
    {
        static constexpr std::string_view kHelper =
            "credential.helper=!f() { test \"$1\" = get || exit 0; "
            "echo \"username=${GF_GIT_USERNAME}\"; echo \"password=${GF_GIT_PASSWORD}\"; }; f";
        command.args.insert(command.args.begin(), {
                                "-c", "credential.helper=",
                                "-c", std::string(kHelper),
                            });
        command.extraEnvironment.emplace_back("GF_GIT_USERNAME", credential.username);
        command.extraEnvironment.emplace_back("GF_GIT_PASSWORD", credential.password);
    }
} // namespace gf::git::bl
