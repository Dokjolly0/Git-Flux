#pragma once

#include <QObject>
#include <QString>

namespace gf::git::bl {
class GitStagingService;
}

namespace gf::desktop {

class CommitBoxViewModel final : public QObject {
    Q_OBJECT

    Q_PROPERTY(QString commitMessage READ commitMessage WRITE setCommitMessage NOTIFY commitMessageChanged)
    Q_PROPERTY(QString authorName READ authorName WRITE setAuthorName NOTIFY authorChanged)
    Q_PROPERTY(QString authorEmail READ authorEmail WRITE setAuthorEmail NOTIFY authorChanged)
    Q_PROPERTY(bool isAmend READ isAmend WRITE setIsAmend NOTIFY isAmendChanged)
    Q_PROPERTY(bool canCommit READ canCommit NOTIFY canCommitChanged)
    Q_PROPERTY(bool isBusy READ isBusy NOTIFY isBusyChanged)
    Q_PROPERTY(QString lastErrorMessage READ lastErrorMessage NOTIFY lastErrorMessageChanged)

public:
    explicit CommitBoxViewModel(gf::git::bl::GitStagingService& stagingService, QObject* parent = nullptr);
    ~CommitBoxViewModel() override = default;

    [[nodiscard]] QString commitMessage() const noexcept { return m_commitMessage; }
    void setCommitMessage(const QString& msg);

    [[nodiscard]] QString authorName() const noexcept { return m_authorName; }
    void setAuthorName(const QString& name);

    [[nodiscard]] QString authorEmail() const noexcept { return m_authorEmail; }
    void setAuthorEmail(const QString& email);

    [[nodiscard]] bool isAmend() const noexcept { return m_isAmend; }
    void setIsAmend(bool amend);

    [[nodiscard]] bool canCommit() const noexcept;
    [[nodiscard]] bool isBusy() const noexcept { return m_isBusy; }
    [[nodiscard]] QString lastErrorMessage() const noexcept { return m_lastErrorMessage; }

    Q_INVOKABLE bool executeCommit(const QString& repoPath);
    Q_INVOKABLE void clear();

signals:
    void commitMessageChanged();
    void authorChanged();
    void isAmendChanged();
    void canCommitChanged();
    void isBusyChanged();
    void lastErrorMessageChanged();

    void commitSucceeded(const QString& commitSha, const QString& summary);
    void commitFailed(const QString& errorMessage);

private:
    gf::git::bl::GitStagingService& m_stagingService;

    QString m_commitMessage;
    QString m_authorName;
    QString m_authorEmail;
    bool m_isAmend = false;
    bool m_isBusy = false;
    QString m_lastErrorMessage;
};

} // namespace gf::desktop
