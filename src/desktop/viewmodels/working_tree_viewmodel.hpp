#pragma once

#include <QObject>
#include <QString>
#include <QVariantList>
#include <QVariantMap>

#include "desktop/viewmodels/working_tree_file_list_model.hpp"

#ifndef Q_MOC_RUN
#include "core/modules/git/bo/repo_working_tree_status.hpp"
#endif

namespace gf::git::bl
{
    class GitWorkingTreeService;
    class GitStagingService;
}

namespace gf::desktop
{
    class WorkingTreeViewModel final : public QObject
    {
        Q_OBJECT

        Q_PROPERTY(WorkingTreeFileListModel* stagedModel READ stagedModel CONSTANT)
        Q_PROPERTY(WorkingTreeFileListModel* unstagedModel READ unstagedModel CONSTANT)
        Q_PROPERTY(WorkingTreeFileListModel* untrackedModel READ untrackedModel CONSTANT)
        Q_PROPERTY(WorkingTreeFileListModel* conflictedModel READ conflictedModel CONSTANT)

        Q_PROPERTY(int stagedCount READ stagedCount NOTIFY statusChanged)
        Q_PROPERTY(int unstagedCount READ unstagedCount NOTIFY statusChanged)
        Q_PROPERTY(int untrackedCount READ untrackedCount NOTIFY statusChanged)
        Q_PROPERTY(int conflictedCount READ conflictedCount NOTIFY statusChanged)
        Q_PROPERTY(bool isClean READ isClean NOTIFY statusChanged)

        Q_PROPERTY(QString branchName READ branchName NOTIFY statusChanged)
        Q_PROPERTY(int aheadCount READ aheadCount NOTIFY statusChanged)
        Q_PROPERTY(int behindCount READ behindCount NOTIFY statusChanged)
        Q_PROPERTY(bool isDetached READ isDetached NOTIFY statusChanged)
        Q_PROPERTY(bool isUnborn READ isUnborn NOTIFY statusChanged)
        Q_PROPERTY(bool isBusy READ isBusy NOTIFY isBusyChanged)

    public:
        explicit WorkingTreeViewModel(gf::git::bl::GitWorkingTreeService& statusService,
                                      gf::git::bl::GitStagingService& stagingService,
                                      QObject* parent = nullptr);
        ~WorkingTreeViewModel() override = default;

        [[nodiscard]] WorkingTreeFileListModel* stagedModel() noexcept { return &m_stagedModel; }
        [[nodiscard]] WorkingTreeFileListModel* unstagedModel() noexcept { return &m_unstagedModel; }
        [[nodiscard]] WorkingTreeFileListModel* untrackedModel() noexcept { return &m_untrackedModel; }
        [[nodiscard]] WorkingTreeFileListModel* conflictedModel() noexcept { return &m_conflictedModel; }

        [[nodiscard]] int stagedCount() const noexcept { return m_stagedCount; }
        [[nodiscard]] int unstagedCount() const noexcept { return m_unstagedCount; }
        [[nodiscard]] int untrackedCount() const noexcept { return m_untrackedCount; }
        [[nodiscard]] int conflictedCount() const noexcept { return m_conflictedCount; }
        [[nodiscard]] bool isClean() const noexcept { return m_isClean; }

        [[nodiscard]] QString branchName() const noexcept { return m_branchName; }
        [[nodiscard]] QString headOid() const noexcept { return m_headOid; }
        [[nodiscard]] int aheadCount() const noexcept { return m_aheadCount; }
        [[nodiscard]] int behindCount() const noexcept { return m_behindCount; }
        [[nodiscard]] bool isDetached() const noexcept { return m_isDetached; }
        [[nodiscard]] bool isUnborn() const noexcept { return m_isUnborn; }
        [[nodiscard]] bool isBusy() const noexcept { return m_isBusy; }

    Q_INVOKABLE bool refresh(const QString& repoPath);
    Q_INVOKABLE bool stageFile(const QString& repoPath, const QString& filePath);
    Q_INVOKABLE bool stageAll(const QString& repoPath);
    Q_INVOKABLE bool unstageFile(const QString& repoPath, const QString& filePath);
    Q_INVOKABLE bool unstageAll(const QString& repoPath);
    Q_INVOKABLE bool discardFile(const QString& repoPath, const QString& filePath);
    Q_INVOKABLE bool discardAll(const QString& repoPath);

        [[nodiscard]] Q_INVOKABLE QVariantList getStagedFiles() const;
        [[nodiscard]] Q_INVOKABLE QVariantList getUnstagedFiles() const;
        [[nodiscard]] Q_INVOKABLE QVariantList getAllWorkingFiles() const;

        signals :

        void statusChanged();
        void isBusyChanged();
        void operationFailed(const QString& errorMessage);

    private:
        gf::git::bl::GitWorkingTreeService& m_statusService;
        gf::git::bl::GitStagingService& m_stagingService;

        WorkingTreeFileListModel m_stagedModel;
        WorkingTreeFileListModel m_unstagedModel;
        WorkingTreeFileListModel m_untrackedModel;
        WorkingTreeFileListModel m_conflictedModel;

        int m_stagedCount = 0;
        int m_unstagedCount = 0;
        int m_untrackedCount = 0;
        int m_conflictedCount = 0;
        bool m_isClean = true;

        QString m_branchName;
        QString m_headOid;
        int m_aheadCount = 0;
        int m_behindCount = 0;
        bool m_isDetached = false;
        bool m_isUnborn = false;
        bool m_isBusy = false;

        QString m_lastRepoPath;
        gf::git::bo::RepoWorkingTreeStatus m_lastStatus;
        bool m_hasLastStatus = false;
    };
} // namespace gf::desktop
