#pragma once

#include "core/modules/git/bo/git_command.hpp"

#include <expected>
#include <stop_token>

namespace gf::git::ports
{
    class IGitCommandExecutor
    {
    public:
        virtual ~IGitCommandExecutor() = default;

        [[nodiscard]] std::expected<bo::GitProcessResult, bo::GitExecutionError>
        execute(const bo::GitCommand& command)
        {
            return execute(command, {});
        }

        [[nodiscard]] virtual std::expected<bo::GitProcessResult, bo::GitExecutionError>
        execute(const bo::GitCommand& command, std::stop_token stopToken) = 0;
    };
} // namespace gf::git::ports
