#pragma once

#include "core/modules/git/bo/history_models.hpp"

#include <QAbstractListModel>
#include <QStringList>
#include <vector>

namespace gf::desktop {

class CommitHistoryListModel final : public QAbstractListModel {
    Q_OBJECT

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
        StructuredDecorationsRole
    };

    explicit CommitHistoryListModel(QObject* parent = nullptr);
    ~CommitHistoryListModel() override = default;

    [[nodiscard]] int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    [[nodiscard]] QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

    void setCommits(std::vector<git::bo::GitCommitLogItem> commits);
    [[nodiscard]] const std::vector<git::bo::GitCommitLogItem>& commits() const noexcept { return m_commits; }

private:
    std::vector<git::bo::GitCommitLogItem> m_commits;
};

} // namespace gf::desktop
