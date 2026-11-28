#include "core/modules/git/bl/fs_event_debouncer.hpp"

#include <algorithm>
#include <ranges>

namespace gf::git::bl
{
    FsEventDebouncer::FsEventDebouncer(const Milliseconds quietPeriod, const Milliseconds maxWindow)
        : m_quietPeriod(quietPeriod)
          , m_maxWindow(maxWindow)
    {
    }

    void FsEventDebouncer::pushEvent(const bo::FsRawEvent& event,
                                     const bo::FsChangeClassification classification,
                                     const TimePoint now)
    {
        if (classification == bo::FsChangeClassification::Ignored)
        {
            return;
        }

        std::lock_guard lock(m_mutex);
        const std::string key = event.repositoryPath.lexically_normal().string();

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
            if (std::ranges::find(batch.affectedPaths, event.changedPath) == batch.affectedPaths.end())
            {
                batch.affectedPaths.push_back(event.changedPath);
            }
        }
    }

    std::vector<bo::FsDebouncedBatch> FsEventDebouncer::flushReadyBatches(const TimePoint now)
    {
        std::lock_guard lock(m_mutex);
        std::vector<bo::FsDebouncedBatch> ready;

        for (auto it = m_pendingByRepo.begin(); it != m_pendingByRepo.end();)
        {
            const bool quietElapsed = now - it->second.lastEventTime >= m_quietPeriod;
            if (const bool maxWindowElapsed = now - it->second.firstEventTime >= m_maxWindow;
                quietElapsed || maxWindowElapsed)
            {
                ready.push_back(it->second.batch);
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
        std::lock_guard lock(m_mutex);
        std::vector<bo::FsDebouncedBatch> all;
        all.reserve(m_pendingByRepo.size());

        for (auto& state : m_pendingByRepo | std::views::values)
        {
            all.push_back(std::move(state.batch));
        }
        m_pendingByRepo.clear();

        return all;
    }

    bool FsEventDebouncer::hasPending() const noexcept
    {
        std::lock_guard lock(m_mutex);
        return !m_pendingByRepo.empty();
    }
} // namespace gf::git::bl
