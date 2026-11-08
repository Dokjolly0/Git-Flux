#pragma once

#include <QAbstractListModel>
#include <QString>
#include <optional>
#include <vector>

namespace gf::workspaces::bo {
struct Workspace;
}

namespace gf::workspaces::bl {
class WorkspaceService;
}

namespace gf::groups::bl {
class GroupService;
}

namespace gf::desktop {

struct WorkspaceItem {
    QString id;
    QString name;
    int groupCount{0};
    int directRepoCount{0};
    int totalRepoCount{0};
    QString summary;
    QString lastOpenedAt;
    QStringList repositoryIds;
    QStringList repositoryNames;
    QStringList groupIds;
    QStringList groupNames;
};

class WorkspaceListModel : public QAbstractListModel {
    Q_OBJECT

public:
    enum WorkspaceRoles {
        IdRole = Qt::UserRole + 1,
        NameRole,
        GroupCountRole,
        DirectRepoCountRole,
        TotalRepoCountRole,
        SummaryRole,
        LastOpenedAtRole,
        RepositoryIdsRole,
        RepositoryNamesRole,
        GroupIdsRole,
        GroupNamesRole
    };

    explicit WorkspaceListModel(QObject* parent = nullptr);
    ~WorkspaceListModel() override = default;

    [[nodiscard]] int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    [[nodiscard]] QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

    void setWorkspaces(const std::vector<gf::workspaces::bo::Workspace>& workspaces,
                       gf::workspaces::bl::WorkspaceService& workspaceService,
                       gf::groups::bl::GroupService* groupService = nullptr);
    [[nodiscard]] const std::vector<WorkspaceItem>& items() const noexcept;
    [[nodiscard]] std::optional<WorkspaceItem> findById(const QString& id) const;

private:
    std::vector<WorkspaceItem> m_items;
};

} // namespace gf::desktop
