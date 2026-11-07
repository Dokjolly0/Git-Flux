#pragma once

#include "core/modules/git/bo/remote_merge_models.hpp"
#include "core/modules/git/ports/repository_git_ops.hpp"

#include <expected>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace gf::git::bl {

class GitRemoteMergeService {
public:
    explicit GitRemoteMergeService(ports::IRepositoryGitOps& gitOps) noexcept;
    ~GitRemoteMergeService() = default;

    /// Verifica se un identificatore di remote è valido.
    [[nodiscard]] static bool isValidRemoteName(std::string_view name) noexcept;

    /// Ritorna la lista dei remote configurati.
    [[nodiscard]] std::expected<std::vector<bo::GitRemote>, ports::GitOpsError>
    listRemotes(const std::filesystem::path& repoPath);

    /// Aggiunge un nuovo remote dopo aver validato nome e URL.
    [[nodiscard]] std::expected<void, ports::GitOpsError>
    addRemote(const std::filesystem::path& repoPath, const std::string& name, const std::string& url);

    /// Rimuove un remote configurato.
    [[nodiscard]] std::expected<void, ports::GitOpsError>
    removeRemote(const std::filesystem::path& repoPath, const std::string& name);

    /// Esegue il fetch da un remote acquisendo il lock sul repository.
    [[nodiscard]] std::expected<bo::RemoteOperationResult, ports::GitOpsError>
    fetchRemote(const std::filesystem::path& repoPath, const bo::RemoteOperationOptions& options = {});

    /// Esegue il pull da un remote acquisendo il lock sul repository.
    [[nodiscard]] std::expected<bo::RemoteOperationResult, ports::GitOpsError>
    pullRemote(const std::filesystem::path& repoPath, const bo::RemoteOperationOptions& options = {});

    /// Esegue il push verso un remote acquisendo il lock sul repository.
    [[nodiscard]] std::expected<bo::RemoteOperationResult, ports::GitOpsError>
    pushRemote(const std::filesystem::path& repoPath, const bo::RemoteOperationOptions& options = {});

    /// Esegue il merge nel branch attivo acquisendo il lock sul repository.
    [[nodiscard]] std::expected<bo::MergeResult, ports::GitOpsError>
    mergeBranch(const std::filesystem::path& repoPath, const bo::MergeOptions& options);

    /// Annulla un merge in corso (`git merge --abort`).
    [[nodiscard]] std::expected<void, ports::GitOpsError>
    abortMerge(const std::filesystem::path& repoPath);

private:
    [[nodiscard]] std::expected<void, ports::GitOpsError>
    validateRepoPath(const std::filesystem::path& repoPath) const;

    ports::IRepositoryGitOps& m_gitOps;
};

} // namespace gf::git::bl
