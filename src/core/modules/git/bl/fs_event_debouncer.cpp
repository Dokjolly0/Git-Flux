#include "core/modules/git/bl/fs_event_debouncer.hpp"

#include <algorithm>

namespace gf::git::bl
{
    FsEventDebouncer::FsEventDebouncer(Milliseconds quietPeriod, Milliseconds maxWindow)
        : m_quietPeriod(quietPeriod)
          , m_maxWindow(maxWindow)
    {
    }

    void FsEventDebouncer::pushEvent(const bo::FsRawEvent& event,
                                     bo::FsChangeClassification classification,
                                     TimePoint now)
    {
        if (classification == bo::FsChangeClassification::Ignored)
        {
            return;
        }

        std::lock_guard<std::mutex> lock(m_mutex);
        std::string key = event.repositoryPath.lexically_normal().string();

        auto it = m_pendingByRepo.find(key);
        if (it == m_pendingByRepo.end())
        {
            RepoPendingState state;
            state.batch.repositoryPath = event.repositoryPath;
            state.firstEventTime = now;
            state.lastEventTime = now;
            it = m_pendingByRepo.emplace(key, std::move(state)).first;
        }
        else
        {
            it->second.lastEventTime = now;
        }

        auto& batch = it->second.batch;

        switch (classification)
        {
        case bo::FsChangeClassification::GitHeadChanged:
            batch.headChanged = true;
            break;
        case bo::FsChangeClassification::GitRefsChanged:
            batch.refsChanged = true;
            break;
        case bo::FsChangeClassification::GitIndexChanged:
            batch.indexChanged = true;
            break;
        case bo::FsChangeClassification::GitOperationState:
            batch.operationStateChanged = true;
            break;
        case bo::FsChangeClassification::WorkingTreeModified:
            batch.workingTreeChanged = true;
            break;
        case bo::FsChangeClassification::Ignored:
            break;
        }

        if (!event.changedPath.empty())
        {
            if (std::find(batch.affectedPaths.begin(), batch.affectedPaths.end(), event.changedPath) == batch.
                affectedPaths.end())
            {
                batch.affectedPaths.push_back(event.changedPath);
            }
        }
    }

    std::vector<bo::FsDebouncedBatch> FsEventDebouncer::flushReadyBatches(TimePoint now)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        std::vector<bo::FsDebouncedBatch> ready;

        for (auto it = m_pendingByRepo.begin(); it != m_pendingByRepo.end();)
        {
            const auto& state = it->second;
            bool quietElapsed = (now - state.lastEventTime) >= m_quietPeriod;
            bool maxWindowElapsed = (now - state.firstEventTime) >= m_maxWindow;

            if (quietElapsed || maxWindowElapsed)
            {
                ready.push_back(state.batch);
                it = m_pendingByRepo.erase(it);
            }
            else
            {
                ++it;
            }
        }

        return ready;
    }

    std::vector<bo::FsDebouncedBatch> FsEventDebouncer::flushAll()
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        std::vector<bo::FsDebouncedBatch> all;
        all.reserve(m_pendingByRepo.size());

        for (auto& [key, state] : m_pendingByRepo)
        {
            all.push_back(std::move(state.batch));
        }
        m_pendingByRepo.clear();

        return all;
    }

    bool FsEventDebouncer::hasPending() const noexcept
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return !m_pendingByRepo.empty();
    }

    void FsEventDebouncer::clear()
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_pendingByRepo.clear();
    }

    void FsEventDebouncer::setQuietPeriod(Milliseconds ms) noexcept
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_quietPeriod = ms;
    }

    void FsEventDebouncer::setMaxWindow(Milliseconds ms) noexcept
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_maxWindow = ms;
    }
} // namespace gf::git::bl
