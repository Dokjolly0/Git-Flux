#include "desktop/services/ssh_connection_probe.hpp"

#include <QProcess>
#include <QProcessEnvironment>
#include <QTimer>

#include <algorithm>
#include <memory>

namespace gf::desktop::services
{
    using gf::core::git::bl::RemoteAccessIssue;

    SshConnectionProbe::SshConnectionProbe(QObject* parent)
        : QObject(parent)
    {
    }

    QStringList SshConnectionProbe::arguments(const Request& request, int connectTimeoutSec)
    {
        QStringList args{
            QStringLiteral("-T"),
            QStringLiteral("-o"), QStringLiteral("BatchMode=yes"),
            QStringLiteral("-o"), QStringLiteral("ConnectTimeout=%1").arg(connectTimeoutSec),
            QStringLiteral("-o"), QStringLiteral("StrictHostKeyChecking=yes"),
        };
        if (request.port > 0)
        {
            args << QStringLiteral("-p") << QString::number(request.port);
        }
        if (!request.identityFile.isEmpty())
        {
            args << QStringLiteral("-i") << request.identityFile;
        }
        const QString user = request.user.isEmpty() ? QStringLiteral("git") : request.user;
        args << user + QLatin1Char('@') + request.target;
        return args;
    }

    QString SshConnectionProbe::commandLine(const Request& request)
    {
        QStringList parts{QStringLiteral("ssh"), QStringLiteral("-T")};
        if (request.port > 0)
        {
            parts << QStringLiteral("-p") << QString::number(request.port);
        }
        if (!request.identityFile.isEmpty())
        {
            parts << QStringLiteral("-i") << request.identityFile;
        }
        const QString user = request.user.isEmpty() ? QStringLiteral("git") : request.user;
        parts << user + QLatin1Char('@') + request.target;
        return parts.join(QLatin1Char(' '));
    }

    void SshConnectionProbe::probe(const QString& tag, const Request& request)
    {
        auto* process = new QProcess(this);
        auto env = QProcessEnvironment::systemEnvironment();
        env.insert(QStringLiteral("LC_ALL"), QStringLiteral("C"));
        process->setProcessEnvironment(env);
        process->setProcessChannelMode(QProcess::MergedChannels);
        process->setStandardInputFile(QProcess::nullDevice());

        auto* watchdog = new QTimer(process);
        watchdog->setSingleShot(true);
        connect(watchdog, &QTimer::timeout, process, [process]() { process->kill(); });

        auto finished = std::make_shared<bool>(false);
        auto finish = [this, tag, process, finished](RemoteAccessIssue issue, const QString& output)
        {
            if (*finished) return;
            *finished = true;
            QString account;
            if (issue == RemoteAccessIssue::None)
            {
                account = QString::fromStdString(gf::core::git::bl::extractSshGreetingAccount(output.toStdString()));
            }
            emit probeFinished(tag, issue, account, output.trimmed());
            process->deleteLater();
        };

        connect(process, &QProcess::finished, this, [process, watchdog, finish](int, QProcess::ExitStatus status)
        {
            watchdog->stop();
            const auto output = QString::fromUtf8(process->readAll());
            if (status == QProcess::CrashExit && output.trimmed().isEmpty())
            {
                finish(RemoteAccessIssue::ConnectionFailed,
                       QStringLiteral("ssh: nessuna risposta dal server entro il tempo limite"));
                return;
            }
            finish(gf::core::git::bl::classifyRemoteAccessOutput(output.toStdString()), output);
        });
        connect(process, &QProcess::errorOccurred, this, [process, watchdog, finish](QProcess::ProcessError error)
        {
            if (error != QProcess::FailedToStart) return; // Crashed/Timedout also emit finished()
            watchdog->stop();
            finish(RemoteAccessIssue::Unknown,
                   QStringLiteral("Impossibile avviare ssh: %1").arg(process->errorString()));
        });

        const int connectTimeoutSec = std::max(1, (m_timeoutMs - 2000) / 1000);
        watchdog->start(m_timeoutMs);
        process->start(m_program, arguments(request, connectTimeoutSec));
    }
} // namespace gf::desktop::services
