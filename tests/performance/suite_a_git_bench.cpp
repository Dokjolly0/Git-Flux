#include <catch2/benchmark/catch_benchmark.hpp>
#include <catch2/catch_test_macros.hpp>

#include "core/modules/git/bl/porcelain_status_parser.hpp"
#include "fixture_generator.hpp"
#include "infrastructure/git/git_discovery.hpp"
#include "infrastructure/git/native/native_git_executor.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <format>
#include <iostream>
#include <numeric>
#include <string>
#include <thread>
#include <vector>

namespace
{
    using Clock = std::chrono::steady_clock;

    struct SampleStats
    {
        double meanMs = 0.0;
        double p50Ms = 0.0;
        double p95Ms = 0.0;
        double minMs = 0.0;
        double maxMs = 0.0;
    };

    [[nodiscard]] SampleStats computeStats(std::vector<double> samplesMs)
    {
        if (samplesMs.empty())
        {
            return {};
        }
        std::ranges::sort(samplesMs);
        const double sum = std::accumulate(samplesMs.begin(), samplesMs.end(), 0.0);
        const auto pct = [&](const double p) -> double
        {
            const double idx = p * static_cast<double>(samplesMs.size() - 1);
            const auto lo = static_cast<std::size_t>(std::floor(idx));
            const auto hi = static_cast<std::size_t>(std::ceil(idx));
            const double frac = idx - static_cast<double>(lo);
            return samplesMs[lo] * (1.0 - frac) + samplesMs[hi] * frac;
        };
        return SampleStats{
            .meanMs = sum / static_cast<double>(samplesMs.size()),
            .p50Ms = pct(0.50),
            .p95Ms = pct(0.95),
            .minMs = samplesMs.front(),
            .maxMs = samplesMs.back()
        };
    }
} // namespace

TEST_CASE (
"Suite A - Report Native Git & Fixture 50k"
,
"[report][suite-a]"
)
 {
    const auto gitInfo = gf::infra::git::GitDiscovery::discover();
    REQUIRE(gitInfo.has_value());

    std::cout << "\n======================================================================\n";
    std::cout << " GIT FLUX — FASE 0 / SUITE A BENCHMARK REPORT\n";
    std::cout << "======================================================================\n";
    std::cout << "Git in PATH      : " << gitInfo->discoveredInPath.string() << "\n";
    std::cout << "Git Direct Binary: " << gitInfo->directBinaryPath.string() << "\n";
    std::cout << "Windows Cmd Shim : " << (gitInfo->isWindowsCmdShim ? "SI" : "NO") << "\n";
    std::cout << "Logical CPUs     : " << std::thread::hardware_concurrency() << "\n\n";

    gf::tools::ScopedTempDir tempDir("gf_bench_a_");
    gf::tools::FixtureGenerator generator(gitInfo->directBinaryPath);

    // -------------------------------------------------------------------------
    // 1. A.2: Single Repo Spawn Overhead (Shim vs Direct x Native vs Qt)
    // -------------------------------------------------------------------------
    const auto singleRepoPath = tempDir.path() / "single_repo";
    auto singleCreated = generator.generateFastImportRepo({
        .repoPath = singleRepoPath,
        .commitCount = 300,
        .seed = 42,
        .checkoutWorkingTree = true
    });
    REQUIRE(singleCreated.has_value());

#ifdef _WIN32
    constexpr auto kNativeAdapterName = "Native (CreateProcessW)";
    constexpr auto kGitBinaryName = "git.exe";
#else
    constexpr auto kNativeAdapterName = "Native (posix_spawn)";
    constexpr auto kGitBinaryName = "git";
#endif

    struct ExecutorVariant {
        std::string name;
        std::unique_ptr<gf::git::ports::IGitCommandExecutor> exec;
    };
    std::vector<ExecutorVariant> variants;

    if (gitInfo->isWindowsCmdShim) {
        variants.push_back({
            .name = "Native (CreateProcessW) + cmd\\git.exe (shim)",
            .exec = std::make_unique<gf::infra::git::NativeGitCommandExecutor>(gitInfo->discoveredInPath)
        });
    }
    variants.push_back({
        .name = std::format("{} + direct {}", kNativeAdapterName, kGitBinaryName),
        .exec = std::make_unique<gf::infra::git::NativeGitCommandExecutor>(gitInfo->directBinaryPath)
    });

    const gf::git::bo::GitCommand statusCmd{
        .args = {"status", "--porcelain=v2", "-z", "--branch"},
        .workingDirectory = singleRepoPath,
        .lockPolicy = gf::git::bo::GitLockPolicy::NoOptionalLocks
    };

    std::cout << "### A.2 — Latenza singolo `git status --porcelain=v2 -z` (40 campioni post-warmup)\n\n";
    std::cout << "| Variante | Mean (ms) | p50 (ms) | p95 (ms) | Min (ms) |\n";
    std::cout << "| :--- | ---: | ---: | ---: | ---: |\n";

    for (const auto& [name, exec] : variants) {
        constexpr int kWarmupRuns = 5;
        constexpr int kMeasuredRuns = 40;
        for (int w = 0; w < kWarmupRuns; ++w) {
            auto r = exec->execute(statusCmd);
            REQUIRE((r.has_value() && r->succeeded()));
        }
        std::vector<double> samples;
        samples.reserve(kMeasuredRuns);
        for (int i = 0; i < kMeasuredRuns; ++i) {
            const auto t0 = Clock::now();
            auto r = exec->execute(statusCmd);
            const auto dt = std::chrono::duration<double, std::milli>(Clock::now() - t0).count();
            REQUIRE((r.has_value() && r->succeeded()));
            auto parsed = gf::git::bl::PorcelainStatusParser::parse(r->stdoutBytes);
            REQUIRE(parsed.has_value());
            samples.push_back(dt);
        }
        const auto st = computeStats(std::move(samples));
        std::cout << std::format("| {} | {:.2f} | {:.2f} | {:.2f} | {:.2f} |\n",
                                 name, st.meanMs, st.p50Ms, st.p95Ms, st.minMs);
    }
    std::cout << "\n";

    // -------------------------------------------------------------------------
    // 2. Verify fixture generator throughput with 50,000 commits for Suites C/D
    // -------------------------------------------------------------------------
    const auto repo50kPath = tempDir.path() / "monorepo_50k";
    auto repo50kRes = generator.generateFastImportRepo({
        .repoPath = repo50kPath,
        .commitCount = 50'000,
        .seed = 42,
        .checkoutWorkingTree = true
    });
    REQUIRE(repo50kRes.has_value());
    std::cout << std::format(
        "### Verifica Fixture 50.000 Commit (`git fast-import`)\n"
        "- Generati **{} commit** (di cui **{} merge**) in **{} ms**.\n",
        repo50kRes->commitsCreated, repo50kRes->mergesCreated, repo50kRes->elapsed.count());
    std::cout << "======================================================================\n\n";
}

TEST_CASE (
"Catch2 Microbenchmarks - Parsing e Batch Read"
,
"[!benchmark][micro]"
)
 {
    const auto gitInfo = gf::infra::git::GitDiscovery::discover();
    REQUIRE(gitInfo.has_value());

    gf::tools::ScopedTempDir tempDir("gf_micro_");
    gf::tools::FixtureGenerator generator(gitInfo->directBinaryPath);
    const auto repoPath = tempDir.path() / "repo";
    REQUIRE(generator.generateFastImportRepo({repoPath, 100, 42, true}).has_value());

    gf::infra::git::NativeGitCommandExecutor exec(gitInfo->directBinaryPath);
    const auto rawStatus = exec.execute({
        .args = {"status", "--porcelain=v2", "-z", "--branch"},
        .workingDirectory = repoPath
    });
    REQUIRE(rawStatus.has_value());

    BENCHMARK("PorcelainStatusParser::parse (clean repo header)") {
        return gf::git::bl::PorcelainStatusParser::parse(rawStatus->stdoutBytes);
    };
}
