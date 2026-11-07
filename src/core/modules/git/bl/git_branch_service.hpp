#pragma once

#include "core/modules/git/bo/branch_models.hpp"
#include "core/modules/git/ports/repository_git_ops.hpp"

#include <expected>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace gf::git::bl {

class GitBranchService {
public:
    explicit GitBranchService(ports::IRepositoryGitOps& gitOps) noexcept;
    ~GitBranchService() = default;

    /// Verifica se un nome rispetta le regole git-check-ref-format per i branch.
    [[nodiscard]] static bool isValidBranchName(std::string_view name) noexcept;

    /// Ritorna l'elenco completo dei branch locali e remoti.
    [[nodiscard]] std::expected<std::vector<bo::GitBranch>, ports::GitOpsError>
    listBranches(const std::filesystem::path& repoPath);

    /// Ritorna solo i branch locali.
    [[nodiscard]] std::expected<std::vector<bo::GitBranch>, ports::GitOpsError>
    getLocalBranches(const std::filesystem::path& repoPath);

    /// Ritorna solo i branch remoti.
    [[nodiscard]] std::expected<std::vector<bo::GitBranch>, ports::GitOpsError>
    getRemoteBranches(const std::filesystem::path& repoPath);

    /// Ritorna il branch correntemente attivo (HEAD), o nullopt se HEAD è detached o non trovato.
    [[nodiscard]] std::expected<std::optional<bo::GitBranch>, ports::GitOpsError>
    getCurrentBranch(const std::filesystem::path& repoPath);

    /// Crea un nuovo branch dopo aver validato il nome.
    [[nodiscard]] std::expected<void, ports::GitOpsError>
    createBranch(const std::filesystem::path& repoPath, const bo::CreateBranchOptions& options);

    /// Esegue il checkout di un branch o commit.
    [[nodiscard]] std::expected<void, ports::GitOpsError>
    checkoutBranch(const std::filesystem::path& repoPath, const bo::CheckoutOptions& options);

    /// Elimina un branch locale.
    [[nodiscard]] std::expected<void, ports::GitOpsError>
    deleteBranch(const std::filesystem::path& repoPath, const std::string& branchName, bool force = false);

    /// Rinomina un branch locale dopo aver validato il nuovo nome.
    [[nodiscard]] std::expected<void, ports::GitOpsError>
    renameBranch(const std::filesystem::path& repoPath, const std::string& oldName, const std::string& newName);

private:
    [[nodiscard]] std::expected<void, ports::GitOpsError>
    validateRepoPath(const std::filesystem::path& repoPath) const;

    ports::IRepositoryGitOps& m_gitOps;
};

} // namespace gf::git::bl
