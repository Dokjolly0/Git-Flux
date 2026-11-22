#pragma once

#include "core/modules/git/bo/https_credential_models.hpp"

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

namespace gf::git::bo
{
    struct GitRemote
    {
        std::string name{};
        std::string fetchUrl{};
        std::string pushUrl{};
    };

    struct RemoteOperationOptions
    {
        std::string remoteName{"origin"};
        std::optional<std::string> branchName{std::nullopt};
        bool prune{false};
        bool setUpstream{false};
        bool force{false};
        bool tags{false};
        bool rebase{false};
        bool fastForwardOnly{false};
        bool allRemotes{false};
        /// Token for HTTPS remotes (fetch/pull/push), see bl::applyHttpsCredential. Empty = none.
        HttpsCredentialLoader httpsCredential{};
    };

    struct RemoteOperationResult
    {
        bool success{false};
        std::string output{};
        std::size_t transferredObjects{0};
        std::vector<std::string> newBranches{};
        std::vector<std::string> updatedBranches{};
    };

    enum class MergeStatus
    {
        UpToDate,
        FastForward,
        Merged,
        Conflict,
        Failed
    };

    struct MergeOptions
    {
        std::string sourceBranchOrCommit{};
        std::optional<std::string> message{std::nullopt};
        bool noFastForward{false};
        bool squash{false};
    };

    struct MergeResult
    {
        MergeStatus status{MergeStatus::Failed};
        std::optional<std::string> targetCommitSha{std::nullopt};
        std::vector<std::string> conflictedFiles{};
        std::string summaryMessage{};
    };
} // namespace gf::git::bo
