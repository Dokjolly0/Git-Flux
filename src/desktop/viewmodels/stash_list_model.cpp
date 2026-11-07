#include "desktop/viewmodels/stash_list_model.hpp"

namespace gf::desktop {

StashListModel::StashListModel(QObject* parent)
    : QAbstractListModel(parent) {}

int StashListModel::rowCount(const QModelIndex& parent) const {
    if (parent.isValid()) {
        return 0;
    }
    return static_cast<int>(m_stashes.size());
}

QVariant StashListModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid() || index.row() < 0 || static_cast<std::size_t>(index.row()) >= m_stashes.size()) {
        return {};
    }

    const auto& item = m_stashes[static_cast<std::size_t>(index.row())];

    switch (role) {
    case IndexRole:
        return static_cast<qlonglong>(item.index);
    case SelectorRole:
        return QString::fromStdString(item.selector);
    case CommitShaRole:
        return QString::fromStdString(item.commitSha);
    case DateIsoRole:
        return QString::fromStdString(item.dateIso);
    case MessageRole:
        return QString::fromStdString(item.message);
    case BranchNameRole:
        return QString::fromStdString(item.branchName);
    case Qt::DisplayRole:
        return QString::fromStdString(item.selector + " " + item.message);
    default:
        return {};
    }
}

QHash<int, QByteArray> StashListModel::roleNames() const {
    return {
        {IndexRole, "stashIndex"},
        {SelectorRole, "selector"},
        {CommitShaRole, "commitSha"},
        {DateIsoRole, "dateIso"},
        {MessageRole, "message"},
        {BranchNameRole, "branchName"}
    };
}

void StashListModel::setStashes(std::vector<gf::git::bo::GitStashItem> stashes) {
    beginResetModel();
    m_stashes = std::move(stashes);
    endResetModel();
    emit countChanged();
}

void StashListModel::clear() {
    beginResetModel();
    m_stashes.clear();
    endResetModel();
    emit countChanged();
}

} // namespace gf::desktop
