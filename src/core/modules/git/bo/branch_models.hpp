#pragma once

#include <optional>
#include <string>

namespace gf::git::bo
{
    enum class BranchType
    {
        Local,
        Remote
    };

    struct GitBranch
    {
        std::string name;
        std::string shortName;
        std::string fullName;
        std::string targetCommitSha;
        std::string upstream;
        int ahead = 0;
        int behind = 0;
        bool isCurrent = false;
        bool isDetached = false;
        BranchType type = BranchType::Local;
    };

    struct CreateBranchOptions
    {
        std::string name;
        std::optional<std::string> startPoint = std::nullopt;
        bool checkoutAfterCreate = false;
    };

    struct CheckoutOptions
    {
        std::string branchOrCommit;
        bool createNewBranch = false;
        bool force = false;
    };
} // namespace gf::git::bo
