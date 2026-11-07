#pragma once

#include <optional>
#include <string>

namespace gf::git::bo {

struct CommitAuthor {
    std::string name;
    std::string email;

    [[nodiscard]] bool isValid() const noexcept {
        return !name.empty() && !email.empty();
    }
};

struct CommitOptions {
    std::string message;
    std::optional<CommitAuthor> author = std::nullopt;
    bool amend = false;
    bool allowEmpty = false;
};

struct CommitResult {
    std::string commitSha;
    std::string shortSha;
    std::string summary;
    std::string branch;
};

} // namespace gf::git::bo
