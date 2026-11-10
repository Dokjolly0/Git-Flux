#pragma once

#include <QAbstractListModel>
#include <QNetworkAccessManager>
#include <QSet>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>
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
            IsOrphanRole
        };

        explicit CommitHistoryListModel(QObject* parent = nullptr);
        ~CommitHistoryListModel() override = default;

        [[nodiscard]] int rowCount(const QModelIndex& parent = QModelIndex()) const override;
        [[nodiscard]] QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
        [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

        void setCommits(std::vector<git::bo::GitCommitLogItem> commits);
        void setWip(bool hasWip, int changedFilesCount = 0);
        [[nodiscard]] bool hasWip() const noexcept { return m_hasWip; }
        [[nodiscard]] const std::vector<git::bo::GitCommitLogItem>& commits() const noexcept { return m_commits; }

        [[nodiscard]] int maxLanes() const noexcept { return m_maxLanes; }
        [[nodiscard]] QString searchFilter() const noexcept { return m_searchFilter; }
        void setSearchFilter(const QString& filter);
        [[nodiscard]] int totalCommitsCount() const noexcept { return static_cast<int>(m_rawCommits.size()); }
        Q_INVOKABLE [[nodiscard]] QVariantMap getCommit(int index) const;

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
        };

        void indexAvatarsFromCommits();
        void resolveMissingAvatarsFromGitHub();
        void applyWipAndRebuild();
        void rebuildGraph();

        QString m_searchFilter;
        std::vector<git::bo::GitCommitLogItem> m_rawCommits;
        std::vector<git::bo::GitCommitLogItem> m_commits;
        std::vector<GraphItem> m_graphItems;
        QNetworkAccessManager m_networkManager;
        QSet<QString> m_queriedEmails;
        bool m_hasWip = false;
        int m_wipCount = 0;
        int m_maxLanes = 1;
    };
} // namespace gf::desktop
