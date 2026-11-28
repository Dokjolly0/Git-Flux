#include "infrastructure/git/credentials/system_credential_detector.hpp"
#include "core/shared/env_var.hpp"
#include "core/shared/string_utils.hpp"

#include <fstream>
#include <sstream>
#include <algorithm>
#include <array>
#include <string_view>

namespace gf::infra::git::credentials
{
    namespace
    {
        std::string trim(const std::string_view str)
        {
            return std::string(core::trim(str));
        }

        std::string toLower(const std::string_view str)
        {
            return core::toLowerAscii(str);
        }

        std::filesystem::path getSystemHomeDir()
        {
            auto home = core::getEnvVar("HOME");
            if (!home)
            {
                home = core::getEnvVar("USERPROFILE");
            }
            if (home)
            {
                return {*home};
            }
            return std::filesystem::current_path();
        }
    } // namespace

    SystemCredentialDetector::SystemCredentialDetector(std::filesystem::path homeDir)
        : m_homeDir(homeDir.empty() ? getSystemHomeDir() : std::move(homeDir))
    {
    }

    std::filesystem::path SystemCredentialDetector::expandTilde(const std::string& path) const
    {
        if (path.starts_with("~/") || path.starts_with("~\\"))
        {
            return m_homeDir / path.substr(2);
        }
        if (path == "~")
        {
            return m_homeDir;
        }
        return {path};
    }

    std::string SystemCredentialDetector::detectGlobalGitUserName() const
    {
        std::string name;

        // Check ~/.gitconfig
        const auto gitConfigPath = m_homeDir / ".gitconfig";
        if (std::error_code ec; std::filesystem::exists(gitConfigPath, ec))
        {
            std::ifstream file(gitConfigPath);
            std::string line;
            bool inUserSection = false;

            while (std::getline(file, line))
            {
                line = trim(line);
                if (line.starts_with('[') && line.ends_with(']'))
                {
                    inUserSection = line == "[user]";
                    continue;
                }
                if (inUserSection && line.starts_with("name"))
                {
                    if (const auto eq = line.find('='); eq != std::string::npos)
                    {
                        name = trim(line.substr(eq + 1));
                    }
                }
            }
        }

        return name;
    }

    void SystemCredentialDetector::parseSshConfig(std::vector<core::git::bo::GitProviderProfile>& profiles) const
    {
        const auto sshConfigPath = m_homeDir / ".ssh" / "config";
        std::error_code ec;
        if (!std::filesystem::exists(sshConfigPath, ec))
        {
            return;
        }

        std::ifstream file(sshConfigPath);
        std::string line;
        // Concrete aliases of the current "Host" block (patterns like "*" or "!foo" are dropped).
        std::vector<std::string> currentAliases;
        std::string currentHostName;
        std::string currentIdentityFile;
        std::string currentUser;
        int currentPort = 0; // 0 = not set (ssh default 22)

        auto finalizeBlock = [&]
        {
            if (currentAliases.empty())
            {
                return;
            }

            const auto& primaryAlias = currentAliases.front();
            const auto hostToUse = !currentHostName.empty() ? currentHostName : primaryAlias;
            const auto keyPath = expandTilde(currentIdentityFile);
            const bool keyExists = !currentIdentityFile.empty() && std::filesystem::exists(keyPath, ec);

            // Check if this host is actually a Git host (filter out general VPS/SSH boxes like remote-viewer, alex-violatto)
            const auto lowerHostName = toLower(currentHostName);
            const auto lowerUser = toLower(currentUser);
            const auto lowerIdentity = toLower(currentIdentityFile);

            constexpr std::array<std::string_view, 6> kForgeNames = {
                "github", "gitlab", "bitbucket", "gitea", "forgejo", "codeberg"
            };
            auto looksLikeGitHost = [&](const std::string& name)
            {
                if (name.empty()) return false;
                for (const auto forge : kForgeNames)
                {
                    if (name.find(forge) != std::string::npos) return true;
                }
                return name.starts_with("git.") || name.find(".git.") != std::string::npos;
            };

            bool isGitHost = lowerUser == "git" ||
                looksLikeGitHost(lowerHostName) ||
                lowerIdentity.find("git") != std::string::npos;
            for (const auto& alias : currentAliases)
            {
                isGitHost = isGitHost || looksLikeGitHost(toLower(alias));
            }

            if (!isGitHost)
            {
                return;
            }

            auto applySshSettings = [&](core::git::bo::GitProviderProfile& p)
            {
                if (keyExists)
                {
                    p.hasSshKey = true;
                    p.sshKeyPath = keyPath.string();
                    p.preferredProtocol = "ssh";
                }
                // First block wins (ssh itself applies the first match), so don't override.
                if (p.sshHostAlias.empty())
                {
                    p.sshHostAlias = primaryAlias;
                    p.sshPort = currentPort;
                }
            };

            // Find if profile for host already exists (built-in provider or an earlier block)
            const auto it = std::ranges::find_if(profiles, [&](const auto& p)
            {
                if (p.host == hostToUse) return true;
                return std::ranges::find(currentAliases, p.host) != currentAliases.end();
            });

            if (it != profiles.end())
            {
                applySshSettings(*it);
            }
            else
            {
                // New custom host profile (e.g. git.alexviolatto.dev)
                core::git::bo::GitProviderProfile custom;
                custom.id = "custom_" + primaryAlias;
                custom.providerType = core::git::bo::GitProviderType::Custom;
                custom.displayName = primaryAlias;
                custom.host = hostToUse;
                custom.username = currentUser;
                custom.preferredProtocol = "ssh";
                // The SSH port never applies to the web API: always https://<host>
                custom.customApiUrl = "https://" + hostToUse;
                applySshSettings(custom);
                profiles.push_back(custom);
            }
        };

        while (std::getline(file, line))
        {
            line = trim(line);
            if (line.empty() || line.starts_with('#'))
            {
                continue;
            }

            // ssh_config also accepts "Key=Value" and "Key = Value"
            if (const auto eq = line.find('='); eq != std::string::npos)
            {
                if (const auto firstSpace = line.find_first_of(" \t");
                    firstSpace == std::string::npos || eq < firstSpace ||
                    line.find_first_not_of(" \t", firstSpace) == eq)
                {
                    line[eq] = ' ';
                }
            }

            std::istringstream iss(line);
            std::string key;
            iss >> key;
            key = toLower(key);

            if (key == "host")
            {
                finalizeBlock();
                currentAliases.clear();
                currentHostName = "";
                currentIdentityFile = "";
                currentUser = "";
                currentPort = 0;

                // "Host a b c": every non-pattern token is an alias of the same block
                std::string alias;
                while (iss >> alias)
                {
                    if (alias.find_first_of("*?!") != std::string::npos) continue;
                    currentAliases.push_back(alias);
                }
            }
            else if (key == "match")
            {
                // A Match block isn't tied to a concrete host name: ignore its settings.
                finalizeBlock();
                currentAliases.clear();
                currentHostName = "";
                currentIdentityFile = "";
                currentUser = "";
                currentPort = 0;
            }
            else if (currentHostName.empty() && key == "hostname")
            {
                iss >> currentHostName;
            }
            else if (currentIdentityFile.empty() && key == "identityfile")
            {
                std::string path;
                iss >> path;
                if (path.size() >= 2 && path.front() == '"' && path.back() == '"')
                {
                    path = path.substr(1, path.size() - 2);
                }
                currentIdentityFile = path;
            }
            else if (currentUser.empty() && key == "user")
            {
                iss >> currentUser;
            }
            else if (currentPort == 0 && key == "port")
            {
                if (int port = 0; iss >> port && port > 0 && port <= 65535)
                {
                    currentPort = port;
                }
            }
        }
        finalizeBlock();
    }

    void SystemCredentialDetector::checkDefaultSshKeys(std::vector<core::git::bo::GitProviderProfile>& profiles) const
    {
        const auto sshDir = m_homeDir / ".ssh";
        std::error_code ec;
        if (!std::filesystem::exists(sshDir, ec))
        {
            return;
        }

        auto checkProfileKey = [&](const std::string& profileId, const std::vector<std::string>& keyNames)
        {
            const auto it = std::ranges::find_if(profiles, [&](const auto& p)
            {
                return p.id == profileId;
            });
            if (it == profiles.end() || it->hasSshKey) return;

            for (const auto& name : keyNames)
            {
                if (const auto path = sshDir / name; std::filesystem::exists(path, ec))
                {
                    it->hasSshKey = true;
                    it->sshKeyPath = path.string();
                    it->preferredProtocol = "ssh";
                    break;
                }
            }
        };

        checkProfileKey("github", {"id_ed25519_github", "id_rsa_github", "id_ed25519", "id_rsa"});
        checkProfileKey("bitbucket", {"id_ed25519_bitbucket", "id_rsa_bitbucket", "id_ed25519", "id_rsa"});
        checkProfileKey("gitlab", {"id_ed25519_gitlab", "id_rsa_gitlab", "id_ed25519", "id_rsa"});
    }

    void SystemCredentialDetector::checkEnvironmentTokens(
        std::vector<core::git::bo::GitProviderProfile>& profiles)
    {
        auto setEnvToken = [&](const std::string& profileId, const char* envVar)
        {
            if (const auto val = core::getEnvVar(envVar); val && !val->empty())
            {
                const auto it = std::ranges::find_if(profiles, [&](const auto& p)
                {
                    return p.id == profileId;
                });
                if (it != profiles.end() && !it->hasToken)
                {
                    it->hasToken = true;
                    // If token exists, we mark hasToken so the UI knows it's configured
                }
            }
        };

        setEnvToken("github", "GITHUB_TOKEN");
        setEnvToken("github", "GH_TOKEN");
        setEnvToken("gitlab", "GITLAB_TOKEN");
        setEnvToken("bitbucket", "BITBUCKET_TOKEN");
        setEnvToken("bitbucket", "BITBUCKET_APP_PASSWORD");
    }

    std::vector<core::git::bo::GitProviderProfile> SystemCredentialDetector::detectProfiles() const
    {
        std::vector<core::git::bo::GitProviderProfile> profiles;

        const std::string userName = detectGlobalGitUserName();

        // 1. Standard GitHub Profile
        {
            core::git::bo::GitProviderProfile p;
            p.id = "github";
            p.providerType = core::git::bo::GitProviderType::GitHub;
            p.displayName = "GitHub";
            p.host = "github.com";
            p.username = userName;
            p.preferredProtocol = "ssh";
            profiles.push_back(p);
        }

        // 2. Standard Bitbucket Profile
        {
            core::git::bo::GitProviderProfile p;
            p.id = "bitbucket";
            p.providerType = core::git::bo::GitProviderType::Bitbucket;
            p.displayName = "Bitbucket";
            p.host = "bitbucket.org";
            p.username = userName;
            p.preferredProtocol = "ssh";
            profiles.push_back(p);
        }

        // 3. Standard GitLab Profile
        {
            core::git::bo::GitProviderProfile p;
            p.id = "gitlab";
            p.providerType = core::git::bo::GitProviderType::GitLab;
            p.displayName = "GitLab";
            p.host = "gitlab.com";
            p.username = userName;
            p.preferredProtocol = "ssh";
            profiles.push_back(p);
        }

        // 4. Parse ~/.ssh/config for host-specific keys and custom git servers
        parseSshConfig(profiles);

        // 5. Check default SSH keys in ~/.ssh/
        checkDefaultSshKeys(profiles);

        // 6. Check environment tokens
        checkEnvironmentTokens(profiles);

        return profiles;
    }
} // namespace gf::infra::git::credentials
