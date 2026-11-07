#include "desktop/viewmodels/group_list_model.hpp"

#include "core/modules/repositories/bl/repository_service.hpp"

namespace gf::desktop {

GroupListModel::GroupListModel(QObject* parent)
    : QAbstractListModel(parent) {}

int GroupListModel::rowCount(const QModelIndex& parent) const {
    if (parent.isValid()) {
        return 0;
    }
    return static_cast<int>(m_items.size());
}

QVariant GroupListModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid() || index.row() < 0 || static_cast<size_t>(index.row()) >= m_items.size()) {
        return {};
    }

    const auto& item = m_items[static_cast<size_t>(index.row())];

    switch (role) {
    case IdRole:
        return item.id;
    case NameRole:
        return item.name;
    case RepositoryCountRole:
        return item.repositoryCount;
    case RepositoryIdsRole:
        return item.repositoryIds;
    case RepositoryNamesRole:
        return item.repositoryNames;
    case Qt::DisplayRole:
        return item.name;
    default:
        return {};
    }
}

QHash<int, QByteArray> GroupListModel::roleNames() const {
    return {
        {IdRole, "id"},
        {NameRole, "name"},
        {RepositoryCountRole, "repositoryCount"},
        {RepositoryIdsRole, "repositoryIds"},
        {RepositoryNamesRole, "repositoryNames"}
    };
}

void GroupListModel::setGroups(const std::vector<gf::groups::bo::Group>& groups,
                               gf::repositories::bl::RepositoryService* repoService) {
    beginResetModel();
    m_items.clear();
    m_items.reserve(groups.size());

    for (const auto& group : groups) {
        GroupItem item;
        item.id = QString::fromStdString(group.id.value());
        item.name = QString::fromStdString(group.name);
        item.repositoryCount = static_cast<int>(group.repositoryIds.size());
        
        for (const auto& repoId : group.repositoryIds) {
            item.repositoryIds.append(QString::fromStdString(repoId.value()));
            if (repoService) {
                auto rRes = repoService->getRepository(repoId);
                if (rRes && rRes->has_value()) {
                    item.repositoryNames.append(QString::fromStdString((*rRes)->displayName));
                } else {
                    item.repositoryNames.append(QString::fromStdString(repoId.value()));
                }
            }
        }

        m_items.push_back(std::move(item));
    }
    endResetModel();
}

const std::vector<GroupItem>& GroupListModel::items() const noexcept {
    return m_items;
}

std::optional<GroupItem> GroupListModel::findById(const QString& id) const {
    for (const auto& item : m_items) {
        if (item.id == id) {
            return item;
        }
    }
    return std::nullopt;
}

} // namespace gf::desktop
