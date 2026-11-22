#pragma once

#include <chrono>
#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <string_view>

namespace gf::git::bo
{
    struct CloneRepositoryOptions
    {
        std::string remoteUrl{};
        std::filesystem::path destinationPath{};
        std::optional<std::string> branch{std::nullopt};
        bool recursiveSubmodules{false};
        std::chrono::milliseconds timeout{180'000};
        std::function<void(int percent, std::string_view statusText)> progressCallback{};
        /// HTTPS only: provider account + personal access token (resolved by the caller from
        /// the saved provider profile). Ignored for SSH URLs.
        std::optional<std::string> httpsUsername{std::nullopt};
        std::optional<std::string> httpsToken{std::nullopt};
    };

    struct CloneRepositoryResult
    {
        std::filesystem::path repositoryPath{};
        std::string displayName{};
        std::string initialBranch{};
    };
} // namespace gf::git::bo
