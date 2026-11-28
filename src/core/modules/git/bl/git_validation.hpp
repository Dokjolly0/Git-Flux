#pragma once

#include "core/modules/git/ports/repository_git_ops.hpp"

#include <expected>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace gf::git::bl
{
    [[nodiscard]] std::expected<void, ports::GitOpsError>
    validateRepoPath(const std::filesystem::path& repoPath,
                     std::string_view commandContext = {});

    [[nodiscard]] std::expected<void, ports::GitOpsError>
    validateRelativeFilePath(std::string_view filePath,
                             std::string_view commandContext);

    [[nodiscard]] std::expected<void, ports::GitOpsError>
    validateRelativeFilePaths(const std::vector<std::string>& filePaths,
                              std::string_view commandContext);

    [[nodiscard]] bool isValidRefName(std::string_view name) noexcept;
} // namespace gf::git::bl
