#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

namespace gf::git::bo {

struct GitStashItem {
    std::size_t index{0};
    std::string selector{};
    std::string commitSha{};
    std::string dateIso{};
    std::string message{};
    std::string branchName{};
};

struct StashPushOptions {
    std::optional<std::string> message{std::nullopt};
    bool includeUntracked{true};
    bool keepIndex{false};
    std::vector<std::string> paths{};
};

struct StashApplyOptions {
    std::size_t stashIndex{0};
    bool restoreIndex{false};
};

} // namespace gf::git::bo
