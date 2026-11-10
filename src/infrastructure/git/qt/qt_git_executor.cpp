#include "infrastructure/git/qt/qt_git_executor.hpp"
#include "core/shared/utf8_path.hpp"

#include <QByteArray>
#include <QProcess>
#include <QStringList>

#include <chrono>

namespace gf::infra::git
{
    namespace
    {
        [[nodiscard]] QString pathToQString(const std::filesystem::path& p)
        {
            const std::string utf8 = gf::core::pathToUtf8(p);
            return QString::fromUtf8(utf8.data(), static_cast<qsizetype>(utf8.size()));
        }

        [[nodiscard]] QString utf8ToQString(std::string_view utf8)
        {
            return QString::fromUtf8(utf8.data(), static_cast<qsizetype>(utf8.size()));
        }
    } // namespace

    QtGitCommandExecutor::QtGitCommandExecutor(std::filesystem::path gitExecutablePath,
                                               std::string label)
        : gitExecutablePath_(std::move(gitExecutablePath))
          , programQt_(pathToQString(gitExecutablePath_))
          , label_(std::move(label))
          , envStandard_(QProcessEnvironment::systemEnvironment())
    {
        envStandard_.insert(QStringLiteral("GIT_TERMINAL_PROMPT"), QStringLiteral("0"));
        envStandard_.insert(QStringLiteral("GCM_INTERACTIVE"), QStringLiteral("never"));
        envStandard_.insert(QStringLiteral("LC_ALL"), QStringLiteral("C"));
        envStandard_.insert(QStringLiteral("GIT_EDITOR"), QStringLiteral("true"));

        envNoOptionalLocks_ = envStandard_;
        envNoOptionalLocks_.insert(QStringLiteral("GIT_OPTIONAL_LOCKS"), QStringLiteral("0"));
    }

    std::expected<gf::git::bo::GitProcessResult, gf::git::bo::GitExecutionError>
    QtGitCommandExecutor::execute(const gf::git::bo::GitCommand& command,
                                  std::stop_token stopToken)
    {
        using gf::git::bo::GitErrorKind;
        using gf::git::bo::GitExecutionError;
        using gf::git::bo::GitLockPolicy;
        using gf::git::bo::GitProcessResult;

        const auto startClock = std::chrono::steady_clock::now();

        std::error_code fsEc;
        if (gitExecutablePath_.empty() || !std::filesystem::exists(gitExecutablePath_, fsEc))
        {
            return std::unexpected(GitExecutionError{
                .kind = GitErrorKind::ExecutableNotFound,
                .message = "Eseguibile Git non trovato: " + gf::core::pathToUtf8(gitExecutablePath_)
            });
        }
        if (!command.workingDirectory.empty() && !std::filesystem::exists(command.workingDirectory, fsEc))
        {
            return std::unexpected(GitExecutionError{
                .kind = GitErrorKind::WorkingDirectoryInvalid,
                .message = "Working directory inesistente: " + gf::core::pathToUtf8(command.workingDirectory)
            });
        }

        const bool noOptLocks = (command.lockPolicy == GitLockPolicy::NoOptionalLocks);

        QStringList qtArgs;
        qtArgs.reserve(static_cast<qsizetype>(command.args.size() + (noOptLocks ? 1 : 0)));
        if (noOptLocks)
        {
            qtArgs.append(QStringLiteral("--no-optional-locks"));
        }
        for (const auto& arg : command.args)
        {
            qtArgs.append(utf8ToQString(arg));
        }

        QProcess proc;
        proc.setProgram(programQt_);
        proc.setArguments(qtArgs);
        if (!command.workingDirectory.empty())
        {
            proc.setWorkingDirectory(pathToQString(command.workingDirectory));
        }
        proc.setProcessEnvironment(noOptLocks ? envNoOptionalLocks_ : envStandard_);
        proc.setStandardInputFile(QProcess::nullDevice());

        proc.start(QIODevice::ReadOnly);
        if (!proc.waitForStarted(5000))
        {
            return std::unexpected(GitExecutionError{
                .kind = GitErrorKind::SpawnFailed,
                .message = "QProcess::start fallita: " + proc.errorString().toStdString(),
                .osErrorCode = static_cast<int>(proc.error())
            });
        }

        GitProcessResult result;
        const auto deadline = startClock + command.timeout;

        while (proc.state() != QProcess::NotRunning)
        {
            if (stopToken.stop_requested())
            {
                result.cancelled = true;
                proc.kill();
                proc.waitForFinished(2000);
                break;
            }
            const auto now = std::chrono::steady_clock::now();
            if (now >= deadline)
            {
                result.timedOut = true;
                proc.kill();
                proc.waitForFinished(2000);
                break;
            }
            const auto remMs = std::chrono::duration_cast<std::chrono::milliseconds>(deadline - now).count();
            const int sliceMs = stopToken.stop_possible()
                                    ? static_cast<int>(std::min<long long>(remMs, 25))
                                    : static_cast<int>(remMs);

            if (proc.waitForFinished(sliceMs))
            {
                break;
            }
            const QByteArray outChunk = proc.readAllStandardOutput();
            if (!outChunk.isEmpty())
            {
                result.stdoutBytes.append(outChunk.constData(), static_cast<std::size_t>(outChunk.size()));
            }
            const QByteArray errChunk = proc.readAllStandardError();
            if (!errChunk.isEmpty())
            {
                result.stderrBytes.append(errChunk.constData(), static_cast<std::size_t>(errChunk.size()));
            }
        }

        const QByteArray finalOut = proc.readAllStandardOutput();
        if (!finalOut.isEmpty())
        {
            result.stdoutBytes.append(finalOut.constData(), static_cast<std::size_t>(finalOut.size()));
        }
        const QByteArray finalErr = proc.readAllStandardError();
        if (!finalErr.isEmpty())
        {
            result.stderrBytes.append(finalErr.constData(), static_cast<std::size_t>(finalErr.size()));
        }

        result.exitCode = (proc.exitStatus() == QProcess::NormalExit) ? proc.exitCode() : -1;
        result.elapsed = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now() - startClock);
        return result;
    }
} // namespace gf::infra::git
