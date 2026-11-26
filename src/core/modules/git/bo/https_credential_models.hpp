#pragma once

#include <functional>
#include <optional>
#include <string>

namespace gf::git::bo
{
    /// Credentials for one git command over HTTP(S): `password` is a personal access token.
    struct HttpsCredential
    {
        std::string username;
        std::string password;
        /// Server the credential belongs to (lower-case, no port). When set, git only receives
        /// it for that host, never for another remote or a redirect target. Empty = any host.
        std::string host{};
    };

    /// Produces the credential for one git command. Called on the worker thread that runs git
    /// (it may decrypt a saved token); returns nullopt when there is nothing to use.
    using HttpsCredentialLoader = std::function<std::optional<HttpsCredential>()>;
} // namespace gf::git::bo
