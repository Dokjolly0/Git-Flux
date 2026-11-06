#include <catch2/catch_test_macros.hpp>

#include "core/modules/git/bl/operation_coordinator.hpp"
#include "core/modules/git/bl/porcelain_status_parser.hpp"
#include "core/modules/repositories/bo/repository_id.hpp"
#include "core/shared/strong_id.hpp"
#include "core/shared/utf8_path.hpp"
#include "infrastructure/git/native/native_git_executor.hpp"

#include <atomic>
#include <chrono>
#include <string>
#include <thread>
#include <unordered_set>

using namespace std::chrono_literals;

TEST_CASE("StrongId garantisce type-safety, confronto e hashing", "[unit][core]") {
    using gf::repositories::bo::RepositoryId;
    RepositoryId a("repo-alpha");
    RepositoryId b("repo-alpha");
    RepositoryId c("repo-beta");

    REQUIRE(a == b);
    REQUIRE(a != c);
    REQUIRE_FALSE(a.empty());

    std::unordered_set<RepositoryId> set;
    set.insert(a);
    set.insert(b);
    set.insert(c);
    REQUIRE(set.size() == 2);
}

TEST_CASE("Conversione UTF-8 <-> std::filesystem::path preserva caratteri Unicode", "[unit][core]") {
    const std::string original = "progetti/città/specifica_🚀_test.md";
    const auto path = gf::core::utf8ToPath(original);
    const std::string roundtrip = gf::core::pathToUtf8(path);
    // Normalizza eventuali separatori prima del confronto
    REQUIRE(roundtrip == original);
}

TEST_CASE("PorcelainStatusParser interpreta header branch e tutti i tipi di record NUL-terminati", "[unit][git]") {
    std::string payload;
    auto addRec = [&](std::string_view s) {
        payload.append(s);
        payload.push_back('\0');
    };

    addRec("# branch.oid 0123456789abcdef0123456789abcdef01234567");
    addRec("# branch.head main");
    addRec("# branch.upstream origin/main");
    addRec("# branch.ab +4 -2");
    addRec("1 M. N... 100644 100644 100644 aaaaaaa bbbbbbb src/main.cpp");
    addRec("1 .M N... 100644 100644 100644 ccccccc ddddddd docs/città con spazi.md");
    addRec("2 R. N... 100644 100644 100644 eeeeeee fffffff R100 src/new_name.cpp");
    addRec("src/old_name.cpp");
    addRec("u UU N... 100644 100644 100644 100644 111 222 333 src/conflict.cpp");
    addRec("? scratch_note.txt");

    const auto parsed = gf::git::bl::PorcelainStatusParser::parse(payload);
    REQUIRE(parsed.has_value());

    REQUIRE(parsed->branch.headName == "main");
    REQUIRE(parsed->branch.upstream == "origin/main");
    REQUIRE(parsed->branch.ahead == 4);
    REQUIRE(parsed->branch.behind == 2);
    REQUIRE_FALSE(parsed->branch.detached);
    REQUIRE(parsed->entries.size() == 5);

    REQUIRE(parsed->entries[0].hasStagedChanges());
    REQUIRE_FALSE(parsed->entries[0].hasUnstagedChanges());
    REQUIRE(parsed->entries[0].path == "src/main.cpp");

    REQUIRE(parsed->entries[1].hasUnstagedChanges());
    REQUIRE(parsed->entries[1].path == "docs/città con spazi.md");

    REQUIRE(parsed->entries[2].stagedStatus == 'R');
    REQUIRE(parsed->entries[2].path == "src/new_name.cpp");
    REQUIRE(parsed->entries[2].originalPath == "src/old_name.cpp");

    REQUIRE(parsed->entries[3].isConflict);
    REQUIRE(parsed->entries[3].path == "src/conflict.cpp");

    REQUIRE(parsed->entries[4].isUntracked);
    REQUIRE(parsed->entries[4].path == "scratch_note.txt");
}

TEST_CASE("OperationCoordinator serializza mutazioni sullo stesso repo ma parallelizza repo distinti", "[unit][coordinator]") {
    using gf::git::bl::OperationCoordinator;
    using gf::git::bl::OperationKind;
    using gf::git::bl::TaskPriority;
    using gf::repositories::bo::RepositoryId;

    OperationCoordinator coordinator(4);

    std::atomic<int> activeOnRepoA{0};
    std::atomic<int> maxConcurrentOnRepoA{0};

    std::vector<std::future<void>> futures;
    // 8 mutazioni concorrenti sullo STESSO RepositoryId("repo-A")
    for (int i = 0; i < 8; ++i) {
        futures.push_back(coordinator.submit(
            RepositoryId("repo-A"),
            OperationKind::Mutating,
            TaskPriority::NormalBackground,
            [&]() {
                const int now = activeOnRepoA.fetch_add(1) + 1;
                int prev = maxConcurrentOnRepoA.load();
                while (now > prev && !maxConcurrentOnRepoA.compare_exchange_weak(prev, now)) {}
                std::this_thread::sleep_for(5ms);
                activeOnRepoA.fetch_sub(1);
            }));
    }

    // In contemporanea, 4 letture passive su repository DIVERSI
    for (int i = 0; i < 4; ++i) {
        futures.push_back(coordinator.submit(
            RepositoryId("other-" + std::to_string(i)),
            OperationKind::PassiveRead,
            TaskPriority::NormalBackground,
            []() { std::this_thread::sleep_for(10ms); }));
    }

    for (auto& f : futures) {
        f.get();
    }

    // Sullo stesso repo non devono MAI sovrapporsi due operazioni Mutating
    REQUIRE(maxConcurrentOnRepoA.load() == 1);
    // Globalmente i worker devono aver lavorato in parallelo entro il tetto di 4
    REQUIRE(coordinator.maxObservedActiveWorkers() <= 4);
    REQUIRE(coordinator.maxObservedActiveWorkers() >= 2);
}

TEST_CASE("OperationCoordinator serve HighActiveRepo prima della coda NormalBackground", "[unit][coordinator]") {
    using gf::git::bl::OperationCoordinator;
    using gf::git::bl::OperationKind;
    using gf::git::bl::TaskPriority;
    using gf::repositories::bo::RepositoryId;

    // 1 solo worker per rendere l'ordine deterministico
    OperationCoordinator coordinator(1);

    std::promise<void> gatePromise;
    auto gateFuture = gatePromise.get_future().share();

    std::vector<int> executionOrder;
    std::mutex orderMutex;

    // Task 0 occupa l'unico worker finché non apriamo il gate
    auto blocker = coordinator.submit(
        RepositoryId("r0"), OperationKind::PassiveRead, TaskPriority::NormalBackground,
        [gateFuture]() { gateFuture.wait(); });

    // Accodiamo 5 task NormalBackground (id 1..5)
    std::vector<std::future<void>> bgFutures;
    for (int i = 1; i <= 5; ++i) {
        bgFutures.push_back(coordinator.submit(
            RepositoryId("bg-" + std::to_string(i)),
            OperationKind::PassiveRead,
            TaskPriority::NormalBackground,
            [i, &executionOrder, &orderMutex]() {
                std::scoped_lock lk(orderMutex);
                executionOrder.push_back(i);
            }));
    }

    // Accodiamo per ultimo un task HighActiveRepo (id 99)
    auto highFuture = coordinator.submit(
        RepositoryId("active-repo"),
        OperationKind::PassiveRead,
        TaskPriority::HighActiveRepo,
        [&executionOrder, &orderMutex]() {
            std::scoped_lock lk(orderMutex);
            executionOrder.push_back(99);
        });

    gatePromise.set_value();
    blocker.get();
    highFuture.get();
    for (auto& f : bgFutures) {
        f.get();
    }

    REQUIRE(executionOrder.size() == 6);
    REQUIRE(executionOrder.front() == 99);
}

#ifdef _WIN32
TEST_CASE("Quoting command-line Windows gestisce spazi, virgolette e backslash finali", "[unit][win32]") {
    const auto cmd = gf::infra::git::NativeGitCommandExecutor::buildWindowsCommandLine(
        R"(C:\Program Files\Git\mingw64\bin\git.exe)",
        {"commit", "-m", R"(Fix "quoted" path C:\dir with space\)"});

    const std::wstring expected =
        LR"("C:\Program Files\Git\mingw64\bin\git.exe" commit -m "Fix \"quoted\" path C:\dir with space\\")";
    REQUIRE(cmd == expected);
}
#endif
