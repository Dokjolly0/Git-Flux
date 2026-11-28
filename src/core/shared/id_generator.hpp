#pragma once

#include "core/shared/time_utils.hpp"

#include <format>
#include <random>
#include <string>
#include <string_view>

namespace gf::core
{
    [[nodiscard]] inline std::string generateUniqueId(std::string_view prefix)
    {
        thread_local std::mt19937_64 rng{std::random_device{}()};
        const auto nowMs = currentUnixMs();
        const auto randPart = static_cast<std::uint32_t>(rng());
        return std::format("{}_{:x}_{:08x}", prefix, nowMs, randPart);
    }
} // namespace gf::core
