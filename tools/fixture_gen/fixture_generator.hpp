#pragma once

#include "core/modules/git/ports/git_command_executor.hpp"

#include <chrono>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <string>
#include <vector>

namespace gf::tools {

struct FastImportSpec {
    std::filesystem::path repoPath;
    std::size_t commitCount = 1000;
    std::uint64_t seed = 42;
    bool checkoutWorkingTree = true;
};

struct FastImportResult {
    std::filesystem::path repoPath;
    std::size_t commitsCreated = 0;
    std::size_t mergesCreated = 0;
    std::chrono::milliseconds elapsed{0};
};

struct FleetSpec {
    std::filesystem::path rootDir;
    std::size_t repoCount = 30;
    std::size_t commitsPerRepo = 40;
    std::uint64_t seed = 42;
};

struct FleetResult {
    std::vector<std::filesystem::path> repoPaths;
    std::size_t cleanCount = 0;
    std::size_t dirtyUnstagedCount = 0;
    std::size_t stagedCount = 0;
    std::chrono::milliseconds elapsed{0};
};

/// Directory temporanea RAII per test e benchmark.
class ScopedTempDir {
public:
    explicit ScopedTempDir(std::string_view prefix = "gf_test_");
    ~ScopedTempDir();

    ScopedTempDir(const ScopedTempDir&) = delete;
    ScopedTempDir& operator=(const ScopedTempDir&) = delete;

    [[nodiscard]] const std::filesystem::path& path() const noexcept { return path_; }

private:
    std::filesystem::path path_;
};

class FixtureGenerator {
public:
    explicit FixtureGenerator(std::filesystem::path gitExecutablePath);

    /// Genera in pochi secondi un repository sintetico deterministico (anche 50.000+ commit)
    /// con branch paralleli, merge commit e path UTF-8 tramite `git fast-import`.
    [[nodiscard]] std::expected<FastImportResult, std::string>
    generateFastImportRepo(const FastImportSpec& spec) const;

    /// Genera una flotta di N repository (es. 30 o 100) con mix realistico di stati
    /// (clean, unstaged/untracked, staged) clonando da un template locale pre-costruito.
    [[nodiscard]] std::expected<FleetResult, std::string>
    generateRepoFleet(const FleetSpec& spec) const;

private:
    [[nodiscard]] std::expected<void, std::string>
    runFastImportFromFile(const std::filesystem::path& repoPath,
                          const std::filesystem::path& streamFilePath) const;

    std::filesystem::path gitExe_;
};

} // namespace gf::tools
