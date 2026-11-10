#include "core/modules/git/bl/fs_event_classifier.hpp"

#include <algorithm>
#include <string>

namespace gf::git::bl {

namespace {

bool startsWith(std::string_view str, std::string_view prefix) noexcept {
    return str.size() >= prefix.size() && str.substr(0, prefix.size()) == prefix;
}

bool endsWith(std::string_view str, std::string_view suffix) noexcept {
    return str.size() >= suffix.size() && str.substr(str.size() - suffix.size()) == suffix;
}

std::string normalizePathString(const std::filesystem::path& path) {
    std::string s = path.generic_string();
    while (s.size() > 1 && s.starts_with("./")) {
        s = s.substr(2);
    }
    return s;
}

} // namespace

std::filesystem::path FsEventClassifier::toRelativePath(const std::filesystem::path& repoRoot,
                                                        const std::filesystem::path& targetPath) noexcept {
    if (!targetPath.has_root_path()) {
        return targetPath.lexically_normal();
    }
    if (!repoRoot.empty()) {
        auto lexRel = targetPath.lexically_relative(repoRoot);
        if (!lexRel.empty() && !startsWith(lexRel.generic_string(), "..")) {
            return lexRel.lexically_normal();
        }
    }
    std::error_code ec;
    auto rel = std::filesystem::relative(targetPath, repoRoot, ec);
    if (!ec && !rel.empty()) {
        return rel.lexically_normal();
    }
    return targetPath.lexically_normal();
}

bo::FsChangeClassification FsEventClassifier::classify(const std::filesystem::path& repoRoot,
                                                       const std::filesystem::path& changedPath) noexcept {
    std::filesystem::path relPath = toRelativePath(repoRoot, changedPath);
    std::string norm = normalizePathString(relPath);

    // Check if it is a common transient file to ignore everywhere
    std::string filename = relPath.filename().string();
    if (endsWith(filename, ".swp") || endsWith(filename, ".swo") ||
        endsWith(filename, ".tmp") || endsWith(filename, "~") ||
        startsWith(filename, ".#") || filename == ".DS_Store" ||
        filename == "Thumbs.db" || filename == "4913") {
        return bo::FsChangeClassification::Ignored;
    }

    // Git or editor lockfiles should be ignored
    if (endsWith(filename, ".lock")) {
        return bo::FsChangeClassification::Ignored;
    }

    // If located inside .git
    if (norm == ".git" || startsWith(norm, ".git/")) {
        // Ignore cache, pack, logs, hooks, info and temporary commit messages
        if (norm == ".git/objects" || startsWith(norm, ".git/objects/")) {
            return bo::FsChangeClassification::Ignored;
        }
        if (norm == ".git/logs" || startsWith(norm, ".git/logs/")) {
            return bo::FsChangeClassification::Ignored;
        }
        if (norm == ".git/hooks" || startsWith(norm, ".git/hooks/")) {
            return bo::FsChangeClassification::Ignored;
        }
        if (norm == ".git/info" || startsWith(norm, ".git/info/")) {
            return bo::FsChangeClassification::Ignored;
        }
        if (norm == ".git/COMMIT_EDITMSG" || norm == ".git/TAG_EDITMSG") {
            return bo::FsChangeClassification::Ignored;
        }

        // HEAD
        if (norm == ".git/HEAD") {
            return bo::FsChangeClassification::GitHeadChanged;
        }

        // Refs (branches, tags, remotes) or packed-refs
        if (norm == ".git/refs" || startsWith(norm, ".git/refs/") ||
            norm == ".git/packed-refs" || norm == ".git/FETCH_HEAD" || norm == ".git/ORIG_HEAD") {
            return bo::FsChangeClassification::GitRefsChanged;
        }

        // Index / Staging
        if (norm == ".git/index") {
            return bo::FsChangeClassification::GitIndexChanged;
        }

        // Rebase, Merge, Cherry-Pick, Revert or Bisect
        if (norm == ".git/MERGE_HEAD" || norm == ".git/CHERRY_PICK_HEAD" ||
            norm == ".git/REVERT_HEAD" || norm == ".git/BISECT_LOG" ||
            norm == ".git/rebase-merge" || startsWith(norm, ".git/rebase-merge/") ||
            norm == ".git/rebase-apply" || startsWith(norm, ".git/rebase-apply/")) {
            return bo::FsChangeClassification::GitOperationState;
        }

        // Any other internal file in .git not explicitly relevant
        return bo::FsChangeClassification::Ignored;
    }

    // Working tree file (source code, assets, etc.)
    return bo::FsChangeClassification::WorkingTreeModified;
}

bool FsEventClassifier::isIgnored(const std::filesystem::path& repoRoot,
                                  const std::filesystem::path& changedPath) noexcept {
    return classify(repoRoot, changedPath) == bo::FsChangeClassification::Ignored;
}

} // namespace gf::git::bl
