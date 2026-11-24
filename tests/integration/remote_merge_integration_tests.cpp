#include "core/modules/git/bl/git_remote_merge_service.hpp"
#include "infrastructure/git/git_discovery.hpp"
#include "infrastructure/git/native/native_git_executor.hpp"
#include "infrastructure/git/native/native_repository_git_ops.hpp"

#include <catch2/catch_test_macros.hpp>
#include <chrono>
#include <filesystem>
#include <fstream>

namespace
{
    struct RemoteMergeSandbox
    {
        std::filesystem::path root;
        std::filesystem::path bareRemote;
        std::filesystem::path localRepoA;
        std::filesystem::path localRepoB;

        RemoteMergeSandbox()
        {
            const auto nonce = std::chrono::steady_clock::now().time_since_epoch().count();
            root = std::filesystem::temp_directory_path() / ("gf_remote_integ_" + std::to_string(nonce));
            bareRemote = root / "upstream.git";
            localRepoA = root / "repo_a";
            localRepoB = root / "repo_b";

            std::filesystem::create_directories(bareRemote);
            std::filesystem::create_directories(localRepoA);
            std::filesystem::create_directories(localRepoB);
        }

        ~RemoteMergeSandbox()
        {
            std::error_code ec;
            std::filesystem::remove_all(root, ec);
        }

        static void writeFile(const std::filesystem::path& repoRoot, const std::string& relativePath,
                              const std::string& content)
        {
            const auto filePath = repoRoot / relativePath;
            std::filesystem::create_directories(filePath.parent_path());
            std::ofstream ofs(filePath, std::ios::trunc);
            ofs << content;
        }

        [[nodiscard]] static std::string readFile(const std::filesystem::path& repoRoot, const std::string& relativePath)
        {
            const auto filePath = repoRoot / relativePath;
            std::ifstream ifs(filePath);
            return std::string(std::istreambuf_iterator(ifs),
                               std::istreambuf_iterator<char>());
        }
    };
} // namespace

TEST_CASE (
"NativeRepositoryGitOps and GitRemoteMergeService real Git integration"
,
"[git][remote][merge][integration]"
)
 {
    auto gitDiscovery = gf::infra::git::GitDiscovery::discover();
    REQUIRE(gitDiscovery.has_value());
    const auto gitExe = gitDiscovery->directBinaryPath.empty()
                            ? gitDiscovery->discoveredInPath
                            : gitDiscovery->directBinaryPath;

    gf::infra::git::NativeGitCommandExecutor executor(gitExe);
    gf::infra::git::NativeRepositoryGitOps gitOps(executor);
    gf::git::bl::GitRemoteMergeService service(gitOps);

    RemoteMergeSandbox sandbox;

    // 1. Initialize bare remote
    REQUIRE(executor.execute(gf::git::bo::GitCommand{
        .args = {"init", "--bare", "-b", "main"},
        .workingDirectory = sandbox.bareRemote,
        .lockPolicy = gf::git::bo::GitLockPolicy::Standard
    })->exitCode == 0);

    // 2. Initialize localRepoA
    REQUIRE(executor.execute(gf::git::bo::GitCommand{
        .args = {"init", "-b", "main"},
        .workingDirectory = sandbox.localRepoA,
        .lockPolicy = gf::git::bo::GitLockPolicy::Standard
    })->exitCode == 0);

    REQUIRE(executor.execute(gf::git::bo::GitCommand{
        .args = {"config", "user.name", "Tester A"},
        .workingDirectory = sandbox.localRepoA,
        .lockPolicy = gf::git::bo::GitLockPolicy::Standard
    })->exitCode == 0);

    REQUIRE(executor.execute(gf::git::bo::GitCommand{
        .args = {"config", "user.email", "tester.a@gitflux.local"},
        .workingDirectory = sandbox.localRepoA,
        .lockPolicy = gf::git::bo::GitLockPolicy::Standard
    })->exitCode == 0);

    // Initial commit in repoA
    RemoteMergeSandbox::writeFile(sandbox.localRepoA, "README.md", "# Repo A\n");
    REQUIRE(executor.execute(gf::git::bo::GitCommand{
        .args = {"add", "README.md"},
        .workingDirectory = sandbox.localRepoA,
        .lockPolicy = gf::git::bo::GitLockPolicy::Standard
    })->exitCode == 0);

    REQUIRE(executor.execute(gf::git::bo::GitCommand{
        .args = {"commit", "-m", "Initial commit from repoA"},
        .workingDirectory = sandbox.localRepoA,
        .lockPolicy = gf::git::bo::GitLockPolicy::Standard
    })->exitCode == 0);

    SECTION("1. Gestione dei Remote (add, list, remove)") {
        auto initialRemotes = service.listRemotes(sandbox.localRepoA);
        REQUIRE(initialRemotes.has_value());
        CHECK(initialRemotes->empty());

        // Add remote 'origin'
        auto addRes = service.addRemote(sandbox.localRepoA, "origin", sandbox.bareRemote.string());
        REQUIRE(addRes.has_value());

        auto remotes = service.listRemotes(sandbox.localRepoA);
        REQUIRE(remotes.has_value());
        REQUIRE(remotes->size() == 1);
        CHECK((*remotes)[0].name == "origin");
        CHECK((*remotes)[0].fetchUrl == sandbox.bareRemote.string());
        CHECK((*remotes)[0].pushUrl == sandbox.bareRemote.string());

        // Reject duplicate remote
        auto dupRes = service.addRemote(sandbox.localRepoA, "origin", sandbox.bareRemote.string());
        REQUIRE_FALSE(dupRes.has_value());
        CHECK(dupRes.error().kind == gf::git::ports::GitOpsError::Kind::RemoteAlreadyExists);

        // Add second remote 'backup'
        REQUIRE(service.addRemote(sandbox.localRepoA, "backup", "/tmp/backup.git").has_value());
        auto twoRemotes = service.listRemotes(sandbox.localRepoA);
        REQUIRE(twoRemotes.has_value());
        REQUIRE(twoRemotes->size() == 2);

        // Remove remote 'backup'
        auto removeRes = service.removeRemote(sandbox.localRepoA, "backup");
        REQUIRE(removeRes.has_value());

        auto afterRemove = service.listRemotes(sandbox.localRepoA);
        REQUIRE(afterRemove.has_value());
        REQUIRE(afterRemove->size() == 1);
        CHECK((*afterRemove)[0].name == "origin");

        // Remove non-existent remote
        auto badRemove = service.removeRemote(sandbox.localRepoA, "ghost_remote");
        REQUIRE_FALSE(badRemove.has_value());
        CHECK(badRemove.error().kind == gf::git::ports::GitOpsError::Kind::RemoteNotFound);
    }

    SECTION("2. Operazioni remote: Push, Fetch e Pull sincronizzati tra repo") {
        REQUIRE(service.addRemote(sandbox.localRepoA, "origin", sandbox.bareRemote.string()).has_value());

        // Push from repoA to bareRemote with upstream
        auto pushRes = service.pushRemote(sandbox.localRepoA, gf::git::bo::RemoteOperationOptions{
            .remoteName = "origin",
            .branchName = "main",
            .setUpstream = true
        });
        REQUIRE(pushRes.has_value());
        CHECK(pushRes->success);

        // Clone into repoB
        REQUIRE(executor.execute(gf::git::bo::GitCommand{
            .args = {"clone", sandbox.bareRemote.string(), "."},
            .workingDirectory = sandbox.localRepoB,
            .lockPolicy = gf::git::bo::GitLockPolicy::Standard
        })->exitCode == 0);

        REQUIRE(executor.execute(gf::git::bo::GitCommand{
            .args = {"config", "user.name", "Tester B"},
            .workingDirectory = sandbox.localRepoB,
            .lockPolicy = gf::git::bo::GitLockPolicy::Standard
        })->exitCode == 0);

        REQUIRE(executor.execute(gf::git::bo::GitCommand{
            .args = {"config", "user.email", "tester.b@gitflux.local"},
            .workingDirectory = sandbox.localRepoB,
            .lockPolicy = gf::git::bo::GitLockPolicy::Standard
        })->exitCode == 0);

        CHECK(RemoteMergeSandbox::readFile(sandbox.localRepoB, "README.md") == "# Repo A\n");

        // RepoA creates a second file and pushes
        RemoteMergeSandbox::writeFile(sandbox.localRepoA, "new_feature.txt", "Contenuto 2\n");
        REQUIRE(executor.execute(gf::git::bo::GitCommand{
            .args = {"add", "new_feature.txt"},
            .workingDirectory = sandbox.localRepoA,
            .lockPolicy = gf::git::bo::GitLockPolicy::Standard
        })->exitCode == 0);

        REQUIRE(executor.execute(gf::git::bo::GitCommand{
            .args = {"commit", "-m", "Aggiunto new_feature.txt"},
            .workingDirectory = sandbox.localRepoA,
            .lockPolicy = gf::git::bo::GitLockPolicy::Standard
        })->exitCode == 0);

        REQUIRE(service.pushRemote(sandbox.localRepoA, gf::git::bo::RemoteOperationOptions{
            .remoteName = "origin"
        }).has_value());

        // In repoB fetch and then pull
        auto fetchRes = service.fetchRemote(sandbox.localRepoB, gf::git::bo::RemoteOperationOptions{
            .remoteName = "origin"
        });
        REQUIRE(fetchRes.has_value());
        CHECK(fetchRes->success);

        auto pullRes = service.pullRemote(sandbox.localRepoB, gf::git::bo::RemoteOperationOptions{
            .remoteName = "origin"
        });
        REQUIRE(pullRes.has_value());
        CHECK(pullRes->success);

        CHECK(RemoteMergeSandbox::readFile(sandbox.localRepoB, "new_feature.txt") == "Contenuto 2\n");
    }

    SECTION("2b. Push rifiutato richiede force-with-lease o force incondizionato") {
        REQUIRE(service.addRemote(sandbox.localRepoA, "origin", sandbox.bareRemote.string()).has_value());

        // Initial push from repoA
        REQUIRE(service.pushRemote(sandbox.localRepoA, gf::git::bo::RemoteOperationOptions{
            .remoteName = "origin",
            .branchName = "main",
            .setUpstream = true
        }).has_value());

        // Clone into repoB
        REQUIRE(executor.execute(gf::git::bo::GitCommand{
            .args = {"clone", sandbox.bareRemote.string(), "."},
            .workingDirectory = sandbox.localRepoB,
            .lockPolicy = gf::git::bo::GitLockPolicy::Standard
        })->exitCode == 0);
        REQUIRE(executor.execute(gf::git::bo::GitCommand{
            .args = {"config", "user.name", "Tester B"},
            .workingDirectory = sandbox.localRepoB,
            .lockPolicy = gf::git::bo::GitLockPolicy::Standard
        })->exitCode == 0);
        REQUIRE(executor.execute(gf::git::bo::GitCommand{
            .args = {"config", "user.email", "tester.b@gitflux.local"},
            .workingDirectory = sandbox.localRepoB,
            .lockPolicy = gf::git::bo::GitLockPolicy::Standard
        })->exitCode == 0);

        // Case 1: repoA amends its pushed commit -> normal push fails with non-fast-forward,
        //         then --force-with-lease succeeds.
        RemoteMergeSandbox::writeFile(sandbox.localRepoA, "README.md", "# Repo A (amended)\n");
        REQUIRE(executor.execute(gf::git::bo::GitCommand{
            .args = {"add", "README.md"},
            .workingDirectory = sandbox.localRepoA,
            .lockPolicy = gf::git::bo::GitLockPolicy::Standard
        })->exitCode == 0);
        REQUIRE(executor.execute(gf::git::bo::GitCommand{
            .args = {"commit", "--amend", "-m", "Commit iniziale emendato"},
            .workingDirectory = sandbox.localRepoA,
            .lockPolicy = gf::git::bo::GitLockPolicy::Standard
        })->exitCode == 0);

        auto rejectedNff = service.pushRemote(sandbox.localRepoA, gf::git::bo::RemoteOperationOptions{
            .remoteName = "origin",
            .branchName = "main"
        });
        REQUIRE_FALSE(rejectedNff.has_value());
        CHECK(rejectedNff.error().kind == gf::git::ports::GitOpsError::Kind::RemoteRejected);
        CHECK(rejectedNff.error().message.find("non-fast-forward") != std::string::npos);

        auto leaseOk = service.pushRemote(sandbox.localRepoA, gf::git::bo::RemoteOperationOptions{
            .remoteName = "origin",
            .branchName = "main",
            .force = true
        });
        REQUIRE(leaseOk.has_value());
        CHECK(leaseOk->success);

        // Case 2: repoB hasn't fetched repoA's amended history and makes a local commit.
        //         Normal push -> fetch first; --force-with-lease -> stale info; --force -> succeeds.
        RemoteMergeSandbox::writeFile(sandbox.localRepoB, "from_b.txt", "Da B\n");
        REQUIRE(executor.execute(gf::git::bo::GitCommand{
            .args = {"add", "from_b.txt"},
            .workingDirectory = sandbox.localRepoB,
            .lockPolicy = gf::git::bo::GitLockPolicy::Standard
        })->exitCode == 0);
        REQUIRE(executor.execute(gf::git::bo::GitCommand{
            .args = {"commit", "-m", "Commit da B"},
            .workingDirectory = sandbox.localRepoB,
            .lockPolicy = gf::git::bo::GitLockPolicy::Standard
        })->exitCode == 0);

        auto rejectedFetchFirst = service.pushRemote(sandbox.localRepoB, gf::git::bo::RemoteOperationOptions{
            .remoteName = "origin",
            .branchName = "main"
        });
        REQUIRE_FALSE(rejectedFetchFirst.has_value());
        CHECK(rejectedFetchFirst.error().kind == gf::git::ports::GitOpsError::Kind::RemoteRejected);
        CHECK(rejectedFetchFirst.error().message.find("fetch first") != std::string::npos);

        auto rejectedStale = service.pushRemote(sandbox.localRepoB, gf::git::bo::RemoteOperationOptions{
            .remoteName = "origin",
            .branchName = "main",
            .force = true
        });
        REQUIRE_FALSE(rejectedStale.has_value());
        CHECK(rejectedStale.error().kind == gf::git::ports::GitOpsError::Kind::RemoteRejected);
        CHECK(rejectedStale.error().message.find("stale info") != std::string::npos);

        auto forceOk = service.pushRemote(sandbox.localRepoB, gf::git::bo::RemoteOperationOptions{
            .remoteName = "origin",
            .branchName = "main",
            .force = true,
            .forceUnconditional = true
        });
        REQUIRE(forceOk.has_value());
        CHECK(forceOk->success);
    }

    SECTION("3. Merge Fast-Forward") {
        // Create branch feature/ff
        REQUIRE(executor.execute(gf::git::bo::GitCommand{
            .args = {"checkout", "-b", "feature/ff"},
            .workingDirectory = sandbox.localRepoA,
            .lockPolicy = gf::git::bo::GitLockPolicy::Standard
        })->exitCode == 0);

        RemoteMergeSandbox::writeFile(sandbox.localRepoA, "ff.txt", "Fast forward content\n");
        REQUIRE(executor.execute(gf::git::bo::GitCommand{
            .args = {"add", "ff.txt"},
            .workingDirectory = sandbox.localRepoA,
            .lockPolicy = gf::git::bo::GitLockPolicy::Standard
        })->exitCode == 0);

        REQUIRE(executor.execute(gf::git::bo::GitCommand{
            .args = {"commit", "-m", "feat: fast forward commit"},
            .workingDirectory = sandbox.localRepoA,
            .lockPolicy = gf::git::bo::GitLockPolicy::Standard
        })->exitCode == 0);

        // Switch back to main
        REQUIRE(executor.execute(gf::git::bo::GitCommand{
            .args = {"checkout", "main"},
            .workingDirectory = sandbox.localRepoA,
            .lockPolicy = gf::git::bo::GitLockPolicy::Standard
        })->exitCode == 0);

        // Merge feature/ff
        auto mergeRes = service.mergeBranch(sandbox.localRepoA, gf::git::bo::MergeOptions{
            .sourceBranchOrCommit = "feature/ff"
        });
        REQUIRE(mergeRes.has_value());
        CHECK((mergeRes->status == gf::git::bo::MergeStatus::FastForward ||
               mergeRes->status == gf::git::bo::MergeStatus::Merged));
        CHECK(RemoteMergeSandbox::readFile(sandbox.localRepoA, "ff.txt") == "Fast forward content\n");
    }

    SECTION("4. Merge a 3 vie con no-ff") {
        // Commit on main
        RemoteMergeSandbox::writeFile(sandbox.localRepoA, "main_file.txt", "Main line\n");
        REQUIRE(executor.execute(gf::git::bo::GitCommand{
            .args = {"add", "main_file.txt"},
            .workingDirectory = sandbox.localRepoA,
            .lockPolicy = gf::git::bo::GitLockPolicy::Standard
        })->exitCode == 0);
        REQUIRE(executor.execute(gf::git::bo::GitCommand{
            .args = {"commit", "-m", "Commit on main"},
            .workingDirectory = sandbox.localRepoA,
            .lockPolicy = gf::git::bo::GitLockPolicy::Standard
        })->exitCode == 0);

        // Create branch feature/divergent
        REQUIRE(executor.execute(gf::git::bo::GitCommand{
            .args = {"checkout", "-b", "feature/divergent"},
            .workingDirectory = sandbox.localRepoA,
            .lockPolicy = gf::git::bo::GitLockPolicy::Standard
        })->exitCode == 0);

        RemoteMergeSandbox::writeFile(sandbox.localRepoA, "feat_file.txt", "Feature line\n");
        REQUIRE(executor.execute(gf::git::bo::GitCommand{
            .args = {"add", "feat_file.txt"},
            .workingDirectory = sandbox.localRepoA,
            .lockPolicy = gf::git::bo::GitLockPolicy::Standard
        })->exitCode == 0);
        REQUIRE(executor.execute(gf::git::bo::GitCommand{
            .args = {"commit", "-m", "Commit on feature branch"},
            .workingDirectory = sandbox.localRepoA,
            .lockPolicy = gf::git::bo::GitLockPolicy::Standard
        })->exitCode == 0);

        // Switch back to main
        REQUIRE(executor.execute(gf::git::bo::GitCommand{
            .args = {"checkout", "main"},
            .workingDirectory = sandbox.localRepoA,
            .lockPolicy = gf::git::bo::GitLockPolicy::Standard
        })->exitCode == 0);

        auto mergeRes = service.mergeBranch(sandbox.localRepoA, gf::git::bo::MergeOptions{
            .sourceBranchOrCommit = "feature/divergent",
            .message = "Merge feature/divergent into main",
            .noFastForward = true
        });
        REQUIRE(mergeRes.has_value());
        CHECK(mergeRes->status == gf::git::bo::MergeStatus::Merged);
        CHECK(mergeRes->conflictedFiles.empty());
        CHECK(RemoteMergeSandbox::readFile(sandbox.localRepoA, "feat_file.txt") == "Feature line\n");
    }

    SECTION("5. Conflitto di Merge e Abort") {
        // Base commit with file_conflict.txt
        RemoteMergeSandbox::writeFile(sandbox.localRepoA, "file_conflict.txt", "Line 1 base\n");
        REQUIRE(executor.execute(gf::git::bo::GitCommand{
            .args = {"add", "file_conflict.txt"},
            .workingDirectory = sandbox.localRepoA,
            .lockPolicy = gf::git::bo::GitLockPolicy::Standard
        })->exitCode == 0);
        REQUIRE(executor.execute(gf::git::bo::GitCommand{
            .args = {"commit", "-m", "Base commit for conflict"},
            .workingDirectory = sandbox.localRepoA,
            .lockPolicy = gf::git::bo::GitLockPolicy::Standard
        })->exitCode == 0);

        // Branch feature/conflict
        REQUIRE(executor.execute(gf::git::bo::GitCommand{
            .args = {"checkout", "-b", "feature/conflict"},
            .workingDirectory = sandbox.localRepoA,
            .lockPolicy = gf::git::bo::GitLockPolicy::Standard
        })->exitCode == 0);

        RemoteMergeSandbox::writeFile(sandbox.localRepoA, "file_conflict.txt", "Line 1 from branch feature\n");
        REQUIRE(executor.execute(gf::git::bo::GitCommand{
            .args = {"commit", "-am", "Change in branch"},
            .workingDirectory = sandbox.localRepoA,
            .lockPolicy = gf::git::bo::GitLockPolicy::Standard
        })->exitCode == 0);

        // Switch back to main and modify the same line
        REQUIRE(executor.execute(gf::git::bo::GitCommand{
            .args = {"checkout", "main"},
            .workingDirectory = sandbox.localRepoA,
            .lockPolicy = gf::git::bo::GitLockPolicy::Standard
        })->exitCode == 0);

        RemoteMergeSandbox::writeFile(sandbox.localRepoA, "file_conflict.txt", "Line 1 from main differently\n");
        REQUIRE(executor.execute(gf::git::bo::GitCommand{
            .args = {"commit", "-am", "Change in main"},
            .workingDirectory = sandbox.localRepoA,
            .lockPolicy = gf::git::bo::GitLockPolicy::Standard
        })->exitCode == 0);

        // Merge feature/conflict generates conflict
        auto conflictRes = service.mergeBranch(sandbox.localRepoA, gf::git::bo::MergeOptions{
            .sourceBranchOrCommit = "feature/conflict"
        });
        REQUIRE(conflictRes.has_value());
        CHECK(conflictRes->status == gf::git::bo::MergeStatus::Conflict);
        REQUIRE(conflictRes->conflictedFiles.size() == 1);
        CHECK(conflictRes->conflictedFiles[0] == "file_conflict.txt");

        // File contains conflict markers
        auto conflictedContent = RemoteMergeSandbox::readFile(sandbox.localRepoA, "file_conflict.txt");
        CHECK(conflictedContent.find("<<<<<<<") != std::string::npos);

        // Cancel merge with abortMerge
        auto abortRes = service.abortMerge(sandbox.localRepoA);
        REQUIRE(abortRes.has_value());

        // File is restored to pre-merge state
        auto restoredContent = RemoteMergeSandbox::readFile(sandbox.localRepoA, "file_conflict.txt");
        CHECK(restoredContent == "Line 1 from main differently\n");
    }

    SECTION("NativeRepositoryGitOps::cloneRepository clones bare remote into target directory") {
        REQUIRE(service.addRemote(sandbox.localRepoA, "origin", sandbox.bareRemote.string()).has_value());
        REQUIRE(service.pushRemote(sandbox.localRepoA, gf::git::bo::RemoteOperationOptions{
            .remoteName = "origin",
            .branchName = "main",
            .setUpstream = true
        }).has_value());

        const auto clonedDir = sandbox.root / "cloned_repo";

        gf::git::bo::CloneRepositoryOptions opts{
            .remoteUrl = sandbox.bareRemote.string(),
            .destinationPath = clonedDir
        };

        auto cloneRes = gitOps.cloneRepository(opts);
        REQUIRE(cloneRes.has_value());
        CHECK(cloneRes->repositoryPath == clonedDir);
        CHECK(cloneRes->displayName == "cloned_repo");
        CHECK(std::filesystem::exists(clonedDir / ".git"));
        CHECK(std::filesystem::exists(clonedDir / "README.md"));

        // Cloning into existing non-empty directory returns DestinationAlreadyExists
        auto duplicateRes = gitOps.cloneRepository(opts);
        REQUIRE_FALSE(duplicateRes.has_value());
        CHECK(duplicateRes.error().kind == gf::git::ports::GitOpsError::Kind::DestinationAlreadyExists);
    }
}

#include "core/modules/git/bl/https_credential.hpp"

#ifndef _WIN32
#include <cstdio>
#include <cstdlib>

TEST_CASE (
"applyHttpsCredential feeds the token to git through a one-shot credential helper"
,
"[git][credentials][integration]"
)
 {
    auto gitDiscovery = gf::infra::git::GitDiscovery::discover();
    REQUIRE(gitDiscovery.has_value());
    const auto gitExe = gitDiscovery->directBinaryPath.empty()
                            ? gitDiscovery->discoveredInPath
                            : gitDiscovery->directBinaryPath;

    gf::git::bo::GitCommand cmd{.args = {"credential", "fill"}};
    gf::git::bl::applyHttpsCredential(cmd, {.username = "x-access-token", .password = "ghp_secret 'quoted' $HOME"});

    // The token must not be on the command line, only in the child's environment.
    for (const auto& arg : cmd.args)
    {
        CHECK(arg.find("ghp_secret") == std::string::npos);
    }
    REQUIRE(cmd.args.size() == 6);
    CHECK(cmd.args[0] == "-c");
    CHECK(cmd.args[1] == "credential.helper=");
    CHECK(cmd.args[4] == "credential");

    // Run exactly these args through the real git, with the extra environment, and ask it for
    // the credential of an https URL (stdin is the credential protocol).
    auto shellQuote = [](const std::string& s)
    {
        std::string out = "'";
        for (const char c : s)
        {
            if (c == '\'') out += "'\\''";
            else out += c;
        }
        return out + "'";
    };
    auto credentialFill = [&](const gf::git::bo::GitCommand& command, const std::string& host, int& exitCode)
    {
        std::string line = "printf 'protocol=https\\nhost=" + host + "\\n\\n' | env -u GIT_ASKPASS -u SSH_ASKPASS "
                           "GIT_TERMINAL_PROMPT=0";
        for (const auto& [name, value] : command.extraEnvironment)
        {
            line += " " + name + "=" + shellQuote(value);
        }
        line += " " + shellQuote(gitExe.string());
        for (const auto& arg : command.args)
        {
            line += " " + shellQuote(arg);
        }
        line += " 2>&1";

        std::string output;
        exitCode = -1;
        if (FILE* pipe = ::popen(line.c_str(), "r"))
        {
            char buf[256];
            while (std::fgets(buf, sizeof buf, pipe)) output += buf;
            exitCode = ::pclose(pipe);
        }
        return output;
    };

    int exitCode = -1;
    const auto output = credentialFill(cmd, "github.com", exitCode);
    INFO(output);
    CHECK(exitCode == 0);
    CHECK(output.find("username=x-access-token\n") != std::string::npos);
    CHECK(output.find("password=ghp_secret 'quoted' $HOME\n") != std::string::npos);

    SECTION("a credential bound to a host is only given to that host")
    {
        gf::git::bo::GitCommand bound{.args = {"credential", "fill"}};
        gf::git::bl::applyHttpsCredential(bound, {.username = "alex", .password = "forgejo-token",
                                                  .host = "192.168.1.122"});

        // Same server on a non-default port: git sends "host=192.168.1.122:3000".
        const auto same = credentialFill(bound, "192.168.1.122:3000", exitCode);
        INFO(same);
        CHECK(exitCode == 0);
        CHECK(same.find("password=forgejo-token\n") != std::string::npos);

        // Another remote (fetch --all) or a redirect target: the helper stays silent and git,
        // with prompts disabled, fails instead of leaking the token.
        const auto other = credentialFill(bound, "evil.example.com", exitCode);
        INFO(other);
        CHECK(exitCode != 0);
        CHECK(other.find("forgejo-token") == std::string::npos);
    }

    SECTION("a loader is only applied when it returns a token")
    {
        gf::git::bo::GitCommand untouched{.args = {"fetch", "origin"}};
        gf::git::bl::applyHttpsCredential(untouched, gf::git::bo::HttpsCredentialLoader{});
        gf::git::bl::applyHttpsCredential(untouched, gf::git::bo::HttpsCredentialLoader{
                                              []() -> std::optional<gf::git::bo::HttpsCredential> { return std::nullopt; }
                                          });
        CHECK(untouched.args.size() == 2);
        CHECK(untouched.extraEnvironment.empty());

        gf::git::bl::applyHttpsCredential(untouched, gf::git::bo::HttpsCredentialLoader{
                                              []() -> std::optional<gf::git::bo::HttpsCredential>
                                              {
                                                  return gf::git::bo::HttpsCredential{.username = "u", .password = "t"};
                                              }
                                          });
        CHECK(untouched.args.size() == 6);
        CHECK(untouched.args[4] == "fetch");
    }
}
#endif
