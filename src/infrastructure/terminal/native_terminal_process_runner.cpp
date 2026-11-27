#include "infrastructure/terminal/native_terminal_process_runner.hpp"

#include "core/modules/terminal/bl/command_line_tokenizer.hpp"
#include "core/shared/utf8_path.hpp"

#include <algorithm>
#include <atomic>
#include <cctype>
#include <chrono>
#include <cstdlib>
#include <mutex>
#include <thread>
#include <unordered_map>

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

namespace gf::infra::terminal
{
    namespace
    {
        namespace tbo = gf::terminal::bo;

        [[nodiscard]] std::string toLowerCopy(const std::string_view sv)
        {
            std::string out;
            out.reserve(sv.size());
            for (const char c : sv)
            {
                out.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
            }
            return out;
        }

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
            const std::string cmd0 = toLowerCopy(rawArgs[0]);
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

#ifdef _WIN32
        [[nodiscard]] std::wstring utf8ToWide(const std::string_view utf8)
        {
            if (utf8.empty()) return {};
            const int needed = MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()), nullptr, 0);
            if (needed <= 0) return {};
            std::wstring result(static_cast<std::size_t>(needed), L'\0');
            MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()), result.data(), needed);
            return result;
        }

        [[nodiscard]] std::string wideToUtf8(const std::wstring_view wide)
        {
            if (wide.empty()) return {};
            const int needed = WideCharToMultiByte(CP_UTF8, 0, wide.data(), static_cast<int>(wide.size()),
                                                   nullptr, 0, nullptr, nullptr);
            if (needed <= 0) return {};
            std::string result(static_cast<std::size_t>(needed), '\0');
            WideCharToMultiByte(CP_UTF8, 0, wide.data(), static_cast<int>(wide.size()),
                                result.data(), needed, nullptr, nullptr);
            return result;
        }

        void appendQuotedWindowsArg(std::wstring& cmdLine, const std::wstring_view arg)
        {
            const bool needsQuotes = arg.empty() || arg.find_first_of(L" \t\n\v\"") != std::wstring_view::npos;
            if (!needsQuotes)
            {
                cmdLine.append(arg);
                return;
            }
            cmdLine.push_back(L'"');
            for (std::size_t i = 0; i < arg.size();)
            {
                std::size_t backslashes = 0;
                while (i < arg.size() && arg[i] == L'\\')
                {
                    ++backslashes;
                    ++i;
                }
                if (i == arg.size())
                {
                    cmdLine.append(backslashes * 2, L'\\');
                    break;
                }
                if (arg[i] == L'"')
                {
                    cmdLine.append(backslashes * 2 + 1, L'\\');
                    cmdLine.push_back(L'"');
                    ++i;
                }
                else
                {
                    cmdLine.append(backslashes, L'\\');
                    cmdLine.push_back(arg[i]);
                    ++i;
                }
            }
            cmdLine.push_back(L'"');
        }

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

        [[nodiscard]] std::wstring buildWindowsEnvironmentBlock(
            const std::vector<std::pair<std::string, std::string>>& overrides)
        {
            std::vector<std::pair<std::wstring, std::wstring>> entries;
            std::unordered_map<std::wstring, std::size_t> keyIndex;

            auto toUpperW = [](const std::wstring_view s)
            {
                std::wstring out;
                out.reserve(s.size());
                for (const wchar_t c : s)
                {
                    out.push_back(static_cast<wchar_t>(towupper(c)));
                }
                return out;
            };

            if (LPWCH envStrings = GetEnvironmentStringsW(); envStrings != nullptr)
            {
                const wchar_t* ptr = envStrings;
                while (*ptr != L'\0')
                {
                    const std::wstring_view entry(ptr);
                    ptr += entry.size() + 1;
                    const std::size_t eq = entry.find(L'=', 1);
                    if (eq == std::wstring_view::npos) continue;
                    std::wstring key(entry.substr(0, eq));
                    std::wstring val(entry.substr(eq + 1));
                    const std::wstring upper = toUpperW(key);
                    keyIndex[upper] = entries.size();
                    entries.emplace_back(std::move(key), std::move(val));
                }
                FreeEnvironmentStringsW(envStrings);
            }

            auto setVar = [&](const std::wstring& key, const std::wstring& val)
            {
                const std::wstring upper = toUpperW(key);
                if (const auto it = keyIndex.find(upper); it != keyIndex.end())
                {
                    entries[it->second].second = val;
                }
                else
                {
                    keyIndex[upper] = entries.size();
                    entries.emplace_back(key, val);
                }
            };

            setVar(L"GIT_PAGER", L"cat");
            setVar(L"PAGER", L"cat");
            setVar(L"TERM", L"xterm-256color");
            setVar(L"CLICOLOR_FORCE", L"1");
            setVar(L"FORCE_COLOR", L"1");

            for (const auto& [k, v] : overrides)
            {
                setVar(utf8ToWide(k), utf8ToWide(v));
            }

            std::ranges::sort(entries, [&](const auto& a, const auto& b)
            {
                return toUpperW(a.first) < toUpperW(b.first);
            });

            std::wstring block;
            for (const auto& [k, v] : entries)
            {
                block.append(k);
                block.push_back(L'=');
                block.append(v);
                block.push_back(L'\0');
            }
            block.push_back(L'\0');
            return block;
        }

        class Win32TerminalProcessHandle final : public gf::terminal::ports::ITerminalProcessHandle
        {
        public:
            Win32TerminalProcessHandle(HANDLE processHandle,
                                       HANDLE jobHandle,
                                       HANDLE stdinWrite,
                                       HANDLE stdoutRead,
                                       HANDLE stderrRead,
                                       tbo::TerminalExecutionCallbacks callbacks)
                : m_process(processHandle)
                , m_job(jobHandle)
                , m_stdinWrite(stdinWrite)
                , m_stdoutRead(stdoutRead)
                , m_stderrRead(stderrRead)
                , m_callbacks(std::move(callbacks))
                , m_startTime(std::chrono::steady_clock::now())
            {
                m_waiterThread = std::thread([this]() { runIoAndWait(); });
            }

            ~Win32TerminalProcessHandle() override
            {
                if (m_running.load(std::memory_order_acquire))
                {
                    terminate();
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
                closeHandles();
            }

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

        private:
            void readPipeLoop(HANDLE pipe, const tbo::TerminalOutputStream stream)
            {
                char buffer[4096];
                while (true)
                {
                    DWORD bytesRead = 0;
                    const BOOL ok = ReadFile(pipe, buffer, sizeof(buffer), &bytesRead, nullptr);
                    if (!ok || bytesRead == 0)
                    {
                        break;
                    }
                    if (m_callbacks.onOutput)
                    {
                        std::scoped_lock cbLock(m_callbackMutex);
                        m_callbacks.onOutput(stream, std::string_view(buffer, bytesRead));
                    }
                }
            }

            void runIoAndWait()
            {
                std::thread outThread([this]() { readPipeLoop(m_stdoutRead, tbo::TerminalOutputStream::Stdout); });
                std::thread errThread([this]() { readPipeLoop(m_stderrRead, tbo::TerminalOutputStream::Stderr); });

                if (outThread.joinable()) outThread.join();
                if (errThread.joinable()) errThread.join();

                DWORD exitCode = 1;
                if (m_process && m_process != INVALID_HANDLE_VALUE)
                {
                    WaitForSingleObject(m_process, INFINITE);
                    GetExitCodeProcess(m_process, &exitCode);
                }

                const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::steady_clock::now() - m_startTime);

                m_running.store(false, std::memory_order_release);

                tbo::TerminalCommandResult res;
                res.exitCode = static_cast<int>(exitCode);
                res.duration = elapsed;
                res.wasCancelled = m_cancelled.load(std::memory_order_acquire);

                if (m_callbacks.onFinished)
                {
                    m_callbacks.onFinished(res);
                }
            }

            void closeHandles()
            {
                {
                    std::scoped_lock lock(m_stdinMutex);
                    if (m_stdinWrite && m_stdinWrite != INVALID_HANDLE_VALUE)
                    {
                        CloseHandle(m_stdinWrite);
                        m_stdinWrite = nullptr;
                    }
                }
                if (m_stdoutRead && m_stdoutRead != INVALID_HANDLE_VALUE)
                {
                    CloseHandle(m_stdoutRead);
                    m_stdoutRead = nullptr;
                }
                if (m_stderrRead && m_stderrRead != INVALID_HANDLE_VALUE)
                {
                    CloseHandle(m_stderrRead);
                    m_stderrRead = nullptr;
                }
                std::scoped_lock procLock(m_procMutex);
                if (m_process && m_process != INVALID_HANDLE_VALUE)
                {
                    CloseHandle(m_process);
                    m_process = nullptr;
                }
                if (m_job && m_job != INVALID_HANDLE_VALUE)
                {
                    CloseHandle(m_job);
                    m_job = nullptr;
                }
            }

            HANDLE m_process{nullptr};
            HANDLE m_job{nullptr};
            HANDLE m_stdinWrite{nullptr};
            HANDLE m_stdoutRead{nullptr};
            HANDLE m_stderrRead{nullptr};
            tbo::TerminalExecutionCallbacks m_callbacks;
            std::chrono::steady_clock::time_point m_startTime;
            std::atomic<bool> m_running{true};
            std::atomic<bool> m_cancelled{false};
            std::mutex m_stdinMutex;
            std::mutex m_procMutex;
            std::mutex m_callbackMutex;
            std::thread m_waiterThread;
        };
#else
        [[nodiscard]] std::vector<std::string> buildPosixEnvironment(
            const std::vector<std::pair<std::string, std::string>>& overrides)
        {
            std::unordered_map<std::string, std::string> envMap;
            if (environ != nullptr)
            {
                for (char** p = environ; *p != nullptr; ++p)
                {
                    const std::string_view entry(*p);
                    if (const std::size_t eq = entry.find('='); eq != std::string_view::npos)
                    {
                        envMap[std::string(entry.substr(0, eq))] = std::string(entry.substr(eq + 1));
                    }
                }
            }
            envMap["GIT_PAGER"] = "cat";
            envMap["PAGER"] = "cat";
            envMap["TERM"] = "xterm-256color";
            envMap["CLICOLOR_FORCE"] = "1";
            envMap["FORCE_COLOR"] = "1";
            for (const auto& [k, v] : overrides)
            {
                envMap[k] = v;
            }

            std::vector<std::string> result;
            result.reserve(envMap.size());
            for (const auto& [k, v] : envMap)
            {
                result.push_back(k + "=" + v);
            }
            return result;
        }

        class PosixTerminalProcessHandle final : public gf::terminal::ports::ITerminalProcessHandle
        {
        public:
            PosixTerminalProcessHandle(const pid_t pid,
                                       const int stdinFd,
                                       const int stdoutFd,
                                       const int stderrFd,
                                       tbo::TerminalExecutionCallbacks callbacks)
                : m_pid(pid)
                , m_stdinFd(stdinFd)
                , m_stdoutFd(stdoutFd)
                , m_stderrFd(stderrFd)
                , m_callbacks(std::move(callbacks))
                , m_startTime(std::chrono::steady_clock::now())
            {
                m_waiterThread = std::thread([this]() { runIoAndWait(); });
            }

            ~PosixTerminalProcessHandle() override
            {
                if (m_running.load(std::memory_order_acquire))
                {
                    terminate();
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
                closeFds();
            }

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
                    if (m_stdinFd >= 0)
                    {
                        ::close(m_stdinFd);
                        m_stdinFd = -1;
                    }
                }
                if (m_pid > 0 && m_running.load(std::memory_order_acquire))
                {
                    ::kill(-m_pid, SIGTERM);
                }
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

        private:
            void readFdLoop(const int fd, const tbo::TerminalOutputStream stream)
            {
                char buffer[4096];
                while (true)
                {
                    const ssize_t n = ::read(fd, buffer, sizeof(buffer));
                    if (n > 0)
                    {
                        if (m_callbacks.onOutput)
                        {
                            std::scoped_lock cbLock(m_callbackMutex);
                            m_callbacks.onOutput(stream, std::string_view(buffer, static_cast<std::size_t>(n)));
                        }
                    }
                    else if (n == 0)
                    {
                        break;
                    }
                    else if (errno != EINTR)
                    {
                        break;
                    }
                }
            }

            void runIoAndWait()
            {
                std::thread outThread([this]() { readFdLoop(m_stdoutFd, tbo::TerminalOutputStream::Stdout); });
                std::thread errThread([this]() { readFdLoop(m_stderrFd, tbo::TerminalOutputStream::Stderr); });

                if (outThread.joinable()) outThread.join();
                if (errThread.joinable()) errThread.join();

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

                const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::steady_clock::now() - m_startTime);

                m_running.store(false, std::memory_order_release);

                tbo::TerminalCommandResult res;
                res.exitCode = exitCode;
                res.duration = elapsed;
                res.wasCancelled = m_cancelled.load(std::memory_order_acquire);

                if (m_callbacks.onFinished)
                {
                    m_callbacks.onFinished(res);
                }
            }

            void closeFds()
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
            tbo::TerminalExecutionCallbacks m_callbacks;
            std::chrono::steady_clock::time_point m_startTime;
            std::atomic<bool> m_running{true};
            std::atomic<bool> m_cancelled{false};
            std::mutex m_stdinMutex;
            std::mutex m_callbackMutex;
            std::thread m_waiterThread;
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

#ifdef _WIN32
        std::wstring applicationPath;
        std::wstring cmdLine;

        if (useDirectGit)
        {
            std::vector<std::string> finalArgs;
            finalArgs.reserve(directGitArgs.size() + 4);
            finalArgs.emplace_back("-c");
            finalArgs.emplace_back("core.quotepath=false");
            if (request.forceGitColor && !hasExplicitColorFlag(directGitArgs))
            {
                finalArgs.emplace_back("-c");
                finalArgs.emplace_back("color.ui=always");
            }
            for (auto& a : directGitArgs)
            {
                finalArgs.push_back(std::move(a));
            }

            applicationPath = m_gitExecutablePath.wstring();
            appendQuotedWindowsArg(cmdLine, applicationPath);
            for (const auto& a : finalArgs)
            {
                cmdLine.push_back(L' ');
                appendQuotedWindowsArg(cmdLine, utf8ToWide(a));
            }
        }
        else
        {
            const auto* shell = pickShell(m_availableShells, request.shellKind);
            if (!shell)
            {
                return std::unexpected(std::string("Nessuna shell disponibile sul sistema."));
            }
            applicationPath = shell->executablePath.wstring();
            appendQuotedWindowsArg(cmdLine, applicationPath);

            if (shell->kind == tbo::TerminalShellKind::PowerShell)
            {
                const std::string psScript =
                    "[Console]::OutputEncoding=[System.Text.Encoding]::UTF8; "
                    "$OutputEncoding=[System.Text.Encoding]::UTF8; "
                    "if ($PSStyle) { $PSStyle.FileInfo.Directory = $PSStyle.Foreground.FromRgb(56,189,248) + $PSStyle.Bold }; " +
                    request.commandLine;
                cmdLine.append(L" -NoLogo -NoProfile -NonInteractive -Command ");
                appendQuotedWindowsArg(cmdLine, utf8ToWide(psScript));
            }
            else if (shell->kind == tbo::TerminalShellKind::GitBash)
            {
                cmdLine.append(L" -c ");
                appendQuotedWindowsArg(cmdLine, utf8ToWide(request.commandLine));
            }
            else // CMD
            {
                cmdLine.append(L" /d /s /c \"chcp 65001>nul & ");
                cmdLine.append(utf8ToWide(request.commandLine));
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
        std::wstring envBlock = buildWindowsEnvironmentBlock(request.extraEnvironment);
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
            pi.hProcess, job, stdinWrite, stdoutRead, stderrRead, std::move(callbacks));
#else
        std::vector<std::string> argvStorage;
        std::string exePath;

        if (useDirectGit)
        {
            exePath = gf::core::pathToUtf8(m_gitExecutablePath);
            argvStorage.push_back(exePath);
            argvStorage.emplace_back("-c");
            argvStorage.emplace_back("core.quotepath=false");
            if (request.forceGitColor && !hasExplicitColorFlag(directGitArgs))
            {
                argvStorage.emplace_back("-c");
                argvStorage.emplace_back("color.ui=always");
            }
            for (auto& a : directGitArgs)
            {
                argvStorage.push_back(std::move(a));
            }
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
            argvStorage.push_back(request.commandLine);
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
            return std::unexpected(std::string("Impossibile creare pipe POSIX."));
        }

        const auto envStorage = buildPosixEnvironment(request.extraEnvironment);
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
            pid, stdinPipe[1], stdoutPipe[0], stderrPipe[0], std::move(callbacks));
#endif
    }
} // namespace gf::infra::terminal
