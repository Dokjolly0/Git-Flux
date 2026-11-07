#pragma once

#include <QAbstractListModel>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>
#include <vector>

#ifndef Q_MOC_RUN
#include "core/modules/git/bo/history_models.hpp"
#else
namespace gf::git::bo {
struct GitCommitLogItem;
}
#endif

namespace gf::desktop {

class CommitHistoryListModel final : public QAbstractListModel {
    Q_OBJECT

    Q_PROPERTY(int maxLanes READ maxLanes NOTIFY maxLanesChanged)

public:
    enum Roles {
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
        IsWipRole
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
    Q_INVOKABLE [[nodiscard]] QVariantMap getCommit(int index) const;

signals:
    void maxLanesChanged();

private:
    struct GraphItem {
        int lane = 0;
        QString color;
        QVariantList routes;
    };

    void applyWipAndRebuild();
    void rebuildGraph();

    std::vector<git::bo::GitCommitLogItem> m_rawCommits;
    std::vector<git::bo::GitCommitLogItem> m_commits;
    std::vector<GraphItem> m_graphItems;
    bool m_hasWip = false;
    int m_wipCount = 0;
    int m_maxLanes = 1;
};

} // namespace gf::desktop
