#pragma once

#include "core/modules/git/ports/git_batch_session.hpp"

#include <filesystem>
#include <string>

namespace gf::infra::git {

class NativeGitBatchSessionFactory final : public gf::git::ports::IGitBatchSessionFactory {
public:
    explicit NativeGitBatchSessionFactory(std::filesystem::path gitExecutablePath,
                                          std::string label = "native-batch");

    [[nodiscard]] std::string_view adapterName() const noexcept override { return label_; }

    [[nodiscard]] std::expected<std::unique_ptr<gf::git::ports::IGitBatchSession>,
                                gf::git::bo::GitExecutionError>
    openSession(const std::filesystem::path& repositoryPath) override;

private:
    std::filesystem::path gitExecutablePath_;
    std::string label_;
};

} // namespace gf::infra::git
