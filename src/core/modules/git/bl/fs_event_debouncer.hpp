#pragma once

#include "core/modules/git/bo/fs_watcher_models.hpp"

#include <chrono>
#include <filesystem>
#include <mutex>
#include <unordered_map>
#include <vector>

namespace gf::git::bl {

class FsEventDebouncer {
public:
    using Clock = std::chrono::steady_clock;
    using TimePoint = Clock::time_point;
    using Milliseconds = std::chrono::milliseconds;

    explicit FsEventDebouncer(Milliseconds quietPeriod = Milliseconds(200),
                              Milliseconds maxWindow = Milliseconds(1000));
    ~FsEventDebouncer() = default;

    void pushEvent(const bo::FsRawEvent& event,
                   bo::FsChangeClassification classification,
                   TimePoint now);

    [[nodiscard]] std::vector<bo::FsDebouncedBatch>
    flushReadyBatches(TimePoint now);

    [[nodiscard]] std::vector<bo::FsDebouncedBatch>
    flushAll();

    [[nodiscard]] bool hasPending() const noexcept;
    void clear();

    void setQuietPeriod(Milliseconds ms) noexcept;
    void setMaxWindow(Milliseconds ms) noexcept;

    [[nodiscard]] Milliseconds quietPeriod() const noexcept { return m_quietPeriod; }
    [[nodiscard]] Milliseconds maxWindow() const noexcept { return m_maxWindow; }

private:
    struct RepoPendingState {
        bo::FsDebouncedBatch batch{};
        TimePoint firstEventTime{};
        TimePoint lastEventTime{};
    };

    mutable std::mutex m_mutex;
    Milliseconds m_quietPeriod{200};
    Milliseconds m_maxWindow{1000};
    std::unordered_map<std::string, RepoPendingState> m_pendingByRepo;
};

} // namespace gf::git::bl
