#pragma once

#include "infrastructure/persistence/sqlite/connection/sqlite_connection.hpp"
#include "infrastructure/persistence/repositories/sqlite_repository_store.hpp"
#include "infrastructure/persistence/groups/sqlite_group_store.hpp"
#include "infrastructure/persistence/workspaces/sqlite_workspace_store.hpp"
#include "core/modules/repositories/bl/repository_service.hpp"
#include "core/modules/groups/bl/group_service.hpp"
#include "core/modules/workspaces/bl/workspace_service.hpp"
#include "core/modules/git/bl/git_working_tree_service.hpp"
#include "core/modules/git/bl/git_staging_service.hpp"
#include "core/modules/git/bl/git_diff_service.hpp"
#include "core/modules/git/bl/git_branch_service.hpp"
#include "core/modules/git/bl/git_history_service.hpp"
#include "core/modules/git/bl/git_remote_merge_service.hpp"
#include "core/modules/git/bl/git_clone_service.hpp"
#include "core/modules/git/bl/git_stash_service.hpp"
#include "core/modules/git/bl/git_rebase_service.hpp"
#include "core/modules/git/bl/git_conflict_service.hpp"
#include "core/modules/git/bl/git_repository_watcher_service.hpp"
#include "core/modules/git/bl/async_git_service.hpp"
#include "core/modules/git/bl/multi_repo_batch_coordinator.hpp"
#include "core/modules/tasks/bl/background_task_runner.hpp"
#include "infrastructure/watcher/native_polling_fs_watcher.hpp"
#include "infrastructure/git/native/native_git_executor.hpp"
#include "infrastructure/git/native/native_repository_git_ops.hpp"
#include "desktop/viewmodels/batch_operations_viewmodel.hpp"
#include "desktop/viewmodels/home_hub_viewmodel.hpp"
#include "desktop/viewmodels/quick_open_viewmodel.hpp"
#include "desktop/viewmodels/workspace_session_viewmodel.hpp"
#include "desktop/viewmodels/git_profiles_viewmodel.hpp"
#include "infrastructure/persistence/profiles/sqlite_profile_store.hpp"
#include "infrastructure/security/crypto_vault.hpp"
#include "infrastructure/git/credentials/system_credential_detector.hpp"
#include "desktop/services/git_provider_api_client.hpp"

#include <filesystem>
#include <memory>

class QQmlApplicationEngine;

namespace gf::desktop
{
    class AppBootstrap
    {
    public:
        explicit AppBootstrap(const std::filesystem::path& databasePath);
        ~AppBootstrap();

        AppBootstrap(const AppBootstrap&) = delete;
        AppBootstrap& operator=(const AppBootstrap&) = delete;
        AppBootstrap(AppBootstrap&&) = default;
        AppBootstrap& operator=(AppBootstrap&&) = default;

        void initialize();
        void registerWithQml(QQmlApplicationEngine& engine);

        [[nodiscard]] HomeHubViewModel& homeHubViewModel() noexcept;
        [[nodiscard]] QuickOpenViewModel& quickOpenViewModel() noexcept;
        [[nodiscard]] WorkspaceSessionViewModel& workspaceSessionViewModel() noexcept;
        [[nodiscard]] BatchOperationsViewModel& batchOperationsViewModel() noexcept;
        [[nodiscard]] viewmodels::GitProfilesViewModel& gitProfilesViewModel() noexcept;
        [[nodiscard]] git::bl::GitRepositoryWatcherService& watcherService() noexcept;
        [[nodiscard]] git::bl::MultiRepoBatchCoordinator& batchCoordinator() noexcept;
        [[nodiscard]] repositories::bl::RepositoryService& repositoryService() noexcept;
        [[nodiscard]] groups::bl::GroupService& groupService() noexcept;
        [[nodiscard]] workspaces::bl::WorkspaceService& workspaceService() noexcept;

        static std::filesystem::path defaultDatabasePath();

    private:
        std::filesystem::path m_dbPath;
        std::unique_ptr<infra::persistence::sqlite::SqliteConnection> m_connection;
        std::unique_ptr<infra::persistence::repositories::SqliteRepositoryStore> m_repoStore;
        std::unique_ptr<infra::persistence::groups::SqliteGroupStore> m_groupStore;
        std::unique_ptr<infra::persistence::workspaces::SqliteWorkspaceStore> m_workspaceStore;

        std::unique_ptr<repositories::bl::RepositoryService> m_repoService;
        std::unique_ptr<groups::bl::GroupService> m_groupService;
        std::unique_ptr<workspaces::bl::WorkspaceService> m_workspaceService;

        std::unique_ptr<infra::git::NativeGitCommandExecutor> m_gitExecutor;
        std::unique_ptr<infra::git::NativeRepositoryGitOps> m_gitOps;

        std::unique_ptr<git::bl::GitWorkingTreeService> m_workingTreeService;
        std::unique_ptr<git::bl::GitStagingService> m_stagingService;
        std::unique_ptr<git::bl::GitDiffService> m_diffService;
        std::unique_ptr<git::bl::GitBranchService> m_branchService;
        std::unique_ptr<git::bl::GitHistoryService> m_historyService;
        std::unique_ptr<git::bl::GitRemoteMergeService> m_remoteMergeService;
        std::unique_ptr<git::bl::GitCloneService> m_cloneService;
        std::unique_ptr<git::bl::GitStashService> m_stashService;
        std::unique_ptr<git::bl::GitRebaseService> m_rebaseService;
        std::unique_ptr<git::bl::GitConflictService> m_conflictService;

        std::unique_ptr<infra::watcher::NativePollingFsWatcher> m_watcherPort;
        std::unique_ptr<git::bl::GitRepositoryWatcherService> m_watcherService;
        std::unique_ptr<tasks::bl::BackgroundTaskRunner> m_taskRunner;
        std::unique_ptr<git::bl::AsyncGitService> m_asyncGitService;
        std::unique_ptr<git::bl::MultiRepoBatchCoordinator> m_batchCoordinator;

        std::unique_ptr<HomeHubViewModel> m_homeHubVm;
        std::unique_ptr<QuickOpenViewModel> m_quickOpenVm;
        std::unique_ptr<WorkspaceSessionViewModel> m_workspaceSessionVm;
        std::unique_ptr<BatchOperationsViewModel> m_batchOperationsVm;

        std::unique_ptr<infra::persistence::profiles::SqliteProfileStore> m_profileStore;
        std::unique_ptr<infra::security::CryptoVault> m_cryptoVault;
        std::unique_ptr<infra::git::credentials::SystemCredentialDetector> m_credentialDetector;
        std::unique_ptr<QNetworkAccessManager> m_nam;
        std::unique_ptr<services::GitProviderApiClient> m_gitApiClient;
        std::unique_ptr<viewmodels::GitProfilesViewModel> m_gitProfilesVm;
    };
} // namespace gf::desktop
