#pragma once

#include "core/modules/git/bo/fs_watcher_models.hpp"

#include <filesystem>
#include <string_view>

namespace gf::git::bl {

class FsEventClassifier {
public:
    [[nodiscard]] static bo::FsChangeClassification
    classify(const std::filesystem::path& repoRoot,
             const std::filesystem::path& changedPath) noexcept;

    [[nodiscard]] static bool
    isIgnored(const std::filesystem::path& repoRoot,
              const std::filesystem::path& changedPath) noexcept;

    [[nodiscard]] static std::filesystem::path
    toRelativePath(const std::filesystem::path& repoRoot,
                   const std::filesystem::path& targetPath) noexcept;
};

} // namespace gf::git::bl
