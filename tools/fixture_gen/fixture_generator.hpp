#pragma once

#include "core/modules/git/ports/git_command_executor.hpp"

#include <chrono>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <string>
#include <vector>

namespace gf::tools
{
    struct FastImportSpec
    {
        std::filesystem::path repoPath;
        std::size_t commitCount = 1000;
        std::uint64_t seed = 42;
        bool checkoutWorkingTree = true;
    };

    struct FastImportResult
    {
        std::filesystem::path repoPath;
        std::size_t commitsCreated = 0;
        std::size_t mergesCreated = 0;
        std::chrono::milliseconds elapsed{0};
    };

    struct FleetSpec
    {
        std::filesystem::path rootDir;
        std::size_t repoCount = 30;
        std::size_t commitsPerRepo = 40;
        std::uint64_t seed = 42;
    };

    struct FleetResult
    {
        std::vector<std::filesystem::path> repoPaths;
        std::size_t cleanCount = 0;
        std::size_t dirtyUnstagedCount = 0;
        std::size_t stagedCount = 0;
        std::chrono::milliseconds elapsed{0};
    };

    /// RAII temporary directory for tests and benchmarks.
    class ScopedTempDir
    {
    public:
        explicit ScopedTempDir(std::string_view prefix = "gf_test_");
        ~ScopedTempDir();

        ScopedTempDir(const ScopedTempDir&) = delete;
        ScopedTempDir& operator=(const ScopedTempDir&) = delete;

        [[nodiscard]] const std::filesystem::path& path() const noexcept { return path_; }

    private:
        std::filesystem::path path_;
    };

    class FixtureGenerator
    {
    public:
        explicit FixtureGenerator(std::filesystem::path gitExecutablePath);

        /// Generates in seconds a deterministic synthetic repository (even 50,000+ commits)
    /// with parallel branches, merge commits, and UTF-8 paths using `git fast-import`.
        [[nodiscard]] std::expected<FastImportResult, std::string>
        generateFastImportRepo(const FastImportSpec& spec) const;

        /// Generates a fleet of N repositories (e.g. 30 or 100) with a realistic mix of states
    /// (clean, unstaged/untracked, staged) by cloning from a pre-built local template.
        [[nodiscard]] std::expected<FleetResult, std::string>
        generateRepoFleet(const FleetSpec& spec) const;

    private:
        [[nodiscard]] std::expected<void, std::string>
        runFastImportFromFile(const std::filesystem::path& repoPath,
                              const std::filesystem::path& streamFilePath) const;

        std::filesystem::path gitExe_;
    };
} // namespace gf::tools
