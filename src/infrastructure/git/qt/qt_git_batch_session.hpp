#pragma once

#include "core/modules/git/ports/git_batch_session.hpp"

#include <QString>

#include <filesystem>
#include <string>

namespace gf::infra::git
{
    class QtGitBatchSessionFactory final : public gf::git::ports::IGitBatchSessionFactory
    {
    public:
        explicit QtGitBatchSessionFactory(std::filesystem::path gitExecutablePath,
                                          std::string label = "qt-batch");

        [[nodiscard]] std::string_view adapterName() const noexcept override { return label_; }

        [[nodiscard]] std::expected<std::unique_ptr<gf::git::ports::IGitBatchSession>,
                                    gf::git::bo::GitExecutionError>
        openSession(const std::filesystem::path& repositoryPath) override;

    private:
        std::filesystem::path gitExecutablePath_;
        QString programQt_;
        std::string label_;
    };
} // namespace gf::infra::git
