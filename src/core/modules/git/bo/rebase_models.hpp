#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

namespace gf::git::bo
{
    enum class RebaseStatus
    {
        UpToDate,
        Applied,
        Conflict,
        Failed,
        Aborted,
        Skipped,
        Finished
    };

    struct RebaseOptions
    {
        std::string upstream{};
        std::optional<std::string> onto{std::nullopt};
        std::optional<std::string> branch{std::nullopt};
        bool autostash{false};
    };

    struct RebaseResult
    {
        RebaseStatus status{RebaseStatus::Applied};
        std::size_t currentStep{0};
        std::size_t totalSteps{0};
        std::string currentCommitSha{};
        std::vector<std::string> conflictedFiles{};
        std::string message{};
    };

    struct CherryPickOptions
    {
        std::string commitSha{};
        bool noCommit{false};
    };

    struct CherryPickResult
    {
        bool success{false};
        bool hasConflicts{false};
        std::string newCommitSha{};
        std::vector<std::string> conflictedFiles{};
        std::string message{};
    };
} // namespace gf::git::bo
