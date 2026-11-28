#pragma once

#include <QAbstractListModel>
#include <QNetworkAccessManager>
#include <QSet>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#ifndef Q_MOC_RUN
#include "core/modules/git/bo/history_models.hpp"
#else
namespace gf::git::bo
{
    struct GitCommitLogItem;
}
#endif

namespace gf::desktop
{
    class CommitHistoryListModel final : public QAbstractListModel
    {
        Q_OBJECT

        Q_PROPERTY(int maxLanes READ maxLanes NOTIFY maxLanesChanged)
        Q_PROPERTY(QString searchFilter READ searchFilter WRITE setSearchFilter NOTIFY searchFilterChanged)
        Q_PROPERTY(int totalCommitsCount READ totalCommitsCount NOTIFY totalCommitsCountChanged)
        Q_PROPERTY(int filteredCommitsCount READ rowCount NOTIFY searchFilterChanged)

    public:
        enum Roles
        {
            ShaRole = Qt::UserRole + 1,
            ShortShaRole,
            SubjectRole,
            BodyRole,
            AuthorNameRole,
            AuthorEmailRole,
            AuthorDateRole,
            DecorationsRole,
            ParentsRole,
            AuthorAvatarUrlRole,
            AuthorInitialsRole,
            AuthorColorRole,
            StructuredDecorationsRole,
            AuthorDateFormattedRole,
            CommitterNameRole,
            CommitterEmailRole,
            CommitterDateFormattedRole,
            CommitterAvatarUrlRole,
            CommitterInitialsRole,
            GraphLaneRole,
            GraphColorRole,
            GraphRoutesRole,
            IsMergeRole,
            IsWipRole,
            IsOrphanRole,
            IsStashRole,
            StashSelectorRole,
            BranchCommitCountRole,
            InferredBranchRole,
            InferredBranchColorRole,
            InferredBranchBgColorRole
        };

        explicit CommitHistoryListModel(QObject* parent = nullptr);
        ~CommitHistoryListModel() override = default;

        [[nodiscard]] int rowCount(const QModelIndex& parent = QModelIndex()) const override;
        [[nodiscard]] QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
        [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

        void setCommits(std::vector<git::bo::GitCommitLogItem> commits, bool isFullDag = true);
        void setWip(bool hasWip, int changedFilesCount = 0);
        [[nodiscard]] bool hasWip() const noexcept { return m_hasWip; }
        [[nodiscard]] const std::vector<git::bo::GitCommitLogItem>& commits() const noexcept { return m_commits; }

        [[nodiscard]] int maxLanes() const noexcept { return m_maxLanes; }
        [[nodiscard]] QString searchFilter() const noexcept { return m_searchFilter; }
        void setSearchFilter(const QString& filter);
        [[nodiscard]] int totalCommitsCount() const noexcept { return static_cast<int>(m_rawCommits.size()); }
        Q_INVOKABLE [[nodiscard]] QVariantMap getCommit(int index) const;
        Q_INVOKABLE [[nodiscard]] QString findTargetBranchAt(int index, const QString& excludeBranch) const;
        // Total reachable commits up to `sha` on the full DAG (matches `git rev-list --count <sha>`), or 0 when unavailable.
        Q_INVOKABLE [[nodiscard]] int reachableCommitCount(const QString& sha) const;
        // Row of the commit with this SHA in the (filtered) list, or -1.
        Q_INVOKABLE [[nodiscard]] int indexOfSha(const QString& sha) const;
        // SHA of the commit HEAD points to (attached or detached), empty when it isn't in the list.
        [[nodiscard]] QString headSha() const noexcept { return m_headSha; }

        struct MultiSelectRangeAnalysis
        {
            QStringList shas;
            bool isContiguousRange = false;
            bool blockedByBranchScope = false;
            QString baseRevision;
            QString headSha;
            QString referenceBranch;
        };

        /// Returns true if `index` refers to a real Git commit (not WIP and not a stash entry).
        [[nodiscard]] Q_INVOKABLE bool isSelectableCommitRow(int index) const noexcept;
        /// Returns true if `shaA` and `shaB` lie on the same `--first-parent` chain in the DAG.
        [[nodiscard]] bool isOnFirstParentLine(const QString& shaA, const QString& shaB) const;
        /// Resolves a Shift+Click range between `anchorIndex` and `targetIndex`.
        /// When `branchOnly` is true, follows only the `--first-parent` chain between the two commits.
        [[nodiscard]] MultiSelectRangeAnalysis resolveRangeSelection(int anchorIndex, int targetIndex, bool branchOnly) const;
        /// Filters `shas` keeping only commits on the `--first-parent` line of `anchorSha`, ordered as in the view.
        [[nodiscard]] QStringList filterCommitsByFirstParentLine(const QStringList& shas, const QString& anchorSha) const;
        /// Orders `shas` according to the current history list and checks whether they form a contiguous `--first-parent` chain.
        [[nodiscard]] MultiSelectRangeAnalysis analyzeSelection(const QStringList& shas, const QString& anchorSha) const;
        /// Returns the inferred branch label for a commit SHA, or empty if unknown.
        [[nodiscard]] QString inferredBranchForSha(const QString& sha) const;

        [[nodiscard]] static QString cachedAvatarForEmail(const QString& rawEmail);
        static void cacheAvatarForEmail(const QString& rawEmail, const QString& avatarUrl);

        signals :

        void maxLanesChanged();
        void searchFilterChanged(const QString& filter);
        void totalCommitsCountChanged();

    private:
        struct GraphItem
        {
            int lane = 0;
            QString color;
            QVariantList routes;
            bool isOrphan = false;
            bool isStash = false;
            QString stashSelector;
        };

        struct InferredBranchInfo
        {
            QString name;
            QString borderColor;
            QString bgColor;
        };

        void indexAvatarsFromCommits();
        void resolveMissingAvatarsFromGitHub();
        void applyWipAndRebuild();
        void rebuildGraph();
        void computeBranchCommitCounts();
        void computeInferredBranches();
        [[nodiscard]] int branchCommitCountForCommit(const git::bo::GitCommitLogItem& commit) const noexcept;
        [[nodiscard]] const InferredBranchInfo* inferredBranchForCommit(const git::bo::GitCommitLogItem& commit) const noexcept;

        QString m_searchFilter;
        std::vector<git::bo::GitCommitLogItem> m_rawCommits;
        std::vector<git::bo::GitCommitLogItem> m_commits;
        std::vector<GraphItem> m_graphItems;
        std::unordered_map<std::string, int> m_branchCommitCounts;
        std::unordered_map<std::string, InferredBranchInfo> m_inferredBranches;
        std::unordered_map<std::string, int> m_rawShaToIdx;
        std::vector<std::vector<int>> m_rawParentsIdx;
        mutable std::unordered_map<std::string, int> m_reachableCountCache;
        mutable std::vector<std::uint32_t> m_reachableVisitStamp;
        mutable std::uint32_t m_reachableCurrentStamp = 0;
        QNetworkAccessManager m_networkManager;
        QSet<QString> m_queriedEmails;
        QString m_headSha;
        bool m_isFullDag = true;
        bool m_hasWip = false;
        int m_wipCount = 0;
        int m_maxLanes = 1;
    };
} // namespace gf::desktop
