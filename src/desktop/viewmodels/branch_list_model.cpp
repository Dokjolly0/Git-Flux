#include "desktop/viewmodels/branch_list_model.hpp"

namespace gf::desktop {

BranchListModel::BranchListModel(QObject* parent)
    : QAbstractListModel(parent) {}

int BranchListModel::rowCount(const QModelIndex& parent) const {
    if (parent.isValid()) {
        return 0;
    }
    return static_cast<int>(m_branches.size());
}

QVariant BranchListModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid() || index.row() < 0 ||
        index.row() >= static_cast<int>(m_branches.size())) {
        return {};
    }

    const auto& b = m_branches[static_cast<std::size_t>(index.row())];

    switch (role) {
    case Qt::DisplayRole:
    case NameRole:
        return QString::fromStdString(b.name);
    case ShortNameRole:
        return QString::fromStdString(b.shortName);
    case IsCurrentRole:
        return b.isCurrent;
    case IsRemoteRole:
        return b.type == git::bo::BranchType::Remote;
    case UpstreamRole:
        return b.upstream.empty() ? QString{} : QString::fromStdString(b.upstream);
    case AheadCountRole:
        return static_cast<qlonglong>(b.ahead);
    case BehindCountRole:
        return static_cast<qlonglong>(b.behind);
    case TargetShaRole:
        return QString::fromStdString(b.targetCommitSha);
    default:
        return {};
    }
}

QHash<int, QByteArray> BranchListModel::roleNames() const {
    return {
        {NameRole, "name"},
        {ShortNameRole, "shortName"},
        {IsCurrentRole, "isCurrent"},
        {IsRemoteRole, "isRemote"},
        {UpstreamRole, "upstream"},
        {AheadCountRole, "ahead"},
        {BehindCountRole, "behind"},
        {TargetShaRole, "targetSha"}
    };
}

void BranchListModel::setBranches(std::vector<git::bo::GitBranch> branches) {
    beginResetModel();
    m_branches = std::move(branches);
    endResetModel();
}

} // namespace gf::desktop
