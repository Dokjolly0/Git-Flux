#include "infrastructure/git/qt/qt_git_batch_session.hpp"
#include "core/shared/utf8_path.hpp"

#include <QByteArray>
#include <QProcess>
#include <QStringList>

#include <charconv>

namespace gf::infra::git
{
    namespace
    {
        using gf::git::bo::GitErrorKind;
        using gf::git::bo::GitExecutionError;
        using gf::git::ports::GitBatchObject;
        using gf::git::ports::IGitBatchSession;

        [[nodiscard]] QString toQString(const std::filesystem::path& p)
        {
            const std::string utf8 = gf::core::pathToUtf8(p);
            return QString::fromUtf8(utf8.data(), static_cast<qsizetype>(utf8.size()));
        }

        class QtGitBatchSessionImpl final : public IGitBatchSession
        {
        public:
            explicit QtGitBatchSessionImpl(std::unique_ptr<QProcess> proc) noexcept
                : proc_(std::move(proc))
            {
            }

            ~QtGitBatchSessionImpl() override
            {
                if (proc_ && proc_->state() != QProcess::NotRunning)
                {
                    proc_->closeWriteChannel();
                    if (!proc_->waitForFinished(1000))
                    {
                        proc_->kill();
                        proc_->waitForFinished(1000);
                    }
                }
            }

            [[nodiscard]] bool isAlive() const noexcept override
            {
                return proc_ && proc_->state() == QProcess::Running;
            }

            [[nodiscard]] std::expected<GitBatchObject, GitExecutionError>
            readObject(std::string_view revSpec) override
            {
                if (!isAlive())
                {
                    return std::unexpected(GitExecutionError{
                        .kind = GitErrorKind::PipeIoError,
                        .message = "QProcess cat-file --batch non in esecuzione"
                    });
                }

                QByteArray req;
                req.reserve(static_cast<qsizetype>(revSpec.size() + 1));
                req.append(revSpec.data(), static_cast<qsizetype>(revSpec.size()));
                req.append('\n');

                if (proc_->write(req) != req.size() || !proc_->waitForBytesWritten(5000))
                {
                    return std::unexpected(GitExecutionError{
                        .kind = GitErrorKind::PipeIoError,
                        .message = "Scrittura QProcess fallita su cat-file --batch"
                    });
                }

                while (!proc_->canReadLine())
                {
                    if (!proc_->waitForReadyRead(5000))
                    {
                        return std::unexpected(GitExecutionError{
                            .kind = GitErrorKind::PipeIoError,
                            .message = "Timeout attesa header da QProcess cat-file --batch"
                        });
                    }
                }

                QByteArray lineBytes = proc_->readLine();
                while (!lineBytes.isEmpty() && (lineBytes.endsWith('\n') || lineBytes.endsWith('\r')))
                {
                    lineBytes.chop(1);
                }
                const std::string_view headerView(lineBytes.constData(), static_cast<std::size_t>(lineBytes.size()));

                if (headerView.ends_with(" missing"))
                {
                    return std::unexpected(GitExecutionError{
                        .kind = GitErrorKind::ProtocolParseError,
                        .message = "Oggetto Git mancante: " + std::string(headerView)
                    });
                }

                const std::size_t firstSpace = headerView.find(' ');
                const std::size_t secondSpace = (firstSpace == std::string_view::npos)
                                                    ? std::string_view::npos
                                                    : headerView.find(' ', firstSpace + 1);
                if (firstSpace == std::string_view::npos || secondSpace == std::string_view::npos)
                {
                    return std::unexpected(GitExecutionError{
                        .kind = GitErrorKind::ProtocolParseError,
                        .message = "Header QProcess cat-file non valido: " + std::string(headerView)
                    });
                }

                GitBatchObject obj;
                obj.header.oid = std::string(headerView.substr(0, firstSpace));
                obj.header.type = std::string(headerView.substr(firstSpace + 1, secondSpace - firstSpace - 1));

                const std::string_view sizeStr = headerView.substr(secondSpace + 1);
                auto [ptr, ec] = std::from_chars(sizeStr.data(), sizeStr.data() + sizeStr.size(), obj.header.size);
                if (ec != std::errc{})
                {
                    return std::unexpected(GitExecutionError{
                        .kind = GitErrorKind::ProtocolParseError,
                        .message = "Dimensione non valida in QProcess cat-file"
                    });
                }

                const qsizetype needed = static_cast<qsizetype>(obj.header.size + 1); // +1 for trailing '\n'
                while (proc_->bytesAvailable() < needed)
                {
                    if (!proc_->waitForReadyRead(5000))
                    {
                        return std::unexpected(GitExecutionError{
                            .kind = GitErrorKind::PipeIoError,
                            .message = "Payload incompleto da QProcess cat-file --batch"
                        });
                    }
                }

                obj.content.resize(obj.header.size);
                if (obj.header.size > 0)
                {
                    const qint64 r = proc_->read(obj.content.data(), static_cast<qint64>(obj.header.size));
                    if (r != static_cast<qint64>(obj.header.size))
                    {
                        return std::unexpected(GitExecutionError{
                            .kind = GitErrorKind::PipeIoError,
                            .message = "Lettura incompleta payload QProcess"
                        });
                    }
                }

                char trailingLf = '\0';
                if (proc_->read(&trailingLf, 1) != 1 || trailingLf != '\n')
                {
                    return std::unexpected(GitExecutionError{
                        .kind = GitErrorKind::ProtocolParseError,
                        .message = "LF finale mancante in QProcess cat-file"
                    });
                }

                return obj;
            }

        private:
            std::unique_ptr<QProcess> proc_;
        };
    } // namespace

    QtGitBatchSessionFactory::QtGitBatchSessionFactory(std::filesystem::path gitExecutablePath,
                                                       std::string label)
        : gitExecutablePath_(std::move(gitExecutablePath))
          , programQt_(toQString(gitExecutablePath_))
          , label_(std::move(label))
    {
    }

    std::expected<std::unique_ptr<gf::git::ports::IGitBatchSession>, gf::git::bo::GitExecutionError>
    QtGitBatchSessionFactory::openSession(const std::filesystem::path& repositoryPath)
    {
        std::error_code ec;
        if (!std::filesystem::exists(gitExecutablePath_, ec))
        {
            return std::unexpected(GitExecutionError{
                .kind = GitErrorKind::ExecutableNotFound,
                .message = "Eseguibile Git non trovato"
            });
        }
        if (!std::filesystem::exists(repositoryPath, ec))
        {
            return std::unexpected(GitExecutionError{
                .kind = GitErrorKind::WorkingDirectoryInvalid,
                .message = "Repository inesistente"
            });
        }

        auto proc = std::make_unique<QProcess>();
        proc->setProgram(programQt_);
        proc->setArguments({
            QStringLiteral("--no-optional-locks"),
            QStringLiteral("cat-file"),
            QStringLiteral("--batch")
        });
        proc->setWorkingDirectory(toQString(repositoryPath));
        proc->setStandardErrorFile(QProcess::nullDevice());

        proc->start(QIODevice::ReadWrite);
        if (!proc->waitForStarted(5000))
        {
            return std::unexpected(GitExecutionError{
                .kind = GitErrorKind::SpawnFailed,
                .message = "Avvio QProcess cat-file --batch fallito: " + proc->errorString().toStdString(),
                .osErrorCode = static_cast<int>(proc->error())
            });
        }

        return std::make_unique<QtGitBatchSessionImpl>(std::move(proc));
    }
} // namespace gf::infra::git
