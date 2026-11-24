#pragma once

#include "core/modules/git/bo/git_command.hpp"
#include "core/modules/git/bo/https_credential_models.hpp"

#include <string>
#include <string_view>

namespace gf::git::bl
{
    using HttpsCredential = bo::HttpsCredential;

    [[nodiscard]] inline bool isHttpRemoteUrl(const std::string_view url) noexcept
    {
        return url.starts_with("https://") || url.starts_with("http://");
    }

    /// Makes `command` authenticate with `credential` without persisting or exposing it:
    ///  - `git -c credential.helper= -c credential.helper=!f...` (before the subcommand, so it is
    ///    NOT written to the repository's config) disables other helpers for this run and
    ///    installs a one-shot helper;
    ///  - the helper reads the values from environment variables of this git process only, so
    ///    the token never appears on the command line (visible to every user through `ps`);
    ///  - when `credential.host` is set, the helper answers only for that host (git passes
    ///    `host=name[:port]` on stdin), so `fetch --all` or a redirect never leaks the token.
    inline void applyHttpsCredential(bo::GitCommand& command, const HttpsCredential& credential)
    {
        static constexpr std::string_view kHelper =
            "credential.helper=!f() { test \"$1\" = get || exit 0; h=; "
            "while IFS= read -r l; do test -n \"$l\" || break; "
            "case \"$l\" in host=*) h=\"${l#host=}\";; esac; done; "
            "test -z \"$GF_GIT_HOST\" || test \"${h%%:*}\" = \"$GF_GIT_HOST\" || exit 0; "
            "echo \"username=${GF_GIT_USERNAME}\"; echo \"password=${GF_GIT_PASSWORD}\"; }; f";
        command.args.insert(command.args.begin(), {
                                "-c", "credential.helper=",
                                "-c", std::string(kHelper),
                            });
        command.extraEnvironment.emplace_back("GF_GIT_USERNAME", credential.username);
        command.extraEnvironment.emplace_back("GF_GIT_PASSWORD", credential.password);
        command.extraEnvironment.emplace_back("GF_GIT_HOST", credential.host);
    }

    /// Runs `loader` (if any) and applies the credential it returns.
    inline void applyHttpsCredential(bo::GitCommand& command, const bo::HttpsCredentialLoader& loader)
    {
        if (!loader) return;
        if (const auto credential = loader(); credential && !credential->password.empty())
        {
            applyHttpsCredential(command, *credential);
        }
    }
} // namespace gf::git::bl
