#include "fixture_generator.hpp"
#include "core/shared/utf8_path.hpp"
#include "infrastructure/git/git_discovery.hpp"

#include <charconv>
#include <iostream>
#include <string_view>

int main(int argc, char* argv[]) {
    auto gitInfo = gf::infra::git::GitDiscovery::discover();
    if (!gitInfo) {
        std::cerr << "Error: " << gitInfo.error().message << "\n";
        return 1;
    }

    if (argc < 3) {
        std::cout << "Usage:\n"
                  << "  gf_fixture_gen monorepo <output_dir> [commit_count=50000]\n"
                  << "  gf_fixture_gen fleet    <output_dir> [repo_count=100] [commits_per_repo=40]\n";
        return 0;
    }

    const std::string_view mode = argv[1];
    const std::filesystem::path targetDir = gf::core::utf8ToPath(argv[2]);
    gf::tools::FixtureGenerator generator(gitInfo->directBinaryPath);

    if (mode == "monorepo") {
        std::size_t commits = 50'000;
        if (argc >= 4) {
            std::string_view argCount = argv[3];
            std::from_chars(argCount.data(), argCount.data() + argCount.size(), commits);
        }
        std::cout << "Generating synthetic repository with " << commits << " commits...\n";
        auto res = generator.generateFastImportRepo({
            .repoPath = targetDir,
            .commitCount = commits,
            .seed = 42,
            .checkoutWorkingTree = true
        });
        if (!res) {
            std::cerr << "Failed: " << res.error() << "\n";
            return 1;
        }
        std::cout << "Completed in " << res->elapsed.count() << " ms ("
                  << res->commitsCreated << " commits, "
                  << res->mergesCreated << " merges).\n";
        return 0;
    }

    if (mode == "fleet") {
        std::size_t repos = 100;
        std::size_t commitsPerRepo = 40;
        if (argc >= 4) {
            std::string_view s = argv[3];
            std::from_chars(s.data(), s.data() + s.size(), repos);
        }
        if (argc >= 5) {
            std::string_view s = argv[4];
            std::from_chars(s.data(), s.data() + s.size(), commitsPerRepo);
        }
        std::cout << "Generating fleet of " << repos << " repositories...\n";
        auto res = generator.generateRepoFleet({
            .rootDir = targetDir,
            .repoCount = repos,
            .commitsPerRepo = commitsPerRepo,
            .seed = 42
        });
        if (!res) {
            std::cerr << "Failed: " << res.error() << "\n";
            return 1;
        }
        std::cout << "Completed in " << res->elapsed.count() << " ms (clean: "
                  << res->cleanCount << ", dirty: " << res->dirtyUnstagedCount
                  << ", staged: " << res->stagedCount << ").\n";
        return 0;
    }

    std::cerr << "Unknown mode: " << mode << "\n";
    return 1;
}
