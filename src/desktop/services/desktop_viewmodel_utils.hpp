#pragma once

#include <QClipboard>
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QGuiApplication>
#include <QString>
#include <QThread>

#include <cstdint>
#include <ctime>

namespace gf::desktop
{
    /// Polls the Qt event loop until `isIdle()` returns true or `timeoutMs` elapses.
    template <typename Predicate>
    [[nodiscard]] inline bool waitUntilIdle(const int timeoutMs,
                                            Predicate&& isIdle,
                                            const int processMaxTimeMs = 10,
                                            const int sleepMs = 1)
    {
        QElapsedTimer timer;
        timer.start();
        while (!isIdle())
        {
            if (timer.elapsed() > timeoutMs)
            {
                return false;
            }
            QCoreApplication::processEvents(QEventLoop::AllEvents, processMaxTimeMs);
            QThread::msleep(static_cast<unsigned long>(sleepMs));
        }
        return true;
    }

    /// Variant that processes Qt events first and runs one confirmation pass when idle.
    template <typename Predicate>
    [[nodiscard]] inline bool waitUntilIdleDraining(const int timeoutMs,
                                                    Predicate&& isIdle,
                                                    const int processMaxTimeMs = 20,
                                                    const int sleepMs = 10)
    {
        QElapsedTimer timer;
        timer.start();
        while (timer.elapsed() < timeoutMs)
        {
            QCoreApplication::processEvents(QEventLoop::AllEvents, processMaxTimeMs);
            if (isIdle())
            {
                QCoreApplication::processEvents(QEventLoop::AllEvents, processMaxTimeMs);
                if (isIdle())
                {
                    return true;
                }
            }
            QThread::msleep(static_cast<unsigned long>(sleepMs));
        }
        return isIdle();
    }

    /// Copies `text` to the system clipboard when running with a GUI application instance.
    inline bool copyTextToClipboard(const QString& text)
    {
        if (!qGuiApp)
        {
            return false;
        }
        if (auto* clipboard = QGuiApplication::clipboard())
        {
            clipboard->setText(text);
            return true;
        }
        return false;
    }

    /// Formats a UTC Unix epoch timestamp in milliseconds as `YYYY-MM-DD HH:MM` (or `-` when unset).
    [[nodiscard]] inline QString formatTimestampMs(const std::int64_t epochMs)
    {
        if (epochMs <= 0)
        {
            return QStringLiteral("-");
        }
        const auto time = static_cast<std::time_t>(epochMs / 1000);
        std::tm tmBuffer{};
#if defined(_WIN32)
        gmtime_s(&tmBuffer, &time);
#else
        gmtime_r(&time, &tmBuffer);
#endif
        return QString::asprintf("%04d-%02d-%02d %02d:%02d",
                                 tmBuffer.tm_year + 1900,
                                 tmBuffer.tm_mon + 1,
                                 tmBuffer.tm_mday,
                                 tmBuffer.tm_hour,
                                 tmBuffer.tm_min);
    }
} // namespace gf::desktop
