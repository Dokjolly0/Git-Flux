#include "core/modules/git/bl/git_branch_service.hpp"
#include "infrastructure/git/git_discovery.hpp"
#include "infrastructure/git/native/native_git_executor.hpp"
#include "infrastructure/git/native/native_repository_git_ops.hpp"

#include <catch2/catch_test_macros.hpp>
#include <chrono>
#include <filesystem>
#include <fstream>

namespace
{
    struct BranchTestSandbox
    {
        std::filesystem::path root;

        BranchTestSandbox()
        {
            const auto nonce = std::chrono::steady_clock::now().time_since_epoch().count();
            root = std::filesystem::temp_directory_path() / ("gf_branch_integ_" + std::to_string(nonce));
            std::filesystem::create_directories(root);
        }

        ~BranchTestSandbox()
        {
            std::error_code ec;
            std::filesystem::remove_all(root, ec);
        }

        void writeFile(const std::string& relativePath, const std::string& content)
        {
            auto filePath = root / relativePath;
            std::filesystem::create_directories(filePath.parent_path());
            std::ofstream ofs(filePath, std::ios::trunc);
            ofs << content;
        }

        std::string readFile(const std::string& relativePath)
        {
            auto filePath = root / relativePath;
            std::ifstream ifs(filePath);
            return std::string((std::istreambuf_iterator<char>(ifs)),
                               std::istreambuf_iterator<char>());
        }
    };
} // namespace

TEST_CASE (
"NativeRepositoryGitOps and GitBranchService real Git integration"
,
"[git][branch][integration]"
)
 {
    auto gitDiscovery = gf::infra::git::GitDiscovery::discover();
    REQUIRE(gitDiscovery.has_value());
    const auto gitExe = gitDiscovery->directBinaryPath.empty()
                            ? gitDiscovery->discoveredInPath
                            : gitDiscovery->directBinaryPath;

    gf::infra::git::NativeGitCommandExecutor executor(gitExe);
    gf::infra::git::NativeRepositoryGitOps gitOps(executor);
    gf::git::bl::GitBranchService branchService(gitOps);

    BranchTestSandbox sandbox;

    // Initialize Git repository with initial branch 'main'
    gf::git::bo::GitCommand initCmd{
        .args = {"init", "-b", "main"},
        .workingDirectory = sandbox.root,
        .lockPolicy = gf::git::bo::GitLockPolicy::Standard
    };
    auto initRes = executor.execute(initCmd);
    REQUIRE(initRes.has_value());
    REQUIRE(initRes->exitCode == 0);

    // Configure local test author identity
    gf::git::bo::GitCommand cfgNameCmd{
        .args = {"config", "user.name", "GitFlux TestBot"},
        .workingDirectory = sandbox.root,
        .lockPolicy = gf::git::bo::GitLockPolicy::Standard
    };
    REQUIRE(executor.execute(cfgNameCmd)->exitCode == 0);

    gf::git::bo::GitCommand cfgMailCmd{
        .args = {"config", "user.email", "bot@gitflux.local"},
        .workingDirectory = sandbox.root,
        .lockPolicy = gf::git::bo::GitLockPolicy::Standard
    };
    REQUIRE(executor.execute(cfgMailCmd)->exitCode == 0);

    // Create initial commit on main
    sandbox.writeFile("README.md", "# Hello GitFlux\n");
    gf::git::bo::GitCommand addCmd{
        .args = {"add", "README.md"},
        .workingDirectory = sandbox.root,
        .lockPolicy = gf::git::bo::GitLockPolicy::Standard
    };
    REQUIRE(executor.execute(addCmd)->exitCode == 0);

    gf::git::bo::GitCommand commitCmd{
        .args = {"commit", "-m", "Initial commit"},
        .workingDirectory = sandbox.root,
        .lockPolicy = gf::git::bo::GitLockPolicy::Standard
    };
    REQUIRE(executor.execute(commitCmd)->exitCode == 0);

    SECTION("1. Elenco branch su repository con solo commit iniziale") {
        auto branchesRes = branchService.listBranches(sandbox.root);
        REQUIRE(branchesRes.has_value());
        REQUIRE(branchesRes->size() == 1);

        const auto& mainBranch = branchesRes->front();
        CHECK(mainBranch.name == "main");
        CHECK(mainBranch.shortName == "main");
        CHECK(mainBranch.fullName == "refs/heads/main");
        CHECK(mainBranch.isCurrent);
        CHECK_FALSE(mainBranch.isDetached);
        CHECK(mainBranch.type == gf::git::bo::BranchType::Local);
        CHECK(mainBranch.targetCommitSha.size() == 40);

        auto currentRes = branchService.getCurrentBranch(sandbox.root);
        REQUIRE(currentRes.has_value());
        REQUIRE(currentRes->has_value());
        CHECK((*currentRes)->name == "main");
    }

    SECTION("2. Creazione nuovo branch senza checkout") {
        auto createRes = branchService.createBranch(sandbox.root, gf::git::bo::CreateBranchOptions{
            .name = "feature/test-1",
            .checkoutAfterCreate = false
        });
        REQUIRE(createRes.has_value());

        auto branches = branchService.listBranches(sandbox.root);
        REQUIRE(branches.has_value());
        REQUIRE(branches->size() == 2);

        // Verify that main remains active
        auto currentRes = branchService.getCurrentBranch(sandbox.root);
        REQUIRE(currentRes.has_value());
        REQUIRE(currentRes->has_value());
        CHECK((*currentRes)->name == "main");

        // Rejection of invalid names
        auto invalidRes = branchService.createBranch(sandbox.root, gf::git::bo::CreateBranchOptions{
            .name = "invalid..branch"
        });
        REQUIRE_FALSE(invalidRes.has_value());
        CHECK(invalidRes.error().kind == gf::git::ports::GitOpsError::Kind::InvalidBranchName);

        // Rejection of duplicate creation
        auto duplicateRes = branchService.createBranch(sandbox.root, gf::git::bo::CreateBranchOptions{
            .name = "feature/test-1"
        });
        REQUIRE_FALSE(duplicateRes.has_value());
        CHECK(duplicateRes.error().kind == gf::git::ports::GitOpsError::Kind::BranchAlreadyExists);
    }

    SECTION("3. Checkout di un branch esistente") {
        REQUIRE(branchService.createBranch(sandbox.root, gf::git::bo::CreateBranchOptions{
            .name = "feature/switch-target"
        }).has_value());

        auto checkoutRes = branchService.checkoutBranch(sandbox.root, gf::git::bo::CheckoutOptions{
            .branchOrCommit = "feature/switch-target"
        });
        REQUIRE(checkoutRes.has_value());

        auto currentRes = branchService.getCurrentBranch(sandbox.root);
        REQUIRE(currentRes.has_value());
        REQUIRE(currentRes->has_value());
        CHECK((*currentRes)->name == "feature/switch-target");

        // Checkout of non-existent branch
        auto badCheckout = branchService.checkoutBranch(sandbox.root, gf::git::bo::CheckoutOptions{
            .branchOrCommit = "nonexistent-branch-xyz"
        });
        REQUIRE_FALSE(badCheckout.has_value());
        CHECK(badCheckout.error().kind == gf::git::ports::GitOpsError::Kind::BranchNotFound);
    }

    SECTION("4. Checkout con flag createNewBranch") {
        auto checkoutNewRes = branchService.checkoutBranch(sandbox.root, gf::git::bo::CheckoutOptions{
            .branchOrCommit = "feature/quick-checkout",
            .createNewBranch = true
        });
        REQUIRE(checkoutNewRes.has_value());

        auto currentRes = branchService.getCurrentBranch(sandbox.root);
        REQUIRE(currentRes.has_value());
        REQUIRE(currentRes->has_value());
        CHECK((*currentRes)->name == "feature/quick-checkout");
    }

    SECTION("5. Ridenominazione di un branch") {
        REQUIRE(branchService.createBranch(sandbox.root, gf::git::bo::CreateBranchOptions{
            .name = "temp/to-rename"
        }).has_value());

        // Rename to invalid name -> error
        auto badRename = branchService.renameBranch(sandbox.root, "temp/to-rename", "bad branch space");
        REQUIRE_FALSE(badRename.has_value());
        CHECK(badRename.error().kind == gf::git::ports::GitOpsError::Kind::InvalidBranchName);

        // Rename to valid name
        auto renameRes = branchService.renameBranch(sandbox.root, "temp/to-rename", "feature/renamed-ok");
        REQUIRE(renameRes.has_value());

        auto branchesRes = branchService.listBranches(sandbox.root);
        REQUIRE(branchesRes.has_value());
        bool foundOld = false;
        bool foundNew = false;
        for (const auto& b : *branchesRes) {
            if (b.name == "temp/to-rename") foundOld = true;
            if (b.name == "feature/renamed-ok") foundNew = true;
        }
        CHECK_FALSE(foundOld);
        CHECK(foundNew);
    }

    SECTION("6. Eliminazione di un branch") {
        REQUIRE(branchService.createBranch(sandbox.root, gf::git::bo::CreateBranchOptions{
            .name = "to-be-deleted"
        }).has_value());

        // Cannot delete current branch
        auto deleteCurrentRes = branchService.deleteBranch(sandbox.root, "main");
        REQUIRE_FALSE(deleteCurrentRes.has_value());
        CHECK(deleteCurrentRes.error().kind == gf::git::ports::GitOpsError::Kind::InvalidOperation);

        // Delete non-current branch
        auto deleteRes = branchService.deleteBranch(sandbox.root, "to-be-deleted");
        REQUIRE(deleteRes.has_value());

        auto branchesRes = branchService.listBranches(sandbox.root);
        REQUIRE(branchesRes.has_value());
        for (const auto& b : *branchesRes) {
            CHECK(b.name != "to-be-deleted");
        }

        // Delete non-existent branch
        auto deleteNonExist = branchService.deleteBranch(sandbox.root, "ghost-branch");
        REQUIRE_FALSE(deleteNonExist.has_value());
        CHECK(deleteNonExist.error().kind == gf::git::ports::GitOpsError::Kind::BranchNotFound);
    }

    SECTION("7. Protezione dirty working tree durante checkout collidente") {
        // Create branch feature/divergent and commit a change to README.md
        REQUIRE(branchService.checkoutBranch(sandbox.root, gf::git::bo::CheckoutOptions{
            .branchOrCommit = "feature/divergent",
            .createNewBranch = true
        }).has_value());

        sandbox.writeFile("README.md", "# Divergent line on feature\n");
        REQUIRE(executor.execute(gf::git::bo::GitCommand{
            .args = {"commit", "-am", "Divergent change"},
            .workingDirectory = sandbox.root,
            .lockPolicy = gf::git::bo::GitLockPolicy::Standard
        })->exitCode == 0);

        // Switch back to main
        REQUIRE(branchService.checkoutBranch(sandbox.root, gf::git::bo::CheckoutOptions{
            .branchOrCommit = "main"
        }).has_value());

        // Locally modify README.md without committing (colliding dirty working tree)
        sandbox.writeFile("README.md", "# Uncommitted dirty change on main\n");

        // Attempt checkout without force -> must fail with DirtyWorkingTree
        auto dirtyCheckout = branchService.checkoutBranch(sandbox.root, gf::git::bo::CheckoutOptions{
            .branchOrCommit = "feature/divergent",
            .force = false
        });
        REQUIRE_FALSE(dirtyCheckout.has_value());
        CHECK(dirtyCheckout.error().kind == gf::git::ports::GitOpsError::Kind::DirtyWorkingTree);

        // Checkout with force -> overwrites and succeeds
        auto forceCheckout = branchService.checkoutBranch(sandbox.root, gf::git::bo::CheckoutOptions{
            .branchOrCommit = "feature/divergent",
            .force = true
        });
        REQUIRE(forceCheckout.has_value());
        CHECK(sandbox.readFile("README.md") == "# Divergent line on feature\n");
    }

    SECTION("8. Filtraggio getLocalBranches vs getRemoteBranches") {
        auto locals = branchService.getLocalBranches(sandbox.root);
        REQUIRE(locals.has_value());
        CHECK(locals->size() >= 1);

        auto remotes = branchService.getRemoteBranches(sandbox.root);
        REQUIRE(remotes.has_value());
        CHECK(remotes->empty());
    }
}
