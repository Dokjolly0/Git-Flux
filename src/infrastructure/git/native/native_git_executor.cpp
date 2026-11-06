#include "infrastructure/git/native/native_git_executor.hpp"
#include "core/shared/utf8_path.hpp"

#include <atomic>
#include <chrono>
#include <format>
#include <thread>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <cerrno>
#include <csignal>
#include <cstring>
#include <fcntl.h>
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>
extern char** environ;
#endif

namespace gf::infra::git {

namespace {

#ifdef _WIN32
[[nodiscard]] std::wstring utf8ToWide(std::string_view utf8) {
    if (utf8.empty()) {
        return {};
    }
    const int needed = ::MultiByteToWideChar(CP_UTF8, 0,
                                             utf8.data(), static_cast<int>(utf8.size()),
                                             nullptr, 0);
    if (needed <= 0) {
        return {};
    }
    std::wstring result(static_cast<std::size_t>(needed), L'\0');
    ::MultiByteToWideChar(CP_UTF8, 0,
                          utf8.data(), static_cast<int>(utf8.size()),
                          result.data(), needed);
    return result;
}

/// Applica le regole di quoting MSVCRT / CommandLineToArgvW senza passare da cmd.exe.
void appendQuotedWindowsArg(std::wstring& cmdLine, std::wstring_view arg) {
    const bool needsQuotes = arg.empty() ||
                             arg.find_first_of(L" \t\n\v\"") != std::wstring_view::npos;
    if (!needsQuotes) {
        cmdLine.append(arg);
        return;
    }

    cmdLine.push_back(L'"');
    for (std::size_t i = 0; i < arg.size();) {
        std::size_t backslashes = 0;
        while (i < arg.size() && arg[i] == L'\\') {
            ++backslashes;
            ++i;
        }

        if (i == arg.size()) {
            cmdLine.append(backslashes * 2, L'\\');
            break;
        } else if (arg[i] == L'"') {
            cmdLine.append(backslashes * 2 + 1, L'\\');
            cmdLine.push_back(L'"');
            ++i;
        } else {
            cmdLine.append(backslashes, L'\\');
            cmdLine.push_back(arg[i]);
            ++i;
        }
    }
    cmdLine.push_back(L'"');
}

struct ScopedHandle {
    HANDLE h = nullptr;
    ScopedHandle() = default;
    explicit ScopedHandle(HANDLE handle) noexcept : h(handle) {}
    ~ScopedHandle() { reset(); }
    ScopedHandle(const ScopedHandle&) = delete;
    ScopedHandle& operator=(const ScopedHandle&) = delete;
    ScopedHandle(ScopedHandle&& other) noexcept : h(other.h) { other.h = nullptr; }
    ScopedHandle& operator=(ScopedHandle&& other) noexcept {
        if (this != &other) {
            reset();
            h = other.h;
            other.h = nullptr;
        }
        return *this;
    }
    void reset(HANDLE newHandle = nullptr) noexcept {
        if (h && h != INVALID_HANDLE_VALUE) {
            ::CloseHandle(h);
        }
        h = newHandle;
    }
};

struct OverlappedPipePair {
    ScopedHandle parentRead;
    ScopedHandle childWrite;
};

[[nodiscard]] bool createOverlappedChildOutputPipe(OverlappedPipePair& out) {
    static std::atomic<std::uint64_t> counter{0};
    const auto id = counter.fetch_add(1, std::memory_order_relaxed);
    const std::wstring pipeName = std::format(
        L"\\\\.\\pipe\\LOCAL\\gf_git_io_{}_{}", ::GetCurrentProcessId(), id);

    HANDLE readEnd = ::CreateNamedPipeW(
        pipeName.c_str(),
        PIPE_ACCESS_INBOUND | FILE_FLAG_OVERLAPPED,
        PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT | PIPE_REJECT_REMOTE_CLIENTS,
        1,
        65536,
        65536,
        0,
        nullptr);
    if (readEnd == INVALID_HANDLE_VALUE) {
        return false;
    }

    SECURITY_ATTRIBUTES saInherit{};
    saInherit.nLength = sizeof(saInherit);
    saInherit.bInheritHandle = TRUE;

    HANDLE writeEnd = ::CreateFileW(
        pipeName.c_str(),
        GENERIC_WRITE,
        0,
        &saInherit,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        nullptr);
    if (writeEnd == INVALID_HANDLE_VALUE) {
        ::CloseHandle(readEnd);
        return false;
    }

    out.parentRead.reset(readEnd);
    out.childWrite.reset(writeEnd);
    return true;
}

struct AsyncPipeChannel {
    HANDLE pipe = nullptr;
    ScopedHandle event;
    OVERLAPPED ov{};
    char buffer[16384]{};
    std::string* destination = nullptr;
    bool pending = false;
    bool eof = false;

    bool init(HANDLE pipeHandle, std::string* dest) {
        pipe = pipeHandle;
        destination = dest;
        HANDLE ev = ::CreateEventW(nullptr, TRUE, FALSE, nullptr);
        if (!ev) {
            return false;
        }
        event.reset(ev);
        ov = OVERLAPPED{};
        ov.hEvent = event.h;
        return true;
    }

    void pumpReads() {
        while (!eof && !pending) {
            ::ResetEvent(event.h);
            DWORD bytesRead = 0;
            const BOOL ok = ::ReadFile(pipe, buffer, sizeof(buffer), &bytesRead, &ov);
            if (ok) {
                if (bytesRead == 0) {
                    eof = true;
                } else {
                    destination->append(buffer, bytesRead);
                }
            } else {
                const DWORD err = ::GetLastError();
                if (err == ERROR_IO_PENDING) {
                    pending = true;
                } else {
                    eof = true;
                }
            }
        }
    }

    void completePending() {
        if (!pending) {
            return;
        }
        DWORD bytesTransferred = 0;
        const BOOL ok = ::GetOverlappedResult(pipe, &ov, &bytesTransferred, FALSE);
        pending = false;
        if (!ok || bytesTransferred == 0) {
            eof = true;
            return;
        }
        destination->append(buffer, bytesTransferred);
        pumpReads();
    }
};

[[nodiscard]] std::wstring buildGitEnvironmentBlock(bool noOptionalLocks) {
    std::wstring block;
    LPWCH rawEnv = ::GetEnvironmentStringsW();
    if (rawEnv) {
        const wchar_t* cur = rawEnv;
        while (*cur != L'\0') {
            std::wstring_view entry(cur);
            const bool skip =
                entry.starts_with(L"GIT_TERMINAL_PROMPT=") ||
                entry.starts_with(L"GCM_INTERACTIVE=") ||
                entry.starts_with(L"GIT_OPTIONAL_LOCKS=") ||
                entry.starts_with(L"LC_ALL=");
            if (!skip) {
                block.append(entry);
                block.push_back(L'\0');
            }
            cur += entry.size() + 1;
        }
        ::FreeEnvironmentStringsW(rawEnv);
    }

    block.append(L"GIT_TERMINAL_PROMPT=0");
    block.push_back(L'\0');
    block.append(L"GCM_INTERACTIVE=never");
    block.push_back(L'\0');
    block.append(L"LC_ALL=C");
    block.push_back(L'\0');
    if (noOptionalLocks) {
        block.append(L"GIT_OPTIONAL_LOCKS=0");
        block.push_back(L'\0');
    }
    block.push_back(L'\0');
    return block;
}
#endif

} // namespace

NativeGitCommandExecutor::NativeGitCommandExecutor(std::filesystem::path gitExecutablePath,
                                                   std::string label)
    : gitExecutablePath_(std::move(gitExecutablePath))
    , label_(std::move(label))
#ifdef _WIN32
    , envBlockStandard_(buildGitEnvironmentBlock(false))
    , envBlockNoLocks_(buildGitEnvironmentBlock(true))
#endif
{}

std::wstring NativeGitCommandExecutor::buildWindowsCommandLine(
    const std::filesystem::path& executable,
    const std::vector<std::string>& utf8Args) {
    std::wstring cmdLine;
#ifdef _WIN32
    appendQuotedWindowsArg(cmdLine, executable.wstring());
    for (const auto& arg : utf8Args) {
        cmdLine.push_back(L' ');
        appendQuotedWindowsArg(cmdLine, utf8ToWide(arg));
    }
#else
    (void)executable;
    (void)utf8Args;
#endif
    return cmdLine;
}

std::expected<gf::git::bo::GitProcessResult, gf::git::bo::GitExecutionError>
NativeGitCommandExecutor::execute(const gf::git::bo::GitCommand& command,
                                  std::stop_token stopToken) {
    using gf::git::bo::GitErrorKind;
    using gf::git::bo::GitExecutionError;
    using gf::git::bo::GitLockPolicy;
    using gf::git::bo::GitProcessResult;

    const auto startClock = std::chrono::steady_clock::now();

    std::error_code fsEc;
    if (gitExecutablePath_.empty() || !std::filesystem::exists(gitExecutablePath_, fsEc)) {
        return std::unexpected(GitExecutionError{
            .kind = GitErrorKind::ExecutableNotFound,
            .message = "Eseguibile Git non trovato: " + gf::core::pathToUtf8(gitExecutablePath_)
        });
    }
    if (!command.workingDirectory.empty() && !std::filesystem::exists(command.workingDirectory, fsEc)) {
        return std::unexpected(GitExecutionError{
            .kind = GitErrorKind::WorkingDirectoryInvalid,
            .message = "Working directory inesistente: " + gf::core::pathToUtf8(command.workingDirectory)
        });
    }

    std::vector<std::string> fullArgs;
    const bool noOptLocks = (command.lockPolicy == GitLockPolicy::NoOptionalLocks);
    fullArgs.reserve(command.args.size() + (noOptLocks ? 1 : 0));
    if (noOptLocks) {
        fullArgs.emplace_back("--no-optional-locks");
    }
    fullArgs.insert(fullArgs.end(), command.args.begin(), command.args.end());

#ifdef _WIN32
    OverlappedPipePair stdoutPipe;
    OverlappedPipePair stderrPipe;
    if (!createOverlappedChildOutputPipe(stdoutPipe) ||
        !createOverlappedChildOutputPipe(stderrPipe)) {
        return std::unexpected(GitExecutionError{
            .kind = GitErrorKind::PipeIoError,
            .message = "Creazione pipe overlapped fallita",
            .osErrorCode = static_cast<int>(::GetLastError())
        });
    }

    SECURITY_ATTRIBUTES saInherit{};
    saInherit.nLength = sizeof(saInherit);
    saInherit.bInheritHandle = TRUE;
    ScopedHandle nulInput(::CreateFileW(L"NUL", GENERIC_READ,
                                        FILE_SHARE_READ | FILE_SHARE_WRITE,
                                        &saInherit, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr));

    STARTUPINFOW si{};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdInput = (nulInput.h != INVALID_HANDLE_VALUE) ? nulInput.h : ::GetStdHandle(STD_INPUT_HANDLE);
    si.hStdOutput = stdoutPipe.childWrite.h;
    si.hStdError = stderrPipe.childWrite.h;

    std::wstring cmdLine = buildWindowsCommandLine(gitExecutablePath_, fullArgs);
    const std::wstring exeWide = gitExecutablePath_.wstring();
    const std::wstring workDirWide = command.workingDirectory.empty()
                                         ? std::wstring{}
                                         : command.workingDirectory.wstring();
    std::wstring envBlock = noOptLocks ? envBlockNoLocks_ : envBlockStandard_;

    PROCESS_INFORMATION pi{};
    const BOOL spawned = ::CreateProcessW(
        exeWide.c_str(),
        cmdLine.data(),
        nullptr,
        nullptr,
        TRUE,
        CREATE_NO_WINDOW | CREATE_UNICODE_ENVIRONMENT,
        envBlock.data(),
        workDirWide.empty() ? nullptr : workDirWide.c_str(),
        &si,
        &pi);

    stdoutPipe.childWrite.reset();
    stderrPipe.childWrite.reset();
    nulInput.reset();

    if (!spawned) {
        return std::unexpected(GitExecutionError{
            .kind = GitErrorKind::SpawnFailed,
            .message = "CreateProcessW fallita per git.exe",
            .osErrorCode = static_cast<int>(::GetLastError())
        });
    }

    ScopedHandle procHandle(pi.hProcess);
    ScopedHandle threadHandle(pi.hThread);

    GitProcessResult result;
    std::atomic<bool> cancelledFlag{false};
    std::stop_callback stopCb(stopToken, [&]() noexcept {
        cancelledFlag.store(true, std::memory_order_release);
        ::TerminateProcess(procHandle.h, 130);
    });

    AsyncPipeChannel outCh;
    AsyncPipeChannel errCh;
    if (!outCh.init(stdoutPipe.parentRead.h, &result.stdoutBytes) ||
        !errCh.init(stderrPipe.parentRead.h, &result.stderrBytes)) {
        ::TerminateProcess(procHandle.h, 1);
        return std::unexpected(GitExecutionError{
            .kind = GitErrorKind::PipeIoError,
            .message = "Inizializzazione eventi overlapped fallita"
        });
    }

    outCh.pumpReads();
    errCh.pumpReads();

    const auto deadline = startClock + command.timeout;
    while (!outCh.eof || !errCh.eof) {
        HANDLE waitHandles[2]{};
        AsyncPipeChannel* waitChannels[2]{};
        DWORD handleCount = 0;

        if (outCh.pending) {
            waitHandles[handleCount] = outCh.event.h;
            waitChannels[handleCount] = &outCh;
            ++handleCount;
        }
        if (errCh.pending) {
            waitHandles[handleCount] = errCh.event.h;
            waitChannels[handleCount] = &errCh;
            ++handleCount;
        }
        if (handleCount == 0) {
            break;
        }

        const auto now = std::chrono::steady_clock::now();
        if (now >= deadline) {
            result.timedOut = true;
            ::TerminateProcess(procHandle.h, 124);
            ::CancelIo(stdoutPipe.parentRead.h);
            ::CancelIo(stderrPipe.parentRead.h);
            break;
        }
        const auto remainingMs = std::chrono::duration_cast<std::chrono::milliseconds>(deadline - now).count();
        const DWORD waitRc = ::WaitForMultipleObjects(
            handleCount, waitHandles, FALSE, static_cast<DWORD>(remainingMs));

        if (waitRc == WAIT_TIMEOUT) {
            result.timedOut = true;
            ::TerminateProcess(procHandle.h, 124);
            ::CancelIo(stdoutPipe.parentRead.h);
            ::CancelIo(stderrPipe.parentRead.h);
            break;
        }
        if (waitRc >= WAIT_OBJECT_0 && waitRc < WAIT_OBJECT_0 + handleCount) {
            waitChannels[waitRc - WAIT_OBJECT_0]->completePending();
        } else {
            break;
        }
    }

    ::WaitForSingleObject(procHandle.h, 5000);
    DWORD exitCode = 0;
    ::GetExitCodeProcess(procHandle.h, &exitCode);
    result.exitCode = static_cast<int>(exitCode);
    result.cancelled = cancelledFlag.load(std::memory_order_acquire);
    result.elapsed = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now() - startClock);
    return result;
#else
    int outPipe[2]{-1, -1};
    int errPipe[2]{-1, -1};
    if (::pipe(outPipe) != 0 || ::pipe(errPipe) != 0) {
        return std::unexpected(GitExecutionError{
            .kind = GitErrorKind::PipeIoError,
            .message = "pipe() fallita",
            .osErrorCode = errno
        });
    }

    posix_spawn_file_actions_t actions;
    ::posix_spawn_file_actions_init(&actions);
    ::posix_spawn_file_actions_addopen(&actions, STDIN_FILENO, "/dev/null", O_RDONLY, 0);
    ::posix_spawn_file_actions_adddup2(&actions, outPipe[1], STDOUT_FILENO);
    ::posix_spawn_file_actions_adddup2(&actions, errPipe[1], STDERR_FILENO);
    ::posix_spawn_file_actions_addclose(&actions, outPipe[0]);
    ::posix_spawn_file_actions_addclose(&actions, outPipe[1]);
    ::posix_spawn_file_actions_addclose(&actions, errPipe[0]);
    ::posix_spawn_file_actions_addclose(&actions, errPipe[1]);

    std::vector<std::string> argvStorage;
    argvStorage.reserve(fullArgs.size() + 3);
    argvStorage.push_back(gitExecutablePath_.string());
    if (!command.workingDirectory.empty()) {
        argvStorage.emplace_back("-C");
        argvStorage.push_back(command.workingDirectory.string());
    }
    for (const auto& a : fullArgs) {
        argvStorage.push_back(a);
    }
    std::vector<char*> argvPtrs;
    argvPtrs.reserve(argvStorage.size() + 1);
    for (auto& s : argvStorage) {
        argvPtrs.push_back(s.data());
    }
    argvPtrs.push_back(nullptr);

    pid_t pid = 0;
    const int spawnRc = ::posix_spawn(&pid, gitExecutablePath_.c_str(), &actions, nullptr,
                                      argvPtrs.data(), environ);
    ::posix_spawn_file_actions_destroy(&actions);
    ::close(outPipe[1]);
    ::close(errPipe[1]);

    if (spawnRc != 0) {
        ::close(outPipe[0]);
        ::close(errPipe[0]);
        return std::unexpected(GitExecutionError{
            .kind = GitErrorKind::SpawnFailed,
            .message = "posix_spawn fallita",
            .osErrorCode = spawnRc
        });
    }

    GitProcessResult result;
    std::jthread errThread([fd = errPipe[0], &result]() {
        char buf[8192];
        ssize_t n = 0;
        while ((n = ::read(fd, buf, sizeof(buf))) > 0) {
            result.stderrBytes.append(buf, static_cast<std::size_t>(n));
        }
        ::close(fd);
    });

    char buf[8192];
    ssize_t n = 0;
    while ((n = ::read(outPipe[0], buf, sizeof(buf))) > 0) {
        result.stdoutBytes.append(buf, static_cast<std::size_t>(n));
    }
    ::close(outPipe[0]);
    errThread.join();

    int status = 0;
    ::waitpid(pid, &status, 0);
    result.exitCode = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
    result.elapsed = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now() - startClock);
    return result;
#endif
}

} // namespace gf::infra::git
