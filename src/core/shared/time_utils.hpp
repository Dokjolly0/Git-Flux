#pragma once

#include <chrono>
#include <cstdint>

namespace gf::core
{
    [[nodiscard]] inline std::int64_t currentUnixMs() noexcept
    {
        return std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch())
            .count();
    }
} // namespace gf::core
