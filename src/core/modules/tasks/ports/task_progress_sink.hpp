#pragma once

#include <string>

namespace gf::tasks::ports
{
    class ITaskProgressSink
    {
    public:
        virtual ~ITaskProgressSink() = default;

        virtual void reportProgress(int percentage, const std::string& message) = 0;

        [[nodiscard]] virtual bool isCancelled() const noexcept = 0;
    };
} // namespace gf::tasks::ports
