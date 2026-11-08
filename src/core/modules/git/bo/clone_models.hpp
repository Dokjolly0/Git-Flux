#pragma once

#include <chrono>
#include <filesystem>
#include <optional>
#include <string>

namespace gf::git::bo {

struct CloneRepositoryOptions {
    std::string remoteUrl{};
    std::filesystem::path destinationPath{};
    std::optional<std::string> branch{std::nullopt};
    bool recursiveSubmodules{false};
    std::chrono::milliseconds timeout{180'000};
};

struct CloneRepositoryResult {
    std::filesystem::path repositoryPath{};
    std::string displayName{};
    std::string initialBranch{};
};

} // namespace gf::git::bo
