#pragma once

#include <chrono>
#include <filesystem>
#include <string>
#include <vector>

namespace gf::git::bo {

/// Politica sui lock opzionali di Git.
/// NoOptionalLocks passa `--no-optional-locks` (e GIT_OPTIONAL_LOCKS=0),
/// impedendo a comandi passivi come `git status` di acquisire `.git/index.lock`.
enum class GitLockPolicy {
    Standard,
    NoOptionalLocks
};

struct GitCommand {
    std::vector<std::string> args;
    std::filesystem::path workingDirectory;
    GitLockPolicy lockPolicy = GitLockPolicy::NoOptionalLocks;
    std::chrono::milliseconds timeout{30'000};
};

struct GitProcessResult {
    int exitCode = -1;
    std::string stdoutBytes;
    std::string stderrBytes;
    std::chrono::microseconds elapsed{0};
    bool cancelled = false;
    bool timedOut = false;

    [[nodiscard]] bool succeeded() const noexcept {
        return exitCode == 0 && !cancelled && !timedOut;
    }
};

enum class GitErrorKind {
    ExecutableNotFound,
    WorkingDirectoryInvalid,
    SpawnFailed,
    PipeIoError,
    ProtocolParseError,
    Cancelled,
    TimedOut
};

struct GitExecutionError {
    GitErrorKind kind = GitErrorKind::SpawnFailed;
    std::string message;
    int osErrorCode = 0;
};

struct GitExecutableInfo {
    std::filesystem::path discoveredInPath;
    std::filesystem::path directBinaryPath;
    std::string versionString;
    bool isWindowsCmdShim = false;
};

} // namespace gf::git::bo
