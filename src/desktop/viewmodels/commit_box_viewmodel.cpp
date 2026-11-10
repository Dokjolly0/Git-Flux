#include "desktop/viewmodels/commit_box_viewmodel.hpp"

#include "core/modules/git/bl/git_staging_service.hpp"

#include <filesystem>

namespace gf::desktop
{
    CommitBoxViewModel::CommitBoxViewModel(gf::git::bl::GitStagingService& stagingService, QObject* parent)
        : QObject(parent)
          , m_stagingService(stagingService)
    {
    }

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

        std::filesystem::path rPath(repoPath.toStdString());
        auto result = m_stagingService.createCommit(rPath, options);

        m_isBusy = false;
        emit isBusyChanged();
        emit canCommitChanged();

        if (!result)
        {
            m_lastErrorMessage = QString::fromStdString(result.error().message);
            emit lastErrorMessageChanged();
            emit commitFailed(m_lastErrorMessage);
            return false;
        }

        m_lastErrorMessage.clear();
        emit lastErrorMessageChanged();

        QString sha = QString::fromStdString(result->commitSha);
        QString summary = QString::fromStdString(result->summary);

        // Reset commit message upon successful commit
        m_commitMessage.clear();
        m_isAmend = false;
        emit commitMessageChanged();
        emit isAmendChanged();
        emit canCommitChanged();

        emit commitSucceeded(sha, summary);
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
