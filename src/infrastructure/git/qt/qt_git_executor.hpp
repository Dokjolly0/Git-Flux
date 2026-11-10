#pragma once

#include "core/modules/git/ports/git_command_executor.hpp"

#include <QProcessEnvironment>
#include <QString>

#include <filesystem>
#include <string>

namespace gf::infra::git
{
    class QtGitCommandExecutor final : public gf::git::ports::IGitCommandExecutor
    {
    public:
        explicit QtGitCommandExecutor(std::filesystem::path gitExecutablePath,
                                      std::string label = "qt-qprocess");

        [[nodiscard]] std::string_view adapterName() const noexcept override { return label_; }

        [[nodiscard]] std::expected<gf::git::bo::GitProcessResult, gf::git::bo::GitExecutionError>
        execute(const gf::git::bo::GitCommand& command,
                std::stop_token stopToken = {}) override;

    private:
        std::filesystem::path gitExecutablePath_;
        QString programQt_;
        std::string label_;
        QProcessEnvironment envStandard_;
        QProcessEnvironment envNoOptionalLocks_;
    };
} // namespace gf::infra::git
