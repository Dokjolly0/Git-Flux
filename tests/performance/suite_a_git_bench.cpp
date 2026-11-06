#include <catch2/benchmark/catch_benchmark.hpp>
#include <catch2/catch_test_macros.hpp>

#include "core/modules/git/bl/operation_coordinator.hpp"
#include "core/modules/git/bl/porcelain_status_parser.hpp"
#include "core/modules/repositories/bo/repository_id.hpp"
#include "fixture_generator.hpp"
#include "infrastructure/git/git_discovery.hpp"
#include "infrastructure/git/native/native_git_batch_session.hpp"
#include "infrastructure/git/native/native_git_executor.hpp"

#ifdef GF_HAS_QT_ADAPTER
#include "infrastructure/git/qt/qt_git_batch_session.hpp"
#include "infrastructure/git/qt/qt_git_executor.hpp"
#include <QCoreApplication>
#endif

#include <algorithm>
#include <chrono>
#include <cmath>
#include <format>
#include <iostream>
#include <numeric>
#include <string>
#include <vector>

namespace {

using Clock = std::chrono::steady_clock;

#ifdef GF_HAS_QT_ADAPTER
void ensureQtApp() {
    if (!QCoreApplication::instance()) {
        static int argc = 1;
        static char appName[] = "gf_bench_suite_a";
        static char* argv[] = {appName, nullptr};
        static QCoreApplication app(argc, argv);
    }
}
#endif

struct SampleStats {
    double meanMs = 0.0;
    double p50Ms = 0.0;
    double p95Ms = 0.0;
    double minMs = 0.0;
    double maxMs = 0.0;
};

[[nodiscard]] SampleStats computeStats(std::vector<double> samplesMs) {
    if (samplesMs.empty()) {
        return {};
    }
    std::ranges::sort(samplesMs);
    const double sum = std::accumulate(samplesMs.begin(), samplesMs.end(), 0.0);
    const auto pct = [&](double p) -> double {
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

[[nodiscard]] std::vector<std::string> loadCommitOids(
    gf::git::ports::IGitCommandExecutor& exec,
    const std::filesystem::path& repoPath,
    std::size_t count) {
    auto res = exec.execute({
        .args = {"rev-list", std::format("--max-count={}", count), "HEAD"},
        .workingDirectory = repoPath,
        .lockPolicy = gf::git::bo::GitLockPolicy::NoOptionalLocks
    });
    std::vector<std::string> oids;
    if (!res || !res->succeeded()) {
        return oids;
    }
    std::size_t pos = 0;
    while (pos < res->stdoutBytes.size()) {
        const auto nl = res->stdoutBytes.find('\n', pos);
        if (nl == std::string::npos) {
            break;
        }
        std::string s = res->stdoutBytes.substr(pos, nl - pos);
        if (!s.empty() && s.back() == '\r') {
            s.pop_back();
        }
        if (!s.empty()) {
            oids.push_back(std::move(s));
        }
        pos = nl + 1;
    }
    return oids;
}

} // namespace

TEST_CASE("Suite A - Report Comparativo Completo (Win32 vs Qt, Shim vs Direct, Concurrency Sweep, Batch Pipe)", "[report][suite-a]") {
#ifdef GF_HAS_QT_ADAPTER
    ensureQtApp();
#endif

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
constexpr const char* kNativeAdapterName = "Native (CreateProcessW)";
constexpr const char* kNativeSpawnLabel = "CreateProcessW";
constexpr const char* kGitBinaryName = "git.exe";
#else
constexpr const char* kNativeAdapterName = "Native (posix_spawn)";
constexpr const char* kNativeSpawnLabel = "posix_spawn";
constexpr const char* kGitBinaryName = "git";
#endif

    struct ExecutorVariant {
        std::string name;
        std::unique_ptr<gf::git::ports::IGitCommandExecutor> exec;
    };
    std::vector<ExecutorVariant> variants;

    if (gitInfo->isWindowsCmdShim) {
        variants.push_back({
            "Native (CreateProcessW) + cmd\\git.exe (shim)",
            std::make_unique<gf::infra::git::NativeGitCommandExecutor>(gitInfo->discoveredInPath)
        });
    }
    variants.push_back({
        std::format("{} + direct {}", kNativeAdapterName, kGitBinaryName),
        std::make_unique<gf::infra::git::NativeGitCommandExecutor>(gitInfo->directBinaryPath)
    });

#ifdef GF_HAS_QT_ADAPTER
    if (gitInfo->isWindowsCmdShim) {
        variants.push_back({
            "Qt 6 (QProcess)         + cmd\\git.exe (shim)",
            std::make_unique<gf::infra::git::QtGitCommandExecutor>(gitInfo->discoveredInPath)
        });
    }
    variants.push_back({
        std::format("Qt 6 (QProcess)         + direct {}", kGitBinaryName),
        std::make_unique<gf::infra::git::QtGitCommandExecutor>(gitInfo->directBinaryPath)
    });
#endif

    const gf::git::bo::GitCommand statusCmd{
        .args = {"status", "--porcelain=v2", "-z", "--branch"},
        .workingDirectory = singleRepoPath,
        .lockPolicy = gf::git::bo::GitLockPolicy::NoOptionalLocks
    };

    std::cout << "### A.2 — Latenza singolo `git status --porcelain=v2 -z` (40 campioni post-warmup)\n\n";
    std::cout << "| Variante | Mean (ms) | p50 (ms) | p95 (ms) | Min (ms) |\n";
    std::cout << "| :--- | ---: | ---: | ---: | ---: |\n";

    constexpr int kWarmupRuns = 5;
    constexpr int kMeasuredRuns = 40;

    for (auto& v : variants) {
        for (int w = 0; w < kWarmupRuns; ++w) {
            auto r = v.exec->execute(statusCmd);
            REQUIRE((r.has_value() && r->succeeded()));
        }
        std::vector<double> samples;
        samples.reserve(kMeasuredRuns);
        for (int i = 0; i < kMeasuredRuns; ++i) {
            const auto t0 = Clock::now();
            auto r = v.exec->execute(statusCmd);
            const auto dt = std::chrono::duration<double, std::milli>(Clock::now() - t0).count();
            REQUIRE((r.has_value() && r->succeeded()));
            auto parsed = gf::git::bl::PorcelainStatusParser::parse(r->stdoutBytes);
            REQUIRE(parsed.has_value());
            samples.push_back(dt);
        }
        const auto st = computeStats(std::move(samples));
        std::cout << std::format("| {} | {:.2f} | {:.2f} | {:.2f} | {:.2f} |\n",
                                 v.name, st.meanMs, st.p50Ms, st.p95Ms, st.minMs);
    }
    std::cout << "\n";

    // -------------------------------------------------------------------------
    // 2. A.3: Concurrency Sweep su flotta di 30 Repository + Active-Repo Priority
    // -------------------------------------------------------------------------
    const auto fleetDir = tempDir.path() / "fleet_30";
    auto fleetRes = generator.generateRepoFleet({
        .rootDir = fleetDir,
        .repoCount = 30,
        .commitsPerRepo = 40,
        .seed = 42
    });
    REQUIRE(fleetRes.has_value());

    std::cout << std::format(
        "### A.3 — Refresh stato su 30 Repository (generati in {} ms) — Concurrency Sweep\n\n",
        fleetRes->elapsed.count());
    std::cout << std::format("| Adapter (direct {}) | Concurrency | Wall-Clock 30 Repo (ms) | Active Repo Latency (ms) |\n",
                             kGitBinaryName);
    std::cout << "| :--- | ---: | ---: | ---: |\n";

    const std::size_t concurrencies[] = {1, 2, 4, 8, 12, 16};

    auto runFleetSweep = [&](std::string_view adapterTitle, auto makeExecutorFn) {
        for (const std::size_t conc : concurrencies) {
            constexpr int kRepeats = 3;
            std::vector<double> totalSamples;
            std::vector<double> activeRepoSamples;

            for (int rep = 0; rep < kRepeats; ++rep) {
                gf::git::bl::OperationCoordinator coordinator(conc);
                auto sharedExec = makeExecutorFn();

                const auto tStart = Clock::now();
                std::vector<std::future<void>> bgFutures;
                bgFutures.reserve(fleetRes->repoPaths.size());

                // Accodiamo 29 repo come NormalBackground
                for (std::size_t i = 0; i < fleetRes->repoPaths.size() - 1; ++i) {
                    const auto& rp = fleetRes->repoPaths[i];
                    bgFutures.push_back(coordinator.submit(
                        gf::repositories::bo::RepositoryId(std::format("repo-{}", i)),
                        gf::git::bl::OperationKind::PassiveRead,
                        gf::git::bl::TaskPriority::NormalBackground,
                        [&sharedExec, rp]() {
                            auto r = sharedExec->execute({
                                .args = {"status", "--porcelain=v2", "-z", "--branch"},
                                .workingDirectory = rp,
                                .lockPolicy = gf::git::bo::GitLockPolicy::NoOptionalLocks
                            });
                            if (r && r->succeeded()) {
                                (void)gf::git::bl::PorcelainStatusParser::parse(r->stdoutBytes);
                            }
                        }));
                }

                // L'ultimo repo (indice 29) viene sottomesso per ULTIMO ma con priorità HighActiveRepo!
                const auto& activePath = fleetRes->repoPaths.back();
                auto activeFuture = coordinator.submit(
                    gf::repositories::bo::RepositoryId("repo-active"),
                    gf::git::bl::OperationKind::PassiveRead,
                    gf::git::bl::TaskPriority::HighActiveRepo,
                    [&sharedExec, activePath, tStart]() -> double {
                        auto r = sharedExec->execute({
                            .args = {"status", "--porcelain=v2", "-z", "--branch"},
                            .workingDirectory = activePath,
                            .lockPolicy = gf::git::bo::GitLockPolicy::NoOptionalLocks
                        });
                        if (r && r->succeeded()) {
                            (void)gf::git::bl::PorcelainStatusParser::parse(r->stdoutBytes);
                        }
                        return std::chrono::duration<double, std::milli>(Clock::now() - tStart).count();
                    });

                const double activeMs = activeFuture.get();
                for (auto& f : bgFutures) {
                    f.get();
                }
                const double totalMs = std::chrono::duration<double, std::milli>(Clock::now() - tStart).count();

                totalSamples.push_back(totalMs);
                activeRepoSamples.push_back(activeMs);
            }

            const auto totSt = computeStats(std::move(totalSamples));
            const auto actSt = computeStats(std::move(activeRepoSamples));
            std::cout << std::format("| {} | {} | {:.1f} | {:.1f} |\n",
                                     adapterTitle, conc, totSt.meanMs, actSt.meanMs);
        }
    };

    runFleetSweep(kNativeAdapterName, [&]() {
        return std::make_unique<gf::infra::git::NativeGitCommandExecutor>(gitInfo->directBinaryPath);
    });

#ifdef GF_HAS_QT_ADAPTER
    runFleetSweep("Qt 6 (QProcess)", [&]() {
        return std::make_unique<gf::infra::git::QtGitCommandExecutor>(gitInfo->directBinaryPath);
    });
#endif
    std::cout << "\n";

    // -------------------------------------------------------------------------
    // 3. A.4: Lettura di 150 commit: Single-Shot Process vs Persistent Pipe `cat-file --batch`
    // -------------------------------------------------------------------------
    gf::infra::git::NativeGitCommandExecutor oidLoader(gitInfo->directBinaryPath);
    const auto oids = loadCommitOids(oidLoader, singleRepoPath, 150);
    REQUIRE(oids.size() == 150);

    std::cout << "### A.4 — Lettura metadati di 150 commit: 150 processi singoli vs 1 pipe `git cat-file --batch`\n\n";
    std::cout << "| Strategia | Tempo Totale 150 commit (ms) | Media per commit (ms) | Speedup |\n";
    std::cout << "| :--- | ---: | ---: | ---: |\n";

    // Misura Single-Shot Native
    double nativeSingleShotMs = 0.0;
    {
        const auto t0 = Clock::now();
        for (const auto& oid : oids) {
            auto r = oidLoader.execute({
                .args = {"cat-file", "commit", oid},
                .workingDirectory = singleRepoPath,
                .lockPolicy = gf::git::bo::GitLockPolicy::NoOptionalLocks
            });
            REQUIRE((r.has_value() && r->succeeded()));
        }
        nativeSingleShotMs = std::chrono::duration<double, std::milli>(Clock::now() - t0).count();
        std::cout << std::format("| Native Single-Shot (150x {}) | {:.1f} | {:.3f} | 1.0x (baseline) |\n",
                                 kNativeSpawnLabel, nativeSingleShotMs, nativeSingleShotMs / 150.0);
    }

#ifdef GF_HAS_QT_ADAPTER
    double qtSingleShotMs = 0.0;
    {
        gf::infra::git::QtGitCommandExecutor qtExec(gitInfo->directBinaryPath);
        const auto t0 = Clock::now();
        for (const auto& oid : oids) {
            auto r = qtExec.execute({
                .args = {"cat-file", "commit", oid},
                .workingDirectory = singleRepoPath,
                .lockPolicy = gf::git::bo::GitLockPolicy::NoOptionalLocks
            });
            REQUIRE((r.has_value() && r->succeeded()));
        }
        qtSingleShotMs = std::chrono::duration<double, std::milli>(Clock::now() - t0).count();
        std::cout << std::format("| Qt 6 Single-Shot (150x QProcess) | {:.1f} | {:.3f} | {:.2f}x |\n",
                                 qtSingleShotMs, qtSingleShotMs / 150.0, nativeSingleShotMs / qtSingleShotMs);
    }
#endif

    // Misura Persistent Batch Session Native (include costo di apertura pipe + 150 query!)
    {
        gf::infra::git::NativeGitBatchSessionFactory batchFactory(gitInfo->directBinaryPath);
        const auto t0 = Clock::now();
        auto session = batchFactory.openSession(singleRepoPath);
        REQUIRE(session.has_value());
        for (const auto& oid : oids) {
            auto obj = (*session)->readObject(oid);
            REQUIRE(obj.has_value());
        }
        const double batchIncludeOpenMs = std::chrono::duration<double, std::milli>(Clock::now() - t0).count();

        // Misura su sessione già calda (warm pipe)
        const auto tWarm = Clock::now();
        for (const auto& oid : oids) {
            auto obj = (*session)->readObject(oid);
            REQUIRE(obj.has_value());
        }
        const double batchWarmMs = std::chrono::duration<double, std::milli>(Clock::now() - tWarm).count();

        std::cout << std::format("| Native Batch Pipe (open + 150 query) | {:.2f} | {:.4f} | **{:.1f}x** |\n",
                                 batchIncludeOpenMs, batchIncludeOpenMs / 150.0, nativeSingleShotMs / batchIncludeOpenMs);
        std::cout << std::format("| Native Batch Pipe (warm session, 150 query) | {:.2f} | {:.4f} | **{:.1f}x** |\n",
                                 batchWarmMs, batchWarmMs / 150.0, nativeSingleShotMs / std::max(0.01, batchWarmMs));
    }

#ifdef GF_HAS_QT_ADAPTER
    {
        gf::infra::git::QtGitBatchSessionFactory qtBatchFactory(gitInfo->directBinaryPath);
        const auto t0 = Clock::now();
        auto session = qtBatchFactory.openSession(singleRepoPath);
        REQUIRE(session.has_value());
        for (const auto& oid : oids) {
            auto obj = (*session)->readObject(oid);
            REQUIRE(obj.has_value());
        }
        const double batchIncludeOpenMs = std::chrono::duration<double, std::milli>(Clock::now() - t0).count();

        const auto tWarm = Clock::now();
        for (const auto& oid : oids) {
            auto obj = (*session)->readObject(oid);
            REQUIRE(obj.has_value());
        }
        const double batchWarmMs = std::chrono::duration<double, std::milli>(Clock::now() - tWarm).count();

        std::cout << std::format("| Qt 6 Batch Pipe (open + 150 query) | {:.2f} | {:.4f} | **{:.1f}x** |\n",
                                 batchIncludeOpenMs, batchIncludeOpenMs / 150.0, nativeSingleShotMs / batchIncludeOpenMs);
        std::cout << std::format("| Qt 6 Batch Pipe (warm session, 150 query) | {:.2f} | {:.4f} | **{:.1f}x** |\n",
                                 batchWarmMs, batchWarmMs / 150.0, nativeSingleShotMs / std::max(0.01, batchWarmMs));
    }
#endif
    std::cout << "\n";

    // -------------------------------------------------------------------------
    // 4. Verifica velocità generatore fixture da 50.000 commit per le Suite C/D
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

TEST_CASE("Catch2 Microbenchmarks - Parsing e Batch Read", "[!benchmark][micro]") {
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

    gf::infra::git::NativeGitBatchSessionFactory factory(gitInfo->directBinaryPath);
    auto session = factory.openSession(repoPath);
    REQUIRE(session.has_value());

    BENCHMARK("NativeGitBatchSession::readObject(\"HEAD\") via warm pipe") {
        return (*session)->readObject("HEAD");
    };
}
