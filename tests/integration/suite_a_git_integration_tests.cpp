#include <catch2/catch_test_macros.hpp>

#include "core/modules/git/bl/porcelain_status_parser.hpp"
#include "core/shared/utf8_path.hpp"
#include "fixture_generator.hpp"
#include "infrastructure/git/git_discovery.hpp"
#include "infrastructure/git/native/native_git_batch_session.hpp"
#include "infrastructure/git/native/native_git_executor.hpp"

#ifdef GF_HAS_QT_ADAPTER
#include "infrastructure/git/qt/qt_git_batch_session.hpp"
#include "infrastructure/git/qt/qt_git_executor.hpp"
#include <QCoreApplication>
#endif

#include <atomic>
#include <fstream>
#include <string>
#include <thread>
#include <vector>

namespace {

#ifdef GF_HAS_QT_ADAPTER
void ensureQtCoreApp() {
    if (!QCoreApplication::instance()) {
        static int argc = 1;
        static char appName[] = "gf_integration_tests";
        static char* argv[] = {appName, nullptr};
        static QCoreApplication app(argc, argv);
    }
}
#endif

} // namespace

TEST_CASE("Suite A.0 - GitDiscovery rileva eseguibile Git e risolve lo shim su Windows", "[integration][suite-a]") {
    const auto discovered = gf::infra::git::GitDiscovery::discover();
    REQUIRE(discovered.has_value());
    REQUIRE(std::filesystem::exists(discovered->discoveredInPath));
    REQUIRE(std::filesystem::exists(discovered->directBinaryPath));

    gf::infra::git::NativeGitCommandExecutor exec(discovered->directBinaryPath);
    const auto ver = exec.execute({
        .args = {"--version"},
        .lockPolicy = gf::git::bo::GitLockPolicy::Standard
    });
    REQUIRE(ver.has_value());
    REQUIRE(ver->succeeded());
    REQUIRE(ver->stdoutBytes.starts_with("git version"));
}

TEST_CASE("Suite A.1 - NoOptionalLocks evita conflitti su .git/index.lock sia statici sia sotto stress concorrente", "[integration][suite-a][locks]") {
    const auto gitInfo = gf::infra::git::GitDiscovery::discover();
    REQUIRE(gitInfo.has_value());
    gf::infra::git::NativeGitCommandExecutor executor(gitInfo->directBinaryPath);

    SECTION("Deterministico: git status con NoOptionalLocks riesce anche se .git/index.lock esiste ed e' bloccato") {
        gf::tools::ScopedTempDir temp("gf_lock_det_");
        const auto repoPath = temp.path() / "repo";

        gf::tools::FixtureGenerator gen(gitInfo->directBinaryPath);
        auto created = gen.generateFastImportRepo({
            .repoPath = repoPath,
            .commitCount = 25,
            .seed = 101,
            .checkoutWorkingTree = true
        });
        if (!created) {
            UNSCOPED_INFO("generateFastImportRepo error: " << created.error());
        }
        REQUIRE(created.has_value());

        // Sporca un file tracciato per invalidare la stat-cache dell'index
        {
            std::ofstream f(repoPath / "src/module_1/item.cpp", std::ios::app);
            f << "// touch while index.lock is held\n";
        }

        // Simula un processo esterno (es. IDE / git add) che detiene .git/index.lock
        const auto indexLockPath = repoPath / ".git" / "index.lock";
        {
            std::ofstream lockFile(indexLockPath, std::ios::binary);
            lockFile << "locked";
        }
        REQUIRE(std::filesystem::exists(indexLockPath));

        const auto statusRes = executor.execute({
            .args = {"status", "--porcelain=v2", "-z", "--branch"},
            .workingDirectory = repoPath,
            .lockPolicy = gf::git::bo::GitLockPolicy::NoOptionalLocks
        });

        REQUIRE(statusRes.has_value());
        REQUIRE(statusRes->succeeded());
        // Verifica che il lock file esterno NON sia stato rimosso o alterato
        REQUIRE(std::filesystem::exists(indexLockPath));

        const auto parsed = gf::git::bl::PorcelainStatusParser::parse(statusRes->stdoutBytes);
        REQUIRE(parsed.has_value());
        REQUIRE_FALSE(parsed->isClean());

        std::filesystem::remove(indexLockPath);
    }

    SECTION("Stress concorrente: 2 thread di polling continuo + 20 cicli git add/commit senza 1 solo errore di lock") {
        gf::tools::ScopedTempDir temp("gf_lock_stress_");
        const auto repoPath = temp.path() / "repo";

        gf::tools::FixtureGenerator gen(gitInfo->directBinaryPath);
        auto created = gen.generateFastImportRepo({
            .repoPath = repoPath,
            .commitCount = 25,
            .seed = 202,
            .checkoutWorkingTree = true
        });
        if (!created) {
            UNSCOPED_INFO("generateFastImportRepo error: " << created.error());
        }
        REQUIRE(created.has_value());

        std::atomic<bool> stopPolling{false};
        std::atomic<int> pollSuccesses{0};
        std::atomic<int> pollFailures{0};

        auto pollerFn = [&]() {
            gf::infra::git::NativeGitCommandExecutor pollExec(gitInfo->directBinaryPath);
            while (!stopPolling.load(std::memory_order_acquire)) {
                auto r = pollExec.execute({
                    .args = {"status", "--porcelain=v2", "-z", "--branch"},
                    .workingDirectory = repoPath,
                    .lockPolicy = gf::git::bo::GitLockPolicy::NoOptionalLocks
                });
                if (r && r->succeeded()) {
                    pollSuccesses.fetch_add(1, std::memory_order_relaxed);
                } else {
                    pollFailures.fetch_add(1, std::memory_order_relaxed);
                }
            }
        };

        std::jthread p1(pollerFn);
        std::jthread p2(pollerFn);

        int commitFailures = 0;
        for (int i = 0; i < 20; ++i) {
            {
                std::ofstream f(repoPath / "src/module_2/item.cpp", std::ios::app);
                f << "// concurrent iteration " << i << "\n";
            }
            auto addRes = executor.execute({
                .args = {"add", "src/module_2/item.cpp"},
                .workingDirectory = repoPath,
                .lockPolicy = gf::git::bo::GitLockPolicy::Standard
            });
            auto commitRes = executor.execute({
                .args = {"commit", "-q", "-m", "Concurrent commit " + std::to_string(i)},
                .workingDirectory = repoPath,
                .lockPolicy = gf::git::bo::GitLockPolicy::Standard
            });
            if (!addRes || !addRes->succeeded() || !commitRes || !commitRes->succeeded()) {
                ++commitFailures;
            }
        }

        stopPolling.store(true, std::memory_order_release);
        p1.join();
        p2.join();

        REQUIRE(commitFailures == 0);
        REQUIRE(pollFailures.load() == 0);
        REQUIRE(pollSuccesses.load() > 0);
    }
}

TEST_CASE("Suite A.2 - Parita' funzionale Native vs Qt QProcess e sessione persistente cat-file --batch", "[integration][suite-a][batch]") {
    const auto gitInfo = gf::infra::git::GitDiscovery::discover();
    REQUIRE(gitInfo.has_value());

    gf::tools::ScopedTempDir temp("gf_parity_test_");
    const auto repoPath = temp.path() / "repo";

    gf::tools::FixtureGenerator gen(gitInfo->directBinaryPath);
    auto created = gen.generateFastImportRepo({
        .repoPath = repoPath,
        .commitCount = 60,
        .seed = 77,
        .checkoutWorkingTree = true
    });
    if (!created) {
        UNSCOPED_INFO("generateFastImportRepo error: " << created.error());
    }
    REQUIRE(created.has_value());

    // Aggiungiamo una modifica su un file con nome UTF-8
    {
        std::ofstream unicodeFile(repoPath / gf::core::utf8ToPath("docs/specifica_città.md"), std::ios::app);
        unicodeFile << "Nuova riga UTF-8!\n";
    }

    gf::infra::git::NativeGitCommandExecutor nativeExec(gitInfo->directBinaryPath);
    const gf::git::bo::GitCommand statusCmd{
        .args = {"status", "--porcelain=v2", "-z", "--branch"},
        .workingDirectory = repoPath,
        .lockPolicy = gf::git::bo::GitLockPolicy::NoOptionalLocks
    };

    const auto nativeStatus = nativeExec.execute(statusCmd);
    REQUIRE(nativeStatus.has_value());
    REQUIRE(nativeStatus->succeeded());

    // Estraiamo gli ultimi 30 OID commit per testare cat-file --batch
    const auto revListRes = nativeExec.execute({
        .args = {"rev-list", "--max-count=30", "HEAD"},
        .workingDirectory = repoPath,
        .lockPolicy = gf::git::bo::GitLockPolicy::NoOptionalLocks
    });
    REQUIRE(revListRes.has_value());
    REQUIRE(revListRes->succeeded());

    std::vector<std::string> oids;
    std::size_t pos = 0;
    while (pos < revListRes->stdoutBytes.size()) {
        const auto nl = revListRes->stdoutBytes.find('\n', pos);
        if (nl == std::string::npos) {
            break;
        }
        std::string line = revListRes->stdoutBytes.substr(pos, nl - pos);
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        if (!line.empty()) {
            oids.push_back(std::move(line));
        }
        pos = nl + 1;
    }
    REQUIRE(oids.size() == 30);

    gf::infra::git::NativeGitBatchSessionFactory nativeBatchFactory(gitInfo->directBinaryPath);
    auto nativeSession = nativeBatchFactory.openSession(repoPath);
    REQUIRE(nativeSession.has_value());
    REQUIRE((*nativeSession)->isAlive());

    std::vector<std::string> nativeContents;
    nativeContents.reserve(oids.size());
    for (const auto& oid : oids) {
        auto obj = (*nativeSession)->readObject(oid);
        REQUIRE(obj.has_value());
        REQUIRE(obj->header.oid == oid);
        REQUIRE(obj->header.type == "commit");
        REQUIRE(obj->content.size() == obj->header.size);
        nativeContents.push_back(std::move(obj->content));
    }

    // Verifica gestione oggetto inesistente senza far cadere la pipe!
    auto missingObj = (*nativeSession)->readObject("deadbeefdeadbeefdeadbeefdeadbeefdeadbeef");
    REQUIRE_FALSE(missingObj.has_value());
    REQUIRE((*nativeSession)->isAlive());

#ifdef GF_HAS_QT_ADAPTER
    ensureQtCoreApp();
    gf::infra::git::QtGitCommandExecutor qtExec(gitInfo->directBinaryPath);
    const auto qtStatus = qtExec.execute(statusCmd);
    REQUIRE(qtStatus.has_value());
    REQUIRE(qtStatus->succeeded());
    REQUIRE(qtStatus->stdoutBytes == nativeStatus->stdoutBytes);

    gf::infra::git::QtGitBatchSessionFactory qtBatchFactory(gitInfo->directBinaryPath);
    auto qtSession = qtBatchFactory.openSession(repoPath);
    REQUIRE(qtSession.has_value());
    for (std::size_t i = 0; i < oids.size(); ++i) {
        auto qtObj = (*qtSession)->readObject(oids[i]);
        REQUIRE(qtObj.has_value());
        REQUIRE(qtObj->content == nativeContents[i]);
    }
#endif
}
