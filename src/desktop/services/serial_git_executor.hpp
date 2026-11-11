#pragma once

#include <QMetaObject>
#include <QObject>
#include <QThreadPool>

#include <atomic>
#include <memory>
#include <type_traits>
#include <utility>

namespace gf::desktop
{
    /// Runs git work off the GUI thread, one task at a time and in submission order.
    ///
    /// Sharing one executor between viewmodels that touch the same repository keeps their
    /// operations ordered: a `stageAll` submitted before a `commit` always completes first,
    /// and a status read queued after a mutation always sees its result.
    ///
    /// `work` runs on the worker thread and must not touch Qt objects: capture everything it
    /// needs by value (services by reference, they outlive the executor). `done` receives the
    /// result on the thread of `context` (the GUI thread for viewmodels).
    ///
    /// Lifetime: the destructor waits for running and queued tasks. Declare the executor
    /// *after* every object used as `context` (members are destroyed in reverse order): it is
    /// then destroyed first, and every completion is posted while its context is still alive.
    class SerialGitExecutor final
    {
    public:
        SerialGitExecutor()
        {
            m_pool.setMaxThreadCount(1);
        }

        ~SerialGitExecutor()
        {
            m_pool.waitForDone();
        }

        SerialGitExecutor(const SerialGitExecutor&) = delete;
        SerialGitExecutor& operator=(const SerialGitExecutor&) = delete;

        template <typename Work, typename Done>
        void run(QObject* context, Work work, Done done)
        {
            using Result = std::invoke_result_t<Work&>;
            m_pool.start([context, work = std::move(work), done = std::move(done)]() mutable
            {
                // shared_ptr: queued functors must be copyable, results may not be cheap to copy.
                auto result = std::make_shared<Result>(work());
                QMetaObject::invokeMethod(context, [done, result]() mutable
                {
                    done(std::move(*result));
                }, Qt::QueuedConnection);
            });
        }

    private:
        QThreadPool m_pool;
    };

    /// "Latest wins" bookkeeping for one kind of read (diff of the selected file, files of the
    /// selected commit...). Each request takes a new id; an older request is skipped before it
    /// even runs git if a newer one exists by the time the executor reaches it, and its result
    /// is dropped if it completes late. Holding an arrow key therefore runs git only for the
    /// entry the user stops on.
    ///
    /// next()/invalidate() are called on the GUI thread; isCurrent() may be called from any
    /// thread, through the shared counter captured by the work lambda (see guard()).
    class LatestRequest final
    {
    public:
        using Id = quint64;

        /// Starts a new request, superseding every previous one.
        Id next() { return ++*m_latest; }

        /// Supersedes every request without starting a new one (e.g. the view was cleared).
        void invalidate() { ++*m_latest; }

        [[nodiscard]] bool isCurrent(Id id) const { return m_latest->load() == id; }

        /// Thread-safe "is request `id` still the latest?" check to capture in a work lambda.
        [[nodiscard]] auto guard(Id id) const
        {
            return [latest = m_latest, id]() { return latest->load() == id; };
        }

    private:
        std::shared_ptr<std::atomic<Id>> m_latest = std::make_shared<std::atomic<Id>>(0);
    };
} // namespace gf::desktop
