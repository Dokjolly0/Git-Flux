#include "infrastructure/terminal/native_terminal_process_runner.hpp"
#include "infrastructure/process/native_process_utils.hpp"

#include "core/modules/terminal/bl/command_line_tokenizer.hpp"
#include "core/shared/string_utils.hpp"
#include "core/shared/utf8_path.hpp"

#include <algorithm>
#include <atomic>
#include <cctype>
#include <chrono>
#include <cstdlib>
#include <fstream>
#include <mutex>
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
#include <sys/wait.h>
#include <unistd.h>
#endif

namespace gf::infra::terminal
{
    namespace
    {
        namespace tbo = gf::terminal::bo;

        [[nodiscard]] bool hasExplicitColorFlag(const std::vector<std::string>& args)
        {
            for (std::size_t i = 1; i < args.size(); ++i)
            {
                const std::string_view a = args[i];
                if (a == "--no-color" || a.starts_with("--color") || a == "color.ui=never" || a == "color.ui=false")
                {
                    return true;
                }
            }
            return false;
        }

        [[nodiscard]] bool isDirectGitInvocation(const std::string_view commandLine,
                                                 std::vector<std::string>& outGitArgs)
        {
            std::vector<std::string> rawArgs;
            if (!gf::terminal::bl::CommandLineTokenizer::trySplitSimpleArgs(commandLine, rawArgs))
            {
                return false;
            }
            if (rawArgs.empty()) return false;
            const std::string cmd0 = gf::core::toLowerAscii(rawArgs[0]);
            if (cmd0 != "git" && cmd0 != "git.exe")
            {
                return false;
            }
            outGitArgs.assign(rawArgs.begin() + 1, rawArgs.end());
            return true;
        }

        [[nodiscard]] const tbo::TerminalShellInfo* pickShell(
            const std::vector<tbo::TerminalShellInfo>& shells,
            const tbo::TerminalShellKind requested)
        {
            if (shells.empty()) return nullptr;
            if (requested != tbo::TerminalShellKind::Auto)
            {
                for (const auto& s : shells)
                {
                    if (s.kind == requested) return &s;
                }
            }
            return &shells.front();
        }

        [[nodiscard]] std::filesystem::path makeCwdTempFilePath()
        {
            static std::atomic<std::uint64_t> s_counter{0};
            std::error_code ec;
            auto dir = std::filesystem::temp_directory_path(ec);
            if (ec || dir.empty())
            {
                return {};
            }
#ifdef _WIN32
            const auto pid = static_cast<std::uint64_t>(GetCurrentProcessId());
#else
            const auto pid = static_cast<std::uint64_t>(::getpid());
#endif
            const auto seq = s_counter.fetch_add(1, std::memory_order_relaxed);
            return dir / ("gitflux_cwd_" + std::to_string(pid) + "_" + std::to_string(seq) + ".tmp");
        }

        [[nodiscard]] std::optional<std::filesystem::path> readAndCleanupCwdFile(
            const std::filesystem::path& cwdFile,
            const bool readValue)
        {
            if (cwdFile.empty()) return std::nullopt;

            std::optional<std::filesystem::path> result;
            if (readValue)
            {
                std::ifstream in(cwdFile, std::ios::binary);
                if (in)
                {
                    std::string raw((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
                    // Strip optional UTF-8 BOM
                    if (raw.size() >= 3 &&
                        static_cast<unsigned char>(raw[0]) == 0xEF &&
                        static_cast<unsigned char>(raw[1]) == 0xBB &&
                        static_cast<unsigned char>(raw[2]) == 0xBF)
                    {
                        raw.erase(0, 3);
                    }
                    while (!raw.empty() && (raw.back() == '\r' || raw.back() == '\n' || raw.back() == ' ' || raw.back() == '\t'))
                    {
                        raw.pop_back();
                    }
                    std::size_t start = 0;
                    while (start < raw.size() && (raw[start] == ' ' || raw[start] == '\t' || raw[start] == '\r' || raw[start] == '\n'))
                    {
                        ++start;
                    }
                    if (start > 0)
                    {
                        raw.erase(0, start);
                    }
                    if (!raw.empty())
                    {
                        const std::filesystem::path candidate = gf::core::utf8ToPath(raw);
                        std::error_code ec;
                        if (std::filesystem::exists(candidate, ec) && std::filesystem::is_directory(candidate, ec))
                        {
                            result = candidate;
                        }
                    }
                }
            }

            std::error_code ec;
            std::filesystem::remove(cwdFile, ec);
            return result;
        }

        void appendDirectGitArgs(std::vector<std::string>& outArgs,
                                 std::vector<std::string>& directGitArgs,
                                 const bool forceGitColor)
        {
            outArgs.emplace_back("-c");
            outArgs.emplace_back("core.quotepath=false");
            if (forceGitColor && !hasExplicitColorFlag(directGitArgs))
            {
                outArgs.emplace_back("-c");
                outArgs.emplace_back("color.ui=always");
            }
            for (auto& a : directGitArgs)
            {
                outArgs.push_back(std::move(a));
            }
        }

        [[nodiscard]] std::vector<std::pair<std::string, std::string>>
        withTerminalEnvDefaults(const std::vector<std::pair<std::string, std::string>>& overrides)
        {
            std::vector<std::pair<std::string, std::string>> env = {
                {"GIT_PAGER", "cat"},
                {"PAGER", "cat"},
                {"TERM", "xterm-256color"},
                {"CLICOLOR_FORCE", "1"},
                {"FORCE_COLOR", "1"}
            };
            env.insert(env.end(), overrides.begin(), overrides.end());
            return env;
        }

        template <typename Derived>
        class TerminalProcessHandleBase : public gf::terminal::ports::ITerminalProcessHandle
        {
        public:
            TerminalProcessHandleBase(tbo::TerminalExecutionCallbacks callbacks,
                                      std::filesystem::path cwdFilePath)
                : m_callbacks(std::move(callbacks))
                , m_cwdFilePath(std::move(cwdFilePath))
                , m_startTime(std::chrono::steady_clock::now())
            {
            }

            [[nodiscard]] bool isRunning() const noexcept override
            {
                return m_running.load(std::memory_order_acquire);
            }

            void wait() override
            {
                if (m_waiterThread.joinable() && std::this_thread::get_id() != m_waiterThread.get_id())
                {
                    m_waiterThread.join();
                }
            }

        protected:
            void startWaiter()
            {
                m_waiterThread = std::thread([this]() { runIoAndWait(); });
            }

            void shutdownWaiterAndCleanup()
            {
                auto* self = static_cast<Derived*>(this);
                if (m_running.load(std::memory_order_acquire))
                {
                    self->terminate();
                }
                if (m_waiterThread.joinable())
                {
                    if (std::this_thread::get_id() == m_waiterThread.get_id())
                    {
                        m_waiterThread.detach();
                    }
                    else
                    {
                        m_waiterThread.join();
                    }
                }
                self->closeOsHandles();
                if (!m_cwdFilePath.empty())
                {
                    std::error_code ec;
                    std::filesystem::remove(m_cwdFilePath, ec);
                }
            }

            void emitOutput(const tbo::TerminalOutputStream stream, const std::string_view data)
            {
                if (m_callbacks.onOutput)
                {
                    std::scoped_lock cbLock(m_callbackMutex);
                    m_callbacks.onOutput(stream, data);
                }
            }

            void runIoAndWait()
            {
                auto* self = static_cast<Derived*>(this);
                std::thread outThread([self]() { self->readStreamLoop(true); });
                std::thread errThread([self]() { self->readStreamLoop(false); });

                if (outThread.joinable()) outThread.join();
                if (errThread.joinable()) errThread.join();

                const int exitCode = self->waitForExitCode();
                const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::steady_clock::now() - m_startTime);

                m_running.store(false, std::memory_order_release);

                tbo::TerminalCommandResult res;
                res.exitCode = exitCode;
                res.duration = elapsed;
                res.wasCancelled = m_cancelled.load(std::memory_order_acquire);
                res.finalWorkingDirectory = readAndCleanupCwdFile(m_cwdFilePath, !res.wasCancelled);
                m_cwdFilePath.clear();

                if (m_callbacks.onFinished)
                {
                    m_callbacks.onFinished(res);
                }
            }

            tbo::TerminalExecutionCallbacks m_callbacks;
            std::filesystem::path m_cwdFilePath;
            std::chrono::steady_clock::time_point m_startTime;
            std::atomic<bool> m_running{true};
            std::atomic<bool> m_cancelled{false};
            std::mutex m_stdinMutex;
            std::mutex m_callbackMutex;
            std::thread m_waiterThread;
        };

#ifdef _WIN32
        [[nodiscard]] std::filesystem::path findInPathWin(const wchar_t* fileName)
        {
            wchar_t buffer[MAX_PATH * 4];
            const DWORD len = SearchPathW(nullptr, fileName, nullptr,
                                          static_cast<DWORD>(std::size(buffer)), buffer, nullptr);
            if (len > 0 && len < std::size(buffer))
            {
                return std::filesystem::path(std::wstring(buffer, len));
            }
            return {};
        }

        class Win32TerminalProcessHandle final : public TerminalProcessHandleBase<Win32TerminalProcessHandle>
        {
            friend class TerminalProcessHandleBase<Win32TerminalProcessHandle>;
        public:
            Win32TerminalProcessHandle(HANDLE processHandle,
                                       HANDLE jobHandle,
                                       HANDLE stdinWrite,
                                       HANDLE stdoutRead,
                                       HANDLE stderrRead,
                                       tbo::TerminalExecutionCallbacks callbacks,
                                       std::filesystem::path cwdFilePath = {})
                : TerminalProcessHandleBase(std::move(callbacks), std::move(cwdFilePath))
                , m_process(processHandle)
                , m_job(jobHandle)
                , m_stdinWrite(stdinWrite)
                , m_stdoutRead(stdoutRead)
                , m_stderrRead(stderrRead)
            {
                startWaiter();
            }

            ~Win32TerminalProcessHandle() override { shutdownWaiterAndCleanup(); }

            bool writeStdin(const std::string_view utf8Data) override
            {
                std::scoped_lock lock(m_stdinMutex);
                if (!m_stdinWrite || m_stdinWrite == INVALID_HANDLE_VALUE || !m_running.load(std::memory_order_acquire))
                {
                    return false;
                }
                const char* ptr = utf8Data.data();
                std::size_t remaining = utf8Data.size();
                while (remaining > 0)
                {
                    const DWORD chunk = static_cast<DWORD>(std::min<std::size_t>(remaining, 32768));
                    DWORD written = 0;
                    if (!WriteFile(m_stdinWrite, ptr, chunk, &written, nullptr) || written == 0)
                    {
                        return false;
                    }
                    ptr += written;
                    remaining -= written;
                }
                return true;
            }

            void terminate() override
            {
                m_cancelled.store(true, std::memory_order_release);
                {
                    std::scoped_lock lock(m_stdinMutex);
                    if (m_stdinWrite && m_stdinWrite != INVALID_HANDLE_VALUE)
                    {
                        CloseHandle(m_stdinWrite);
                        m_stdinWrite = nullptr;
                    }
                }
                std::scoped_lock procLock(m_procMutex);
                if (m_job && m_job != INVALID_HANDLE_VALUE)
                {
                    TerminateJobObject(m_job, 130);
                }
                else if (m_process && m_process != INVALID_HANDLE_VALUE)
                {
                    TerminateProcess(m_process, 130);
                }
            }

        private:
            void readStreamLoop(const bool isStdout)
            {
                HANDLE pipe = isStdout ? m_stdoutRead : m_stderrRead;
                const auto stream = isStdout ? tbo::TerminalOutputStream::Stdout : tbo::TerminalOutputStream::Stderr;
                char buffer[4096];
                while (true)
                {
                    DWORD bytesRead = 0;
                    const BOOL ok = ReadFile(pipe, buffer, sizeof(buffer), &bytesRead, nullptr);
                    if (!ok || bytesRead == 0) break;
                    emitOutput(stream, std::string_view(buffer, bytesRead));
                }
            }

            [[nodiscard]] int waitForExitCode() const
            {
                DWORD exitCode = 1;
                if (m_process && m_process != INVALID_HANDLE_VALUE)
                {
                    WaitForSingleObject(m_process, INFINITE);
                    GetExitCodeProcess(m_process, &exitCode);
                }
                return static_cast<int>(exitCode);
            }

            void closeOsHandles()
            {
                {
                    std::scoped_lock lock(m_stdinMutex);
                    if (m_stdinWrite && m_stdinWrite != INVALID_HANDLE_VALUE) { CloseHandle(m_stdinWrite); m_stdinWrite = nullptr; }
                }
                if (m_stdoutRead && m_stdoutRead != INVALID_HANDLE_VALUE) { CloseHandle(m_stdoutRead); m_stdoutRead = nullptr; }
                if (m_stderrRead && m_stderrRead != INVALID_HANDLE_VALUE) { CloseHandle(m_stderrRead); m_stderrRead = nullptr; }
                std::scoped_lock procLock(m_procMutex);
                if (m_process && m_process != INVALID_HANDLE_VALUE) { CloseHandle(m_process); m_process = nullptr; }
                if (m_job && m_job != INVALID_HANDLE_VALUE) { CloseHandle(m_job); m_job = nullptr; }
            }

            HANDLE m_process{nullptr};
            HANDLE m_job{nullptr};
            HANDLE m_stdinWrite{nullptr};
            HANDLE m_stdoutRead{nullptr};
            HANDLE m_stderrRead{nullptr};
            std::mutex m_procMutex;
        };
#else
        class PosixTerminalProcessHandle final : public TerminalProcessHandleBase<PosixTerminalProcessHandle>
        {
            friend class TerminalProcessHandleBase<PosixTerminalProcessHandle>;
        public:
            PosixTerminalProcessHandle(const pid_t pid,
                                       const int stdinFd,
                                       const int stdoutFd,
                                       const int stderrFd,
                                       tbo::TerminalExecutionCallbacks callbacks,
                                       std::filesystem::path cwdFilePath = {})
                : TerminalProcessHandleBase(std::move(callbacks), std::move(cwdFilePath))
                , m_pid(pid)
                , m_stdinFd(stdinFd)
                , m_stdoutFd(stdoutFd)
                , m_stderrFd(stderrFd)
            {
                startWaiter();
            }

            ~PosixTerminalProcessHandle() override { shutdownWaiterAndCleanup(); }

            bool writeStdin(const std::string_view utf8Data) override
            {
                std::scoped_lock lock(m_stdinMutex);
                if (m_stdinFd < 0 || !m_running.load(std::memory_order_acquire))
                {
                    return false;
                }
                const char* ptr = utf8Data.data();
                std::size_t remaining = utf8Data.size();
                while (remaining > 0)
                {
                    const ssize_t written = ::write(m_stdinFd, ptr, remaining);
                    if (written <= 0)
                    {
                        if (written < 0 && errno == EINTR) continue;
                        return false;
                    }
                    ptr += written;
                    remaining -= static_cast<std::size_t>(written);
                }
                return true;
            }

            void terminate() override
            {
                m_cancelled.store(true, std::memory_order_release);
                {
                    std::scoped_lock lock(m_stdinMutex);
                    if (m_stdinFd >= 0) { ::close(m_stdinFd); m_stdinFd = -1; }
                }
                if (m_pid > 0 && m_running.load(std::memory_order_acquire))
                {
                    ::kill(-m_pid, SIGTERM);
                }
            }

        private:
            void readStreamLoop(const bool isStdout)
            {
                const int fd = isStdout ? m_stdoutFd : m_stderrFd;
                const auto stream = isStdout ? tbo::TerminalOutputStream::Stdout : tbo::TerminalOutputStream::Stderr;
                char buffer[4096];
                while (true)
                {
                    const ssize_t n = ::read(fd, buffer, sizeof(buffer));
                    if (n > 0)
                    {
                        emitOutput(stream, std::string_view(buffer, static_cast<std::size_t>(n)));
                    }
                    else if (n == 0 || errno != EINTR)
                    {
                        break;
                    }
                }
            }

            [[nodiscard]] int waitForExitCode() const
            {
                int status = 0;
                int exitCode = 1;
                if (m_pid > 0)
                {
                    while (::waitpid(m_pid, &status, 0) < 0 && errno == EINTR)
                    {
                    }
                    if (WIFEXITED(status)) exitCode = WEXITSTATUS(status);
                    else if (WIFSIGNALED(status)) exitCode = 128 + WTERMSIG(status);
                }
                return exitCode;
            }

            void closeOsHandles()
            {
                std::scoped_lock lock(m_stdinMutex);
                if (m_stdinFd >= 0) { ::close(m_stdinFd); m_stdinFd = -1; }
                if (m_stdoutFd >= 0) { ::close(m_stdoutFd); m_stdoutFd = -1; }
                if (m_stderrFd >= 0) { ::close(m_stderrFd); m_stderrFd = -1; }
            }

            pid_t m_pid{-1};
            int m_stdinFd{-1};
            int m_stdoutFd{-1};
            int m_stderrFd{-1};
        };
#endif

        [[nodiscard]] std::vector<tbo::TerminalShellInfo> detectShells(const std::filesystem::path& gitExe)
        {
            std::vector<tbo::TerminalShellInfo> shells;
#ifdef _WIN32
            if (const auto pwsh = findInPathWin(L"pwsh.exe"); !pwsh.empty())
            {
                shells.push_back({tbo::TerminalShellKind::PowerShell, "powershell", "PowerShell 7", pwsh});
            }
            else if (const auto winPs = findInPathWin(L"powershell.exe"); !winPs.empty())
            {
                shells.push_back({tbo::TerminalShellKind::PowerShell, "powershell", "PowerShell", winPs});
            }

            // Detect Git Bash near discovered git.exe or standard install locations
            std::vector<std::filesystem::path> bashCandidates;
            if (!gitExe.empty())
            {
                auto p = gitExe.parent_path();
                for (int i = 0; i < 4 && !p.empty(); ++i)
                {
                    bashCandidates.push_back(p / "bin" / "bash.exe");
                    bashCandidates.push_back(p / "usr" / "bin" / "bash.exe");
                    p = p.parent_path();
                }
            }
            bashCandidates.emplace_back("C:/Program Files/Git/bin/bash.exe");
            bashCandidates.emplace_back("C:/Program Files (x86)/Git/bin/bash.exe");
            for (const auto& candidate : bashCandidates)
            {
                std::error_code ec;
                if (std::filesystem::exists(candidate, ec))
                {
                    shells.push_back({tbo::TerminalShellKind::GitBash, "git-bash", "Git Bash", candidate});
                    break;
                }
            }

            if (const auto cmd = findInPathWin(L"cmd.exe"); !cmd.empty())
            {
                shells.push_back({tbo::TerminalShellKind::Cmd, "cmd", "Command Prompt", cmd});
            }
#else
            (void)gitExe;
            auto addIfExists = [&](const std::filesystem::path& p, const std::string& label)
            {
                std::error_code ec;
                if (!p.empty() && std::filesystem::exists(p, ec))
                {
                    for (const auto& existing : shells)
                    {
                        if (existing.executablePath == p) return;
                    }
                    shells.push_back({tbo::TerminalShellKind::PosixShell, p.filename().string(), label, p});
                }
            };
            if (const char* envShell = std::getenv("SHELL"); envShell && *envShell)
            {
                const std::filesystem::path sp(envShell);
                addIfExists(sp, sp.filename().string());
            }
            addIfExists("/bin/bash", "bash");
            addIfExists("/usr/bin/zsh", "zsh");
            addIfExists("/bin/zsh", "zsh");
            addIfExists("/bin/sh", "sh");
#endif
            return shells;
        }
    } // namespace

    NativeTerminalProcessRunner::NativeTerminalProcessRunner(std::filesystem::path gitExecutablePath)
        : m_gitExecutablePath(std::move(gitExecutablePath))
        , m_availableShells(detectShells(m_gitExecutablePath))
    {
    }

    std::vector<tbo::TerminalShellInfo> NativeTerminalProcessRunner::availableShells() const
    {
        return m_availableShells;
    }

    std::expected<std::shared_ptr<gf::terminal::ports::ITerminalProcessHandle>, std::string>
    NativeTerminalProcessRunner::start(const tbo::TerminalExecutionRequest& request,
                                       tbo::TerminalExecutionCallbacks callbacks)
    {
        if (request.commandLine.empty())
        {
            return std::unexpected(std::string("Comando vuoto."));
        }

        std::error_code ec;
        if (request.workingDirectory.empty() || !std::filesystem::exists(request.workingDirectory, ec))
        {
            return std::unexpected(std::string("Directory di lavoro non valida: ") +
                                   gf::core::pathToUtf8(request.workingDirectory));
        }

        std::vector<std::string> directGitArgs;
        const bool useDirectGit = isDirectGitInvocation(request.commandLine, directGitArgs) &&
                                  !m_gitExecutablePath.empty();

        std::filesystem::path cwdFilePath;
        std::vector<std::pair<std::string, std::string>> extraEnv = request.extraEnvironment;
        if (!useDirectGit)
        {
            cwdFilePath = makeCwdTempFilePath();
            if (!cwdFilePath.empty())
            {
                extraEnv.emplace_back("GF_CWD_FILE", gf::core::pathToUtf8(cwdFilePath));
            }
        }

#ifdef _WIN32
        std::wstring applicationPath;
        std::wstring cmdLine;

        if (useDirectGit)
        {
            std::vector<std::string> finalArgs;
            finalArgs.reserve(directGitArgs.size() + 4);
            appendDirectGitArgs(finalArgs, directGitArgs, request.forceGitColor);

            applicationPath = m_gitExecutablePath.wstring();
            cmdLine = process::buildWindowsCommandLine(m_gitExecutablePath, finalArgs);
        }
        else
        {
            const auto* shell = pickShell(m_availableShells, request.shellKind);
            if (!shell)
            {
                return std::unexpected(std::string("Nessuna shell disponibile sul sistema."));
            }
            applicationPath = shell->executablePath.wstring();
            process::appendQuotedWindowsArg(cmdLine, applicationPath);

            if (shell->kind == tbo::TerminalShellKind::PowerShell)
            {
                std::string psScript;
                if (!cwdFilePath.empty())
                {
                    psScript =
                        "[Console]::OutputEncoding=[System.Text.Encoding]::UTF8\n"
                        "$OutputEncoding=[System.Text.Encoding]::UTF8\n"
                        "if ($PSStyle) { $PSStyle.FileInfo.Directory = $PSStyle.Foreground.FromRgb(56,189,248) + $PSStyle.Bold }\n"
                        "$global:LASTEXITCODE = 0\n"
                        "try {\n" +
                        request.commandLine +
                        "\n$__gf_ok = $?\n"
                        "$__gf_ec = $LASTEXITCODE\n"
                        "} finally {\n"
                        "try { [System.IO.File]::WriteAllText($env:GF_CWD_FILE, (Get-Location).ProviderPath, [System.Text.UTF8Encoding]::new($false)) } catch {}\n"
                        "}\n"
                        "if (-not $__gf_ok) { if ($null -ne $__gf_ec -and $__gf_ec -ne 0) { exit $__gf_ec } else { exit 1 } }";
                }
                else
                {
                    psScript =
                        "[Console]::OutputEncoding=[System.Text.Encoding]::UTF8; "
                        "$OutputEncoding=[System.Text.Encoding]::UTF8; "
                        "if ($PSStyle) { $PSStyle.FileInfo.Directory = $PSStyle.Foreground.FromRgb(56,189,248) + $PSStyle.Bold }; " +
                        request.commandLine;
                }
                cmdLine.append(L" -NoLogo -NoProfile -NonInteractive -Command ");
                process::appendQuotedWindowsArg(cmdLine, process::utf8ToWide(psScript));
            }
            else if (shell->kind == tbo::TerminalShellKind::GitBash)
            {
                std::string bashScript = request.commandLine;
                if (!cwdFilePath.empty())
                {
                    bashScript =
                        "__gf_save_cwd() { pwd -W > \"$GF_CWD_FILE\" 2>/dev/null || pwd -P > \"$GF_CWD_FILE\" 2>/dev/null; }; "
                        "trap __gf_save_cwd EXIT\n" +
                        request.commandLine;
                }
                cmdLine.append(L" -c ");
                process::appendQuotedWindowsArg(cmdLine, process::utf8ToWide(bashScript));
            }
            else // CMD
            {
                cmdLine.append(L" /d /s /c \"chcp 65001>nul & ");
                cmdLine.append(process::utf8ToWide(request.commandLine));
                if (!cwdFilePath.empty())
                {
                    cmdLine.append(L" & if errorlevel 1 (cd>\"%GF_CWD_FILE%\" & cmd /c exit 1) else (cd>\"%GF_CWD_FILE%\")");
                }
                cmdLine.push_back(L'"');
            }
        }

        SECURITY_ATTRIBUTES sa{};
        sa.nLength = sizeof(SECURITY_ATTRIBUTES);
        sa.bInheritHandle = TRUE;

        HANDLE stdinRead = nullptr, stdinWrite = nullptr;
        HANDLE stdoutRead = nullptr, stdoutWrite = nullptr;
        HANDLE stderrRead = nullptr, stderrWrite = nullptr;

        auto cleanupPipes = [&]()
        {
            if (stdinRead) CloseHandle(stdinRead);
            if (stdinWrite) CloseHandle(stdinWrite);
            if (stdoutRead) CloseHandle(stdoutRead);
            if (stdoutWrite) CloseHandle(stdoutWrite);
            if (stderrRead) CloseHandle(stderrRead);
            if (stderrWrite) CloseHandle(stderrWrite);
        };

        if (!CreatePipe(&stdinRead, &stdinWrite, &sa, 0) ||
            !CreatePipe(&stdoutRead, &stdoutWrite, &sa, 0) ||
            !CreatePipe(&stderrRead, &stderrWrite, &sa, 0))
        {
            cleanupPipes();
            return std::unexpected(std::string("Impossibile creare le pipe di I/O del terminale."));
        }

        SetHandleInformation(stdinWrite, HANDLE_FLAG_INHERIT, 0);
        SetHandleInformation(stdoutRead, HANDLE_FLAG_INHERIT, 0);
        SetHandleInformation(stderrRead, HANDLE_FLAG_INHERIT, 0);

        HANDLE job = CreateJobObjectW(nullptr, nullptr);
        if (job)
        {
            JOBOBJECT_EXTENDED_LIMIT_INFORMATION info{};
            info.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
            SetInformationJobObject(job, JobObjectExtendedLimitInformation, &info, sizeof(info));
        }

        STARTUPINFOW si{};
        si.cb = sizeof(STARTUPINFOW);
        si.dwFlags = STARTF_USESTDHANDLES;
        si.hStdInput = stdinRead;
        si.hStdOutput = stdoutWrite;
        si.hStdError = stderrWrite;

        PROCESS_INFORMATION pi{};
        std::wstring envBlock = process::buildWindowsEnvironmentBlock(withTerminalEnvDefaults(extraEnv));
        std::wstring workDirW = request.workingDirectory.wstring();

        const BOOL created = CreateProcessW(
            applicationPath.empty() ? nullptr : applicationPath.c_str(),
            cmdLine.data(),
            nullptr,
            nullptr,
            TRUE,
            CREATE_NO_WINDOW | CREATE_SUSPENDED | CREATE_UNICODE_ENVIRONMENT,
            envBlock.data(),
            workDirW.c_str(),
            &si,
            &pi);

        // Parent no longer needs child ends of the pipes
        CloseHandle(stdinRead);
        CloseHandle(stdoutWrite);
        CloseHandle(stderrWrite);

        if (!created)
        {
            const DWORD err = GetLastError();
            CloseHandle(stdinWrite);
            CloseHandle(stdoutRead);
            CloseHandle(stderrRead);
            if (job) CloseHandle(job);
            if (!cwdFilePath.empty()) std::filesystem::remove(cwdFilePath, ec);
            return std::unexpected(std::string("Impossibile avviare il processo (Win32 error ") +
                                   std::to_string(err) + ").");
        }

        if (job)
        {
            AssignProcessToJobObject(job, pi.hProcess);
        }
        ResumeThread(pi.hThread);
        CloseHandle(pi.hThread);

        return std::make_shared<Win32TerminalProcessHandle>(
            pi.hProcess, job, stdinWrite, stdoutRead, stderrRead, std::move(callbacks), std::move(cwdFilePath));
#else
        std::vector<std::string> argvStorage;
        std::string exePath;

        if (useDirectGit)
        {
            exePath = gf::core::pathToUtf8(m_gitExecutablePath);
            argvStorage.push_back(exePath);
            appendDirectGitArgs(argvStorage, directGitArgs, request.forceGitColor);
        }
        else
        {
            const auto* shell = pickShell(m_availableShells, request.shellKind);
            if (!shell)
            {
                return std::unexpected(std::string("Nessuna shell POSIX disponibile."));
            }
            exePath = gf::core::pathToUtf8(shell->executablePath);
            argvStorage.push_back(exePath);
            argvStorage.emplace_back("-c");
            if (!cwdFilePath.empty())
            {
                argvStorage.push_back(
                    "__gf_save_cwd() { pwd -P > \"$GF_CWD_FILE\" 2>/dev/null || pwd > \"$GF_CWD_FILE\" 2>/dev/null; }; "
                    "trap __gf_save_cwd EXIT\n" +
                    request.commandLine);
            }
            else
            {
                argvStorage.push_back(request.commandLine);
            }
        }

        int stdinPipe[2] = {-1, -1};
        int stdoutPipe[2] = {-1, -1};
        int stderrPipe[2] = {-1, -1};
        if (::pipe(stdinPipe) != 0 || ::pipe(stdoutPipe) != 0 || ::pipe(stderrPipe) != 0)
        {
            for (int fd : {stdinPipe[0], stdinPipe[1], stdoutPipe[0], stdoutPipe[1], stderrPipe[0], stderrPipe[1]})
            {
                if (fd >= 0) ::close(fd);
            }
            if (!cwdFilePath.empty()) std::filesystem::remove(cwdFilePath, ec);
            return std::unexpected(std::string("Impossibile creare pipe POSIX."));
        }

        const auto envStorage = process::buildPosixEnvironment(withTerminalEnvDefaults(extraEnv));
        std::vector<char*> argvPtrs;
        argvPtrs.reserve(argvStorage.size() + 1);
        for (auto& s : argvStorage) argvPtrs.push_back(s.data());
        argvPtrs.push_back(nullptr);

        std::vector<char*> envPtrs;
        envPtrs.reserve(envStorage.size() + 1);
        for (const auto& s : envStorage) envPtrs.push_back(const_cast<char*>(s.c_str()));
        envPtrs.push_back(nullptr);

        const std::string workDirUtf8 = gf::core::pathToUtf8(request.workingDirectory);

        const pid_t pid = ::fork();
        if (pid < 0)
        {
            for (int fd : {stdinPipe[0], stdinPipe[1], stdoutPipe[0], stdoutPipe[1], stderrPipe[0], stderrPipe[1]})
            {
                ::close(fd);
            }
            if (!cwdFilePath.empty()) std::filesystem::remove(cwdFilePath, ec);
            return std::unexpected(std::string("fork() fallita."));
        }

        if (pid == 0)
        {
            ::setpgid(0, 0);
            ::dup2(stdinPipe[0], STDIN_FILENO);
            ::dup2(stdoutPipe[1], STDOUT_FILENO);
            ::dup2(stderrPipe[1], STDERR_FILENO);

            ::close(stdinPipe[0]);
            ::close(stdinPipe[1]);
            ::close(stdoutPipe[0]);
            ::close(stdoutPipe[1]);
            ::close(stderrPipe[0]);
            ::close(stderrPipe[1]);

            if (::chdir(workDirUtf8.c_str()) != 0)
            {
                _exit(126);
            }

            ::execve(exePath.c_str(), argvPtrs.data(), envPtrs.data());
            _exit(127);
        }

        ::close(stdinPipe[0]);
        ::close(stdoutPipe[1]);
        ::close(stderrPipe[1]);

        return std::make_shared<PosixTerminalProcessHandle>(
            pid, stdinPipe[1], stdoutPipe[0], stderrPipe[0], std::move(callbacks), std::move(cwdFilePath));
#endif
    }
} // namespace gf::infra::terminal
