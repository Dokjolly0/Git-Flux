#pragma once

#include "core/modules/git/bo/history_models.hpp"
#include "core/modules/git/ports/repository_git_ops.hpp"

#include <expected>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace gf::git::bl {

class GitHistoryService {
public:
    explicit GitHistoryService(ports::IRepositoryGitOps& gitOps) noexcept;
    ~GitHistoryService() = default;

    /// Verifica se un nome rispetta le specifiche git-check-ref-format per i tag.
    [[nodiscard]] static bool isValidTagName(std::string_view name) noexcept;

    /// Ritorna la cronologia dei commit con opzioni di paginazione e filtri.
    [[nodiscard]] std::expected<std::vector<bo::GitCommitLogItem>, ports::GitOpsError>
    getCommitHistory(const std::filesystem::path& repoPath, const bo::CommitLogOptions& options = {});

    /// Ritorna i dettagli di un singolo commit dato il suo hash SHA o riferimento.
    [[nodiscard]] std::expected<std::optional<bo::GitCommitLogItem>, ports::GitOpsError>
    getCommitDetails(const std::filesystem::path& repoPath, const std::string& commitSha);

    /// Ritorna tutti i tag del repository (lightweight e annotated).
    [[nodiscard]] std::expected<std::vector<bo::GitTag>, ports::GitOpsError>
    listTags(const std::filesystem::path& repoPath);

    /// Crea un nuovo tag dopo aver validato il nome e acquisito il lock sul repository.
    [[nodiscard]] std::expected<void, ports::GitOpsError>
    createTag(const std::filesystem::path& repoPath, const bo::CreateTagOptions& options);

    /// Elimina un tag locale acquisendo il lock sul repository.
    [[nodiscard]] std::expected<void, ports::GitOpsError>
    deleteTag(const std::filesystem::path& repoPath, const std::string& tagName);

private:
    [[nodiscard]] std::expected<void, ports::GitOpsError>
    validateRepoPath(const std::filesystem::path& repoPath) const;

    ports::IRepositoryGitOps& m_gitOps;
};

} // namespace gf::git::bl
