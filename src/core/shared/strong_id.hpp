#pragma once

#include <compare>
#include <functional>
#include <string>
#include <string_view>
#include <utility>

namespace gf::core
{
    template <typename Tag>
    class StrongId
    {
    public:
        StrongId() = default;

        explicit StrongId(std::string value) noexcept
            : value_(std::move(value))
        {
        }

        explicit StrongId(std::string_view value)
            : value_(value)
        {
        }

        explicit StrongId(const char* value)
            : value_(value ? value : "")
        {
        }

        [[nodiscard]] const std::string& value() const noexcept { return value_; }
        [[nodiscard]] bool empty() const noexcept { return value_.empty(); }

        auto operator<=>(const StrongId&) const = default;

    private:
        std::string value_;
    };
} // namespace gf::core

namespace std
{
    template <typename Tag>
    struct hash<gf::core::StrongId<Tag>>
    {
        [[nodiscard]] size_t operator()(const gf::core::StrongId<Tag>& id) const noexcept
        {
            return std::hash<std::string>{}(id.value());
        }
    };
} // namespace std
