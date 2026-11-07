#pragma once

#include <chrono>
#include <format>
#include <random>
#include <string>
#include <string_view>

namespace gf::core {

[[nodiscard]] inline std::string generateUniqueId(std::string_view prefix) {
    thread_local std::mt19937_64 rng{std::random_device{}()};
    const auto nowMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                           std::chrono::system_clock::now().time_since_epoch())
                           .count();
    const auto randPart = static_cast<std::uint32_t>(rng());
    return std::format("{}_{:x}_{:08x}", prefix, nowMs, randPart);
}

} // namespace gf::core
