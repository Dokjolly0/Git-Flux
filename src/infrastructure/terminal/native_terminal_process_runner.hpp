#pragma once

#include "core/modules/terminal/ports/terminal_process_port.hpp"

#include <filesystem>
#include <vector>

namespace gf::infra::terminal
{
    class NativeTerminalProcessRunner final : public gf::terminal::ports::ITerminalProcessRunner
    {
    public:
        explicit NativeTerminalProcessRunner(std::filesystem::path gitExecutablePath);

        [[nodiscard]] std::vector<gf::terminal::bo::TerminalShellInfo> availableShells() const override;

        [[nodiscard]] std::expected<std::shared_ptr<gf::terminal::ports::ITerminalProcessHandle>, std::string>
        start(const gf::terminal::bo::TerminalExecutionRequest& request,
              gf::terminal::bo::TerminalExecutionCallbacks callbacks) override;

        [[nodiscard]] const std::filesystem::path& gitExecutablePath() const noexcept
        {
            return m_gitExecutablePath;
        }

    private:
        std::filesystem::path m_gitExecutablePath;
        std::vector<gf::terminal::bo::TerminalShellInfo> m_availableShells;
    };
} // namespace gf::infra::terminal
