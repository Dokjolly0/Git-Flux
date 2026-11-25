#pragma once

#include "core/modules/git/bl/remote_access_diagnostics.hpp"

#include <QObject>
#include <QString>
#include <QStringList>

namespace gf::desktop::services
{
    /// Checks whether an SSH key is accepted by a Git server by running
    ///   ssh -T -o BatchMode=yes -o ConnectTimeout=N [-p port] [-i key] git@<target>
    /// in the background. Never prompts and never touches known_hosts (an unknown host key is
    /// reported as SshHostKeyUnknown so the user can verify the fingerprint themselves).
    class SshConnectionProbe : public QObject
    {
        Q_OBJECT

    public:
        struct Request
        {
            QString target; // Host alias from ~/.ssh/config, or the server host name
            int port{0}; // 0 = let ssh decide (config / 22)
            QString identityFile; // optional private key path
            QString user{QStringLiteral("git")};
        };

        explicit SshConnectionProbe(QObject* parent = nullptr);
        ~SshConnectionProbe() override;

        /// Starts a probe; the result arrives through probeFinished(). `tag` is echoed back.
        void probe(const QString& tag, const Request& request);

        /// The ssh command line a user can paste in a terminal to run the same check.
        [[nodiscard]] static QString commandLine(const Request& request);

        /// Overrides the ssh executable (tests).
        void setProgram(const QString& program) { m_program = program; }
        void setTimeoutMs(int ms) { m_timeoutMs = ms; }

    signals:
        void probeFinished(const QString& tag, gf::core::git::bl::RemoteAccessIssue issue,
                           const QString& account, const QString& output);

    private:
        [[nodiscard]] static QStringList arguments(const Request& request, int connectTimeoutSec);

        QString m_program{QStringLiteral("ssh")};
        int m_timeoutMs{12000};
    };
} // namespace gf::desktop::services
