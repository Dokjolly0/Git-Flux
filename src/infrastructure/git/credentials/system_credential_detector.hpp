#pragma once

#include "core/modules/git/bo/git_provider_profile.hpp"
#include <vector>
#include <string>
#include <filesystem>

namespace gf::infra::git::credentials
{
    class SystemCredentialDetector
    {
    public:
        explicit SystemCredentialDetector(std::filesystem::path homeDir = "");

        /// Detects all Git provider profiles configured on the system (SSH config, SSH keys, env vars, git config).
        [[nodiscard]] std::vector<core::git::bo::GitProviderProfile> detectProfiles() const;

        /// Reads user.name and user.email from global git config.
        [[nodiscard]] std::pair<std::string, std::string> detectGlobalGitUser() const;

    private:
        std::filesystem::path m_homeDir;

        void parseSshConfig(std::vector<core::git::bo::GitProviderProfile>& profiles) const;
        void checkDefaultSshKeys(std::vector<core::git::bo::GitProviderProfile>& profiles) const;
        void checkEnvironmentTokens(std::vector<core::git::bo::GitProviderProfile>& profiles) const;
        [[nodiscard]] std::filesystem::path expandTilde(const std::string& path) const;
    };
} // namespace gf::infra::git::credentials
