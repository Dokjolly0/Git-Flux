#include "fixture_generator.hpp"
#include "core/shared/utf8_path.hpp"
#include "infrastructure/git/native/native_git_executor.hpp"

#include <atomic>
#include <format>
#include <fstream>
#include <random>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <fcntl.h>
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>
extern char** environ;
#endif

namespace gf::tools {

ScopedTempDir::ScopedTempDir(std::string_view prefix) {
    static std::atomic<std::uint64_t> seq{0};
    const auto nowTicks = std::chrono::steady_clock::now().time_since_epoch().count();
    const auto id = seq.fetch_add(1, std::memory_order_relaxed);
    const std::string folderName = std::format("{}{}_{}", prefix, nowTicks, id);
    path_ = std::filesystem::temp_directory_path() / folderName;
    std::filesystem::create_directories(path_);
}

ScopedTempDir::~ScopedTempDir() {
    if (path_.empty()) {
        return;
    }
    std::error_code ec;
    // Su Windows i packfile Git in .git/objects/pack hanno talvolta l'attributo read-only:
    // rimuoviamo i permessi sola-lettura prima della cancellazione ricorsiva.
    for (auto it = std::filesystem::recursive_directory_iterator(
             path_, std::filesystem::directory_options::skip_permission_denied, ec);
         !ec && it != std::filesystem::recursive_directory_iterator();
         it.increment(ec)) {
        std::filesystem::permissions(
            it->path(),
            std::filesystem::perms::owner_write,
            std::filesystem::perm_options::add,
            ec);
        ec.clear();
    }
    std::filesystem::remove_all(path_, ec);
}

FixtureGenerator::FixtureGenerator(std::filesystem::path gitExecutablePath)
    : gitExe_(std::move(gitExecutablePath)) {}

std::expected<void, std::string>
FixtureGenerator::runFastImportFromFile(const std::filesystem::path& repoPath,
                                        const std::filesystem::path& streamFilePath) const {
    const std::vector<std::string> args{"fast-import", "--quiet", "--force"};

#ifdef _WIN32
    SECURITY_ATTRIBUTES sa{};
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;

    HANDLE inputHandle = ::CreateFileW(
        streamFilePath.c_str(),
        GENERIC_READ,
        FILE_SHARE_READ,
        &sa,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN,
        nullptr);
    if (inputHandle == INVALID_HANDLE_VALUE) {
        return std::unexpected("Impossibile aprire file stream fast-import: err=" + std::to_string(::GetLastError()));
    }

    HANDLE errRead = nullptr;
    HANDLE errWrite = nullptr;
    ::CreatePipe(&errRead, &errWrite, &sa, 0);
    ::SetHandleInformation(errRead, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOW si{};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdInput = inputHandle;
    si.hStdOutput = ::GetStdHandle(STD_OUTPUT_HANDLE);
    si.hStdError = errWrite;

    std::wstring cmdLine = gf::infra::git::NativeGitCommandExecutor::buildWindowsCommandLine(gitExe_, args);
    const std::wstring exeWide = gitExe_.wstring();
    const std::wstring workDirWide = repoPath.wstring();

    PROCESS_INFORMATION pi{};
    const BOOL spawned = ::CreateProcessW(
        exeWide.c_str(),
        cmdLine.data(),
        nullptr,
        nullptr,
        TRUE,
        CREATE_NO_WINDOW,
        nullptr,
        workDirWide.c_str(),
        &si,
        &pi);

    ::CloseHandle(inputHandle);
    ::CloseHandle(errWrite);

    if (!spawned) {
        if (errRead) ::CloseHandle(errRead);
        return std::unexpected("CreateProcessW fallita per git fast-import");
    }

    std::string errOutput;
    char errBuf[4096];
    DWORD bytesRead = 0;
    while (::ReadFile(errRead, errBuf, sizeof(errBuf), &bytesRead, nullptr) && bytesRead > 0) {
        errOutput.append(errBuf, bytesRead);
    }
    ::CloseHandle(errRead);

    ::WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD exitCode = 1;
    ::GetExitCodeProcess(pi.hProcess, &exitCode);
    ::CloseHandle(pi.hThread);
    ::CloseHandle(pi.hProcess);

    if (exitCode != 0 || !errOutput.empty()) {
        return std::unexpected(std::format("git fast-import terminato con codice {} (stderr: {})", exitCode, errOutput));
    }
    return {};
#else
    posix_spawn_file_actions_t actions;
    ::posix_spawn_file_actions_init(&actions);
    ::posix_spawn_file_actions_addopen(&actions, STDIN_FILENO, streamFilePath.c_str(), O_RDONLY, 0);
    ::posix_spawn_file_actions_addopen(&actions, STDOUT_FILENO, "/dev/null", O_WRONLY, 0);
    ::posix_spawn_file_actions_addopen(&actions, STDERR_FILENO, "/dev/null", O_WRONLY, 0);

    std::vector<std::string> argvStorage{
        gitExe_.string(), "-C", repoPath.string(), "fast-import", "--quiet", "--force"
    };
    std::vector<char*> argvPtrs;
    for (auto& s : argvStorage) {
        argvPtrs.push_back(s.data());
    }
    argvPtrs.push_back(nullptr);

    pid_t pid = 0;
    const int rc = ::posix_spawn(&pid, gitExe_.c_str(), &actions, nullptr, argvPtrs.data(), environ);
    ::posix_spawn_file_actions_destroy(&actions);
    if (rc != 0) {
        return std::unexpected("posix_spawn fallita per git fast-import");
    }
    int status = 0;
    ::waitpid(pid, &status, 0);
    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
        return std::unexpected("git fast-import fallito");
    }
    return {};
#endif
}

std::expected<FastImportResult, std::string>
FixtureGenerator::generateFastImportRepo(const FastImportSpec& spec) const {
    const auto t0 = std::chrono::steady_clock::now();

    std::error_code ec;
    if (std::filesystem::exists(spec.repoPath, ec)) {
        for (auto it = std::filesystem::recursive_directory_iterator(
                 spec.repoPath, std::filesystem::directory_options::skip_permission_denied, ec);
             !ec && it != std::filesystem::recursive_directory_iterator();
             it.increment(ec)) {
            std::filesystem::permissions(
                it->path(), std::filesystem::perms::owner_write,
                std::filesystem::perm_options::add, ec);
            ec.clear();
        }
        std::filesystem::remove_all(spec.repoPath, ec);
    }
    std::filesystem::create_directories(spec.repoPath, ec);
    if (ec) {
        return std::unexpected("Impossibile creare directory repo: " + ec.message());
    }

    gf::infra::git::NativeGitCommandExecutor executor(gitExe_);

    // 1. git init -q -b main
    auto initRes = executor.execute({
        .args = {"init", "-q", "-b", "main"},
        .workingDirectory = spec.repoPath,
        .lockPolicy = gf::git::bo::GitLockPolicy::Standard
    });
    if (!initRes || !initRes->succeeded()) {
        return std::unexpected("git init fallito");
    }

    // Configura identità locale per eventuali commit successivi nei test
    (void)executor.execute({
        .args = {"config", "user.name", "Git Flux Test Bot"},
        .workingDirectory = spec.repoPath,
        .lockPolicy = gf::git::bo::GitLockPolicy::Standard
    });
    (void)executor.execute({
        .args = {"config", "user.email", "bot@gitflux.local"},
        .workingDirectory = spec.repoPath,
        .lockPolicy = gf::git::bo::GitLockPolicy::Standard
    });

    if (spec.commitCount == 0) {
        return FastImportResult{spec.repoPath, 0, 0, std::chrono::milliseconds{0}};
    }

    // 2. Genera stream fast-import deterministico su file binario temporaneo
    const std::filesystem::path streamPath = spec.repoPath.parent_path() /
        (spec.repoPath.filename().string() + "_fast_import.stream");

    std::size_t mergesCount = 0;
    {
        std::ofstream out(streamPath, std::ios::binary);
        if (!out) {
            return std::unexpected("Impossibile creare file stream temporaneo: " + gf::core::pathToUtf8(streamPath));
        }

        std::mt19937_64 rng(spec.seed);
        constexpr std::size_t kFeatureSlots = 8;
        std::size_t mainTipMark = 0;
        std::size_t featureTipMarks[kFeatureSlots]{};
        std::size_t featureEpochs[kFeatureSlots]{};

        constexpr std::int64_t baseTimestamp = 1700000000;

        for (std::size_t i = 1; i <= spec.commitCount; ++i) {
            const std::int64_t ts = baseTimestamp + static_cast<std::int64_t>(i * 60);
            const std::uint64_t roll = rng() % 100;

            std::string targetRef = "refs/heads/main";
            std::size_t parentMark = mainTipMark;
            std::size_t mergeMark = 0;

            if (i == 1) {
                mainTipMark = 1;
            } else if (roll < 20) {
                // Avanza o crea un feature branch parallelo
                const std::size_t slot = static_cast<std::size_t>(rng() % kFeatureSlots);
                if (featureTipMarks[slot] == 0) {
                    featureEpochs[slot] = (featureEpochs[slot] + 1) % 4;
                    parentMark = mainTipMark;
                } else {
                    parentMark = featureTipMarks[slot];
                }
                targetRef = std::format("refs/heads/feat-{}-{}", slot, featureEpochs[slot]);
                featureTipMarks[slot] = i;
            } else if (roll < 28) {
                // Cerca un feature branch aperto da mergiare in main
                const std::size_t slot = static_cast<std::size_t>(rng() % kFeatureSlots);
                if (featureTipMarks[slot] != 0 && featureTipMarks[slot] != mainTipMark) {
                    mergeMark = featureTipMarks[slot];
                    featureTipMarks[slot] = 0;
                    ++mergesCount;
                }
                parentMark = mainTipMark;
                mainTipMark = i;
            } else {
                parentMark = mainTipMark;
                mainTipMark = i;
            }

            const std::string msg = (mergeMark != 0)
                ? std::format("Merge feature branch into main (#{})", i)
                : std::format("feat(core): synthetic commit #{} for benchmark", i);

            const std::string fileContent = std::format(
                "// Git Flux synthetic file\ncommit_index={}\nseed={}\n", i, spec.seed);
            const std::size_t fileIdx = i % 32;

            out << "commit " << targetRef << "\n"
                << "mark :" << i << "\n"
                << "committer Git Flux Bot <bot@gitflux.local> " << ts << " +0000\n"
                << "data " << msg.size() << "\n" << msg << "\n";

            if (parentMark != 0) {
                out << "from :" << parentMark << "\n";
            }
            if (mergeMark != 0) {
                out << "merge :" << mergeMark << "\n";
            }

            out << "M 100644 inline src/module_" << fileIdx << "/item.cpp\n"
                << "data " << fileContent.size() << "\n" << fileContent << "\n";

            if (i == 1) {
                const std::string unicodeDoc = "# Documentazione Città & Flussi\nTest UTF-8.\n";
                out << "M 100644 inline docs/specifica_città.md\n"
                    << "data " << unicodeDoc.size() << "\n" << unicodeDoc << "\n";
            }
            out << "\n";
        }
        out << "done\n";
        out.flush();
        out.close();
    }

    const auto streamSize = std::filesystem::file_size(streamPath, ec);
    if (ec || streamSize == 0) {
        return std::unexpected("Stream fast-import vuoto o non scritto su disco");
    }

    // 3. Esegue git fast-import leggendo il file binario
    auto importRes = runFastImportFromFile(spec.repoPath, streamPath);
    std::filesystem::remove(streamPath, ec);
    if (!importRes) {
        return std::unexpected(importRes.error());
    }

    // 4. Allinea l'index e il working tree a `main` se richiesto
    if (spec.checkoutWorkingTree) {
        auto coRes = executor.execute({
            .args = {"reset", "--hard", "refs/heads/main"},
            .workingDirectory = spec.repoPath,
            .lockPolicy = gf::git::bo::GitLockPolicy::Standard
        });
        if (!coRes || !coRes->succeeded()) {
            return std::unexpected("git reset --hard main post fast-import fallito: code=" +
                                  std::to_string(coRes ? coRes->exitCode : -1) +
                                  " err=" + (coRes ? coRes->stderrBytes : ""));
        }
    }

    const auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - t0);

    return FastImportResult{
        .repoPath = spec.repoPath,
        .commitsCreated = spec.commitCount,
        .mergesCreated = mergesCount,
        .elapsed = elapsedMs
    };
}

std::expected<FleetResult, std::string>
FixtureGenerator::generateRepoFleet(const FleetSpec& spec) const {
    const auto t0 = std::chrono::steady_clock::now();

    std::error_code ec;
    std::filesystem::create_directories(spec.rootDir, ec);
    if (ec) {
        return std::unexpected("Impossibile creare directory flotta: " + ec.message());
    }

    // 1. Crea un singolo template locale con N commit
    const std::filesystem::path templateDir = spec.rootDir / "_template_base";
    auto tplRes = generateFastImportRepo({
        .repoPath = templateDir,
        .commitCount = spec.commitsPerRepo,
        .seed = spec.seed,
        .checkoutWorkingTree = true
    });
    if (!tplRes) {
        return std::unexpected("Creazione template flotta fallita: " + tplRes.error());
    }

    gf::infra::git::NativeGitCommandExecutor executor(gitExe_);
    FleetResult result;
    result.repoPaths.reserve(spec.repoCount);

    for (std::size_t i = 0; i < spec.repoCount; ++i) {
        const std::filesystem::path repoDir = spec.rootDir / std::format("repo_{:03}", i);
        std::filesystem::copy(
            templateDir, repoDir,
            std::filesystem::copy_options::recursive | std::filesystem::copy_options::overwrite_existing,
            ec);
        if (ec) {
            return std::unexpected("Copia template fallita: " + ec.message());
        }

        const std::size_t bucket = i % 10;
        if (bucket < 7) {
            // 70% clean
            ++result.cleanCount;
        } else if (bucket < 9) {
            // 20% modified + untracked
            {
                std::ofstream mod(repoDir / "src/module_1/item.cpp", std::ios::app);
                mod << "// Local unstaged edit in repo " << i << "\n";
            }
            {
                std::ofstream untracked(repoDir / "local_notes.txt");
                untracked << "Untracked scratch note " << i << "\n";
            }
            ++result.dirtyUnstagedCount;
        } else {
            // 10% staged modification
            {
                std::ofstream stagedFile(repoDir / "src/module_1/item.cpp", std::ios::app);
                stagedFile << "// Staged change in repo " << i << "\n";
            }
            auto addRes = executor.execute({
                .args = {"add", "src/module_1/item.cpp"},
                .workingDirectory = repoDir,
                .lockPolicy = gf::git::bo::GitLockPolicy::Standard
            });
            if (!addRes || !addRes->succeeded()) {
                return std::unexpected("git add nella generazione flotta fallito");
            }
            ++result.stagedCount;
        }

        result.repoPaths.push_back(repoDir);
    }

    // Rimuove il template di appoggio
    for (auto it = std::filesystem::recursive_directory_iterator(templateDir, ec);
         !ec && it != std::filesystem::recursive_directory_iterator();
         it.increment(ec)) {
        std::filesystem::permissions(
            it->path(), std::filesystem::perms::owner_write,
            std::filesystem::perm_options::add, ec);
        ec.clear();
    }
    std::filesystem::remove_all(templateDir, ec);

    result.elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - t0);
    return result;
}

} // namespace gf::tools
