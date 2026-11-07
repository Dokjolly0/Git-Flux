#include "infrastructure/git/native/native_git_batch_session.hpp"
#include "infrastructure/git/native/native_git_executor.hpp"
#include "core/shared/utf8_path.hpp"

#include <charconv>
#include <cstring>
#include <string>
#include <vector>

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
#include <fcntl.h>
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>
extern char** environ;
#endif

namespace gf::infra::git {

namespace {

using gf::git::bo::GitErrorKind;
using gf::git::bo::GitExecutionError;
using gf::git::ports::GitBatchObject;
using gf::git::ports::IGitBatchSession;

class NativeGitBatchSessionImpl final : public IGitBatchSession {
public:
#ifdef _WIN32
    NativeGitBatchSessionImpl(HANDLE processHandle, HANDLE stdinWrite, HANDLE stdoutRead) noexcept
        : processHandle_(processHandle)
        , stdinWrite_(stdinWrite)
        , stdoutRead_(stdoutRead) {}
#else
    NativeGitBatchSessionImpl(pid_t pid, int stdinFd, int stdoutFd) noexcept
        : pid_(pid)
        , stdinFd_(stdinFd)
        , stdoutFd_(stdoutFd) {}
#endif

    ~NativeGitBatchSessionImpl() override {
        closeSession();
    }

    [[nodiscard]] bool isAlive() const noexcept override {
#ifdef _WIN32
        if (!processHandle_) {
            return false;
        }
        DWORD exitCode = 0;
        return ::GetExitCodeProcess(processHandle_, &exitCode) && exitCode == STILL_ACTIVE;
#else
        return pid_ > 0 && stdinFd_ >= 0;
#endif
    }

    [[nodiscard]] std::expected<GitBatchObject, GitExecutionError>
    readObject(std::string_view revSpec) override {
        if (!isAlive()) {
            return std::unexpected(GitExecutionError{
                .kind = GitErrorKind::PipeIoError,
                .message = "Sessione git cat-file --batch non attiva"
            });
        }

        std::string request;
        request.reserve(revSpec.size() + 1);
        request.append(revSpec);
        request.push_back('\n');

        if (!writeAll(request.data(), request.size())) {
            return std::unexpected(GitExecutionError{
                .kind = GitErrorKind::PipeIoError,
                .message = "Scrittura fallita sulla pipe di git cat-file --batch"
            });
        }

        std::string headerLine;
        if (!readLine(headerLine)) {
            return std::unexpected(GitExecutionError{
                .kind = GitErrorKind::PipeIoError,
                .message = "Lettura header fallita da git cat-file --batch"
            });
        }

        // Expected format: "<oid> <type> <size>" or "<rev> missing"
        if (headerLine.ends_with(" missing")) {
            return std::unexpected(GitExecutionError{
                .kind = GitErrorKind::ProtocolParseError,
                .message = "Oggetto Git mancante: " + headerLine
            });
        }

        const std::size_t firstSpace = headerLine.find(' ');
        const std::size_t secondSpace = (firstSpace == std::string::npos)
                                            ? std::string::npos
                                            : headerLine.find(' ', firstSpace + 1);
        if (firstSpace == std::string::npos || secondSpace == std::string::npos) {
            return std::unexpected(GitExecutionError{
                .kind = GitErrorKind::ProtocolParseError,
                .message = "Header cat-file non valido: " + headerLine
            });
        }

        GitBatchObject obj;
        obj.header.oid = headerLine.substr(0, firstSpace);
        obj.header.type = headerLine.substr(firstSpace + 1, secondSpace - firstSpace - 1);

        std::string_view sizeStr(headerLine.data() + secondSpace + 1,
                                 headerLine.size() - secondSpace - 1);
        auto [ptr, ec] = std::from_chars(sizeStr.data(), sizeStr.data() + sizeStr.size(), obj.header.size);
        if (ec != std::errc{}) {
            return std::unexpected(GitExecutionError{
                .kind = GitErrorKind::ProtocolParseError,
                .message = "Dimensione oggetto non valida in cat-file: " + headerLine
            });
        }

        obj.content.resize(obj.header.size);
        if (obj.header.size > 0 && !readExact(obj.content.data(), obj.header.size)) {
            return std::unexpected(GitExecutionError{
                .kind = GitErrorKind::PipeIoError,
                .message = "Lettura payload incompleta da git cat-file --batch"
            });
        }

        // Consume trailing '\n' after binary payload
        char trailingLf = '\0';
        if (!readExact(&trailingLf, 1) || trailingLf != '\n') {
            return std::unexpected(GitExecutionError{
                .kind = GitErrorKind::ProtocolParseError,
                .message = "Terminatore LF mancante dopo payload cat-file"
            });
        }

        return obj;
    }

private:
    bool writeAll(const char* data, std::size_t len) noexcept {
        std::size_t writtenTotal = 0;
        while (writtenTotal < len) {
#ifdef _WIN32
            DWORD written = 0;
            if (!::WriteFile(stdinWrite_, data + writtenTotal,
                             static_cast<DWORD>(len - writtenTotal), &written, nullptr) ||
                written == 0) {
                return false;
            }
            writtenTotal += written;
#else
            const ssize_t n = ::write(stdinFd_, data + writtenTotal, len - writtenTotal);
            if (n <= 0) {
                return false;
            }
            writtenTotal += static_cast<std::size_t>(n);
#endif
        }
        return true;
    }

    bool refillBuffer() noexcept {
        bufPos_ = 0;
#ifdef _WIN32
        DWORD bytesRead = 0;
        if (!::ReadFile(stdoutRead_, readBuf_, sizeof(readBuf_), &bytesRead, nullptr) || bytesRead == 0) {
            bufLen_ = 0;
            return false;
        }
        bufLen_ = static_cast<std::size_t>(bytesRead);
        return true;
#else
        const ssize_t n = ::read(stdoutFd_, readBuf_, sizeof(readBuf_));
        if (n <= 0) {
            bufLen_ = 0;
            return false;
        }
        bufLen_ = static_cast<std::size_t>(n);
        return true;
#endif
    }

    bool readLine(std::string& outLine) {
        outLine.clear();
        while (true) {
            if (bufPos_ >= bufLen_ && !refillBuffer()) {
                return false;
            }
            const void* nlPtr = std::memchr(readBuf_ + bufPos_, '\n', bufLen_ - bufPos_);
            if (nlPtr) {
                const auto chunkLen = static_cast<const char*>(nlPtr) - (readBuf_ + bufPos_);
                outLine.append(readBuf_ + bufPos_, static_cast<std::size_t>(chunkLen));
                bufPos_ += static_cast<std::size_t>(chunkLen) + 1;
                if (!outLine.empty() && outLine.back() == '\r') {
                    outLine.pop_back();
                }
                return true;
            }
            outLine.append(readBuf_ + bufPos_, bufLen_ - bufPos_);
            bufPos_ = bufLen_;
        }
    }

    bool readExact(char* dest, std::size_t totalBytes) noexcept {
        std::size_t copied = 0;
        while (copied < totalBytes) {
            if (bufPos_ < bufLen_) {
                const std::size_t avail = bufLen_ - bufPos_;
                const std::size_t toCopy = (avail < (totalBytes - copied)) ? avail : (totalBytes - copied);
                std::memcpy(dest + copied, readBuf_ + bufPos_, toCopy);
                bufPos_ += toCopy;
                copied += toCopy;
            } else {
                // If remaining payload is large, read directly into destination buffer
                const std::size_t remaining = totalBytes - copied;
                if (remaining >= sizeof(readBuf_)) {
#ifdef _WIN32
                    DWORD n = 0;
                    if (!::ReadFile(stdoutRead_, dest + copied, static_cast<DWORD>(remaining), &n, nullptr) || n == 0) {
                        return false;
                    }
                    copied += static_cast<std::size_t>(n);
#else
                    const ssize_t n = ::read(stdoutFd_, dest + copied, remaining);
                    if (n <= 0) {
                        return false;
                    }
                    copied += static_cast<std::size_t>(n);
#endif
                } else if (!refillBuffer()) {
                    return false;
                }
            }
        }
        return true;
    }

    void closeSession() noexcept {
#ifdef _WIN32
        if (stdinWrite_) {
            ::CloseHandle(stdinWrite_);
            stdinWrite_ = nullptr;
        }
        if (stdoutRead_) {
            ::CloseHandle(stdoutRead_);
            stdoutRead_ = nullptr;
        }
        if (processHandle_) {
            if (::WaitForSingleObject(processHandle_, 1000) == WAIT_TIMEOUT) {
                ::TerminateProcess(processHandle_, 0);
            }
            ::CloseHandle(processHandle_);
            processHandle_ = nullptr;
        }
#else
        if (stdinFd_ >= 0) {
            ::close(stdinFd_);
            stdinFd_ = -1;
        }
        if (stdoutFd_ >= 0) {
            ::close(stdoutFd_);
            stdoutFd_ = -1;
        }
        if (pid_ > 0) {
            int status = 0;
            ::waitpid(pid_, &status, 0);
            pid_ = -1;
        }
#endif
    }

#ifdef _WIN32
    HANDLE processHandle_ = nullptr;
    HANDLE stdinWrite_ = nullptr;
    HANDLE stdoutRead_ = nullptr;
#else
    pid_t pid_ = -1;
    int stdinFd_ = -1;
    int stdoutFd_ = -1;
#endif
    char readBuf_[16384]{};
    std::size_t bufPos_ = 0;
    std::size_t bufLen_ = 0;
};

} // namespace

NativeGitBatchSessionFactory::NativeGitBatchSessionFactory(std::filesystem::path gitExecutablePath,
                                                           std::string label)
    : gitExecutablePath_(std::move(gitExecutablePath))
    , label_(std::move(label)) {}

std::expected<std::unique_ptr<gf::git::ports::IGitBatchSession>, gf::git::bo::GitExecutionError>
NativeGitBatchSessionFactory::openSession(const std::filesystem::path& repositoryPath) {
    std::error_code ec;
    if (!std::filesystem::exists(gitExecutablePath_, ec)) {
        return std::unexpected(GitExecutionError{
            .kind = GitErrorKind::ExecutableNotFound,
            .message = "Eseguibile Git non trovato per batch session"
        });
    }
    if (!std::filesystem::exists(repositoryPath, ec)) {
        return std::unexpected(GitExecutionError{
            .kind = GitErrorKind::WorkingDirectoryInvalid,
            .message = "Repository inesistente: " + gf::core::pathToUtf8(repositoryPath)
        });
    }

    const std::vector<std::string> args{"--no-optional-locks", "cat-file", "--batch"};

#ifdef _WIN32
    SECURITY_ATTRIBUTES sa{};
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;

    HANDLE childStdinRead = nullptr;
    HANDLE parentStdinWrite = nullptr;
    if (!::CreatePipe(&childStdinRead, &parentStdinWrite, &sa, 0)) {
        return std::unexpected(GitExecutionError{
            .kind = GitErrorKind::PipeIoError,
            .message = "CreatePipe stdin fallita",
            .osErrorCode = static_cast<int>(::GetLastError())
        });
    }
    ::SetHandleInformation(parentStdinWrite, HANDLE_FLAG_INHERIT, 0);

    HANDLE parentStdoutRead = nullptr;
    HANDLE childStdoutWrite = nullptr;
    if (!::CreatePipe(&parentStdoutRead, &childStdoutWrite, &sa, 65536)) {
        ::CloseHandle(childStdinRead);
        ::CloseHandle(parentStdinWrite);
        return std::unexpected(GitExecutionError{
            .kind = GitErrorKind::PipeIoError,
            .message = "CreatePipe stdout fallita",
            .osErrorCode = static_cast<int>(::GetLastError())
        });
    }
    ::SetHandleInformation(parentStdoutRead, HANDLE_FLAG_INHERIT, 0);

    HANDLE nulErr = ::CreateFileW(L"NUL", GENERIC_WRITE,
                                  FILE_SHARE_READ | FILE_SHARE_WRITE,
                                  &sa, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);

    STARTUPINFOW si{};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdInput = childStdinRead;
    si.hStdOutput = childStdoutWrite;
    si.hStdError = (nulErr != INVALID_HANDLE_VALUE) ? nulErr : ::GetStdHandle(STD_ERROR_HANDLE);

    std::wstring cmdLine = NativeGitCommandExecutor::buildWindowsCommandLine(gitExecutablePath_, args);
    const std::wstring exeWide = gitExecutablePath_.wstring();
    const std::wstring workDirWide = repositoryPath.wstring();

    PROCESS_INFORMATION pi{};
    const BOOL ok = ::CreateProcessW(
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

    ::CloseHandle(childStdinRead);
    ::CloseHandle(childStdoutWrite);
    if (nulErr != INVALID_HANDLE_VALUE) {
        ::CloseHandle(nulErr);
    }

    if (!ok) {
        ::CloseHandle(parentStdinWrite);
        ::CloseHandle(parentStdoutRead);
        return std::unexpected(GitExecutionError{
            .kind = GitErrorKind::SpawnFailed,
            .message = "CreateProcessW fallita per git cat-file --batch",
            .osErrorCode = static_cast<int>(::GetLastError())
        });
    }

    ::CloseHandle(pi.hThread);
    return std::make_unique<NativeGitBatchSessionImpl>(pi.hProcess, parentStdinWrite, parentStdoutRead);
#else
    int inPipe[2]{-1, -1};
    int outPipe[2]{-1, -1};
    if (::pipe(inPipe) != 0 || ::pipe(outPipe) != 0) {
        return std::unexpected(GitExecutionError{
            .kind = GitErrorKind::PipeIoError,
            .message = "pipe() fallita per batch session",
            .osErrorCode = errno
        });
    }

    posix_spawn_file_actions_t actions;
    ::posix_spawn_file_actions_init(&actions);
    ::posix_spawn_file_actions_adddup2(&actions, inPipe[0], STDIN_FILENO);
    ::posix_spawn_file_actions_adddup2(&actions, outPipe[1], STDOUT_FILENO);
    ::posix_spawn_file_actions_addopen(&actions, STDERR_FILENO, "/dev/null", O_WRONLY, 0);
    ::posix_spawn_file_actions_addclose(&actions, inPipe[0]);
    ::posix_spawn_file_actions_addclose(&actions, inPipe[1]);
    ::posix_spawn_file_actions_addclose(&actions, outPipe[0]);
    ::posix_spawn_file_actions_addclose(&actions, outPipe[1]);

    std::vector<std::string> argvStorage{
        gitExecutablePath_.string(), "-C", repositoryPath.string(),
        "--no-optional-locks", "cat-file", "--batch"
    };
    std::vector<char*> argvPtrs;
    for (auto& s : argvStorage) {
        argvPtrs.push_back(s.data());
    }
    argvPtrs.push_back(nullptr);

    pid_t pid = 0;
    const int rc = ::posix_spawn(&pid, gitExecutablePath_.c_str(), &actions, nullptr,
                                 argvPtrs.data(), environ);
    ::posix_spawn_file_actions_destroy(&actions);
    ::close(inPipe[0]);
    ::close(outPipe[1]);

    if (rc != 0) {
        ::close(inPipe[1]);
        ::close(outPipe[0]);
        return std::unexpected(GitExecutionError{
            .kind = GitErrorKind::SpawnFailed,
            .message = "posix_spawn fallita per git cat-file --batch",
            .osErrorCode = rc
        });
    }

    return std::make_unique<NativeGitBatchSessionImpl>(pid, inPipe[1], outPipe[0]);
#endif
}

} // namespace gf::infra::git
