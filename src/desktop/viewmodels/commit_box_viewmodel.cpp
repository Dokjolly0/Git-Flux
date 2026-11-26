#include "desktop/viewmodels/commit_box_viewmodel.hpp"

#include "core/modules/git/bl/git_staging_service.hpp"
#include "core/shared/utf8_path.hpp"
#include "desktop/services/serial_git_executor.hpp"

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QThread>

#include <expected>
#include <filesystem>

namespace gf::desktop
{
    CommitBoxViewModel::CommitBoxViewModel(gf::git::bl::GitStagingService& stagingService, QObject* parent)
        : QObject(parent)
          , m_stagingService(stagingService)
          , m_ownedExecutor(std::make_unique<SerialGitExecutor>())
    {
        m_executor = m_ownedExecutor.get();
    }

    CommitBoxViewModel::CommitBoxViewModel(gf::git::bl::GitStagingService& stagingService,
                                           SerialGitExecutor& executor,
                                           QObject* parent)
        : QObject(parent)
          , m_stagingService(stagingService)
          , m_executor(&executor)
    {
    }

    CommitBoxViewModel::~CommitBoxViewModel() = default;

    void CommitBoxViewModel::setCommitMessage(const QString& msg)
    {
        if (m_commitMessage != msg)
        {
            m_commitMessage = msg;
            emit commitMessageChanged();
            emit canCommitChanged();
        }
    }

    void CommitBoxViewModel::setAuthorName(const QString& name)
    {
        if (m_authorName != name)
        {
            m_authorName = name;
            emit authorChanged();
        }
    }

    void CommitBoxViewModel::setAuthorEmail(const QString& email)
    {
        if (m_authorEmail != email)
        {
            m_authorEmail = email;
            emit authorChanged();
        }
    }

    void CommitBoxViewModel::setIsAmend(bool amend)
    {
        if (m_isAmend != amend)
        {
            m_isAmend = amend;
            emit isAmendChanged();
        }
    }

    bool CommitBoxViewModel::canCommit() const noexcept
    {
        return !m_commitMessage.trimmed().isEmpty() && !m_isBusy;
    }

    bool CommitBoxViewModel::executeCommit(const QString& repoPath)
    {
        if (!canCommit() || repoPath.trimmed().isEmpty())
        {
            return false;
        }

        m_isBusy = true;
        emit isBusyChanged();
        emit canCommitChanged();

        gf::git::bo::CommitOptions options;
        options.message = m_commitMessage.toStdString();
        options.amend = m_isAmend;

        if (!m_authorName.trimmed().isEmpty() && !m_authorEmail.trimmed().isEmpty())
        {
            options.author = gf::git::bo::CommitAuthor{
                .name = m_authorName.toStdString(),
                .email = m_authorEmail.toStdString()
            };
        }

        const auto rPath = gf::core::utf8ToPath(repoPath.toStdString());
        const QString committedMessage = m_commitMessage;
        auto& staging = m_stagingService;

        m_executor->run(this,
                        [&staging, rPath, options]()
                        {
                            return staging.createCommit(rPath, options);
                        },
                        [this, committedMessage](
                        std::expected<gf::git::bo::CommitResult, gf::git::ports::GitOpsError> result)
                        {
                            m_isBusy = false;
                            emit isBusyChanged();
                            emit canCommitChanged();

                            if (!result)
                            {
                                m_lastErrorMessage = QString::fromStdString(result.error().message);
                                emit lastErrorMessageChanged();
                                emit commitFailed(m_lastErrorMessage);
                                return;
                            }

                            m_lastErrorMessage.clear();
                            emit lastErrorMessageChanged();

                            // Reset the message unless the user already started a new one meanwhile.
                            if (m_commitMessage == committedMessage)
                            {
                                m_commitMessage.clear();
                                emit commitMessageChanged();
                            }
                            m_isAmend = false;
                            emit isAmendChanged();
                            emit canCommitChanged();

                            emit commitSucceeded(QString::fromStdString(result->commitSha),
                                                 QString::fromStdString(result->summary));
                        });
        return true;
    }

    bool CommitBoxViewModel::waitForIdle(int timeoutMs)
    {
        QElapsedTimer timer;
        timer.start();
        while (m_isBusy)
        {
            if (timer.elapsed() > timeoutMs)
            {
                return false;
            }
            QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
            QThread::msleep(1);
        }
        return true;
    }

    void CommitBoxViewModel::clear()
    {
        m_commitMessage.clear();
        m_isAmend = false;
        m_lastErrorMessage.clear();
        emit commitMessageChanged();
        emit isAmendChanged();
        emit canCommitChanged();
        emit lastErrorMessageChanged();
    }
} // namespace gf::desktop
