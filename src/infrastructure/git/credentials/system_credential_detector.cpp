#include "infrastructure/git/credentials/system_credential_detector.hpp"

#include <fstream>
#include <sstream>
#include <algorithm>
#include <cstdlib>
#include <array>

namespace gf::infra::git::credentials {

namespace {

std::string trim(const std::string& str) {
    const auto first = str.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return "";
    const auto last = str.find_last_not_of(" \t\r\n");
    return str.substr(first, (last - first + 1));
}

std::filesystem::path getSystemHomeDir() {
    const char* home = std::getenv("HOME");
    if (!home) {
        home = std::getenv("USERPROFILE");
    }
    if (home) {
        return std::filesystem::path(home);
    }
    return std::filesystem::current_path();
}

} // namespace

SystemCredentialDetector::SystemCredentialDetector(std::filesystem::path homeDir)
    : m_homeDir(homeDir.empty() ? getSystemHomeDir() : std::move(homeDir)) {}

std::filesystem::path SystemCredentialDetector::expandTilde(const std::string& path) const {
    if (path.starts_with("~/") || path.starts_with("~\\")) {
        return m_homeDir / path.substr(2);
    }
    if (path == "~") {
        return m_homeDir;
    }
    return std::filesystem::path(path);
}

std::pair<std::string, std::string> SystemCredentialDetector::detectGlobalGitUser() const {
    std::string name;
    std::string email;

    // Check ~/.gitconfig
    const auto gitConfigPath = m_homeDir / ".gitconfig";
    std::error_code ec;
    if (std::filesystem::exists(gitConfigPath, ec)) {
        std::ifstream file(gitConfigPath);
        std::string line;
        bool inUserSection = false;

        while (std::getline(file, line)) {
            line = trim(line);
            if (line.starts_with("[") && line.ends_with("]")) {
                inUserSection = (line == "[user]");
                continue;
            }
            if (inUserSection) {
                if (line.starts_with("name")) {
                    auto eq = line.find('=');
                    if (eq != std::string::npos) {
                        name = trim(line.substr(eq + 1));
                    }
                } else if (line.starts_with("email")) {
                    auto eq = line.find('=');
                    if (eq != std::string::npos) {
                        email = trim(line.substr(eq + 1));
                    }
                }
            }
        }
    }

    return {name, email};
}

void SystemCredentialDetector::parseSshConfig(std::vector<core::git::bo::GitProviderProfile>& profiles) const {
    const auto sshConfigPath = m_homeDir / ".ssh" / "config";
    std::error_code ec;
    if (!std::filesystem::exists(sshConfigPath, ec)) {
        return;
    }

    std::ifstream file(sshConfigPath);
    std::string line;
    std::string currentHost;
    std::string currentHostName;
    std::string currentIdentityFile;
    std::string currentUser;
    int currentPort = 22;

    auto finalizeBlock = [&]() {
        if (currentHost.empty() || currentHost == "*") {
            return;
        }

        const auto hostToUse = !currentHostName.empty() ? currentHostName : currentHost;
        const auto keyPath = expandTilde(currentIdentityFile);
        const bool keyExists = !currentIdentityFile.empty() && std::filesystem::exists(keyPath, ec);

        // Check if this host is actually a Git host (filter out general VPS/SSH boxes like remote-viewer, alex-violatto)
        std::string lowerHost = currentHost;
        std::transform(lowerHost.begin(), lowerHost.end(), lowerHost.begin(), ::tolower);
        std::string lowerHostName = currentHostName;
        std::transform(lowerHostName.begin(), lowerHostName.end(), lowerHostName.begin(), ::tolower);
        std::string lowerUser = currentUser;
        std::transform(lowerUser.begin(), lowerUser.end(), lowerUser.begin(), ::tolower);
        std::string lowerIdentity = currentIdentityFile;
        std::transform(lowerIdentity.begin(), lowerIdentity.end(), lowerIdentity.begin(), ::tolower);

        bool isGitHost = (lowerUser == "git") ||
                         (lowerHost.find("github") != std::string::npos) ||
                         (lowerHost.find("gitlab") != std::string::npos) ||
                         (lowerHost.find("bitbucket") != std::string::npos) ||
                         (lowerHost.find("gitea") != std::string::npos) ||
                         (lowerHost.find("forgejo") != std::string::npos) ||
                         (lowerHost.find("codeberg") != std::string::npos) ||
                         (lowerHostName.find("github") != std::string::npos) ||
                         (lowerHostName.find("gitlab") != std::string::npos) ||
                         (lowerHostName.find("bitbucket") != std::string::npos) ||
                         (lowerHostName.find("gitea") != std::string::npos) ||
                         (lowerHostName.find("forgejo") != std::string::npos) ||
                         (lowerHostName.find("codeberg") != std::string::npos) ||
                         (lowerHost.starts_with("git.") || lowerHostName.starts_with("git.")) ||
                         (lowerHost.find(".git.") != std::string::npos || lowerHostName.find(".git.") != std::string::npos) ||
                         (lowerIdentity.find("git") != std::string::npos);

        if (!isGitHost) {
            return;
        }

        // Find if profile for host already exists
        auto it = std::find_if(profiles.begin(), profiles.end(), [&](const auto& p) {
            return p.host == hostToUse || p.host == currentHost;
        });

        if (it != profiles.end()) {
            if (keyExists) {
                it->hasSshKey = true;
                it->sshKeyPath = keyPath.string();
                it->preferredProtocol = "ssh";
            }
        } else {
            // New custom host profile (e.g. git.alexviolatto.dev)
            core::git::bo::GitProviderProfile custom;
            custom.id = "custom_" + currentHost;
            custom.providerType = core::git::bo::GitProviderType::Custom;
            custom.displayName = currentHost;
            custom.host = hostToUse;
            custom.username = currentUser;
            custom.preferredProtocol = "ssh";
            if (keyExists) {
                custom.hasSshKey = true;
                custom.sshKeyPath = keyPath.string();
            }
            if (currentPort != 22) {
                custom.customApiUrl = "https://" + hostToUse + ":" + std::to_string(currentPort);
            }
            profiles.push_back(custom);
        }
    };

    while (std::getline(file, line)) {
        line = trim(line);
        if (line.empty() || line.starts_with("#")) {
            continue;
        }

        std::istringstream iss(line);
        std::string key;
        iss >> key;
        std::transform(key.begin(), key.end(), key.begin(), ::tolower);

        if (key == "host") {
            finalizeBlock();
            currentHost = "";
            currentHostName = "";
            currentIdentityFile = "";
            currentUser = "";
            currentPort = 22;
            iss >> currentHost;
        } else if (key == "hostname") {
            iss >> currentHostName;
        } else if (key == "identityfile") {
            std::string path;
            iss >> path;
            currentIdentityFile = path;
        } else if (key == "user") {
            iss >> currentUser;
        } else if (key == "port") {
            iss >> currentPort;
        }
    }
    finalizeBlock();
}

void SystemCredentialDetector::checkDefaultSshKeys(std::vector<core::git::bo::GitProviderProfile>& profiles) const {
    const auto sshDir = m_homeDir / ".ssh";
    std::error_code ec;
    if (!std::filesystem::exists(sshDir, ec)) {
        return;
    }

    auto checkProfileKey = [&](const std::string& profileId, const std::vector<std::string>& keyNames) {
        auto it = std::find_if(profiles.begin(), profiles.end(), [&](const auto& p) {
            return p.id == profileId;
        });
        if (it == profiles.end() || it->hasSshKey) return;

        for (const auto& name : keyNames) {
            auto path = sshDir / name;
            if (std::filesystem::exists(path, ec)) {
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

void SystemCredentialDetector::checkEnvironmentTokens(std::vector<core::git::bo::GitProviderProfile>& profiles) const {
    auto setEnvToken = [&](const std::string& profileId, const char* envVar) {
        const char* val = std::getenv(envVar);
        if (val && std::char_traits<char>::length(val) > 0) {
            auto it = std::find_if(profiles.begin(), profiles.end(), [&](const auto& p) {
                return p.id == profileId;
            });
            if (it != profiles.end() && !it->hasToken) {
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

std::vector<core::git::bo::GitProviderProfile> SystemCredentialDetector::detectProfiles() const {
    std::vector<core::git::bo::GitProviderProfile> profiles;

    const auto [userName, userEmail] = detectGlobalGitUser();

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
