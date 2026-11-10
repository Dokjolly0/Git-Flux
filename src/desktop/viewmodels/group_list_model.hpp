#pragma once

#include <QAbstractListModel>
#include <QString>
#include <QStringList>
#include <optional>
#include <vector>

namespace gf::groups::bo
{
    struct Group;
}

namespace gf::repositories::bl
{
    class RepositoryService;
}

namespace gf::desktop
{
    struct GroupItem
    {
        QString id;
        QString name;
        int repositoryCount{0};
        QStringList repositoryIds;
        QStringList repositoryNames;
    };

    class GroupListModel : public QAbstractListModel
    {
        Q_OBJECT

    public:
        enum GroupRoles
        {
            IdRole = Qt::UserRole + 1,
            NameRole,
            RepositoryCountRole,
            RepositoryIdsRole,
            RepositoryNamesRole
        };

        explicit GroupListModel(QObject* parent = nullptr);
        ~GroupListModel() override = default;

        [[nodiscard]] int rowCount(const QModelIndex& parent = QModelIndex()) const override;
        [[nodiscard]] QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
        [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

        void setGroups(const std::vector<gf::groups::bo::Group>& groups,
                       gf::repositories::bl::RepositoryService* repoService = nullptr);
        [[nodiscard]] const std::vector<GroupItem>& items() const noexcept;
        [[nodiscard]] std::optional<GroupItem> findById(const QString& id) const;

    private:
        std::vector<GroupItem> m_items;
    };
} // namespace gf::desktop
