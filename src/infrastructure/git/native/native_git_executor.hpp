#pragma once

#include "core/modules/git/ports/git_command_executor.hpp"

#include <filesystem>
#include <string>

namespace gf::infra::git
{
    class NativeGitCommandExecutor final : public gf::git::ports::IGitCommandExecutor
    {
    public:
        explicit NativeGitCommandExecutor(std::filesystem::path gitExecutablePath,
                                          std::string label = "native-os");

        [[nodiscard]] std::string_view adapterName() const noexcept override { return label_; }

        [[nodiscard]] const std::filesystem::path& executablePath() const noexcept
        {
            return gitExecutablePath_;
        }

        [[nodiscard]] std::expected<gf::git::bo::GitProcessResult, gf::git::bo::GitExecutionError>
        execute(const gf::git::bo::GitCommand& command,
                std::stop_token stopToken = {}) override;

        /// Exposed for unit tests verifying Windows command line escaping and quoting security.
        [[nodiscard]] static std::wstring buildWindowsCommandLine(
            const std::filesystem::path& executable,
            const std::vector<std::string>& utf8Args);

    private:
        std::filesystem::path gitExecutablePath_;
        std::string label_;
        std::wstring envBlockStandard_;
        std::wstring envBlockNoLocks_;
    };
} // namespace gf::infra::git
