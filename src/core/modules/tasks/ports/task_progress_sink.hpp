#pragma once

#include "core/modules/tasks/bo/task_models.hpp"

#include <stop_token>
#include <string>

namespace gf::tasks::ports {

class ITaskProgressSink {
public:
    virtual ~ITaskProgressSink() = default;

    virtual void reportProgress(const bo::TaskProgress& progress) = 0;
    virtual void reportProgress(int percentage, const std::string& message = {}) = 0;

    [[nodiscard]] virtual bool isCancelled() const noexcept = 0;
    [[nodiscard]] virtual std::stop_token stopToken() const noexcept = 0;
};

} // namespace gf::tasks::ports
