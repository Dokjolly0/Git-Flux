#include "desktop/viewmodels/working_tree_file_list_model.hpp"

namespace gf::desktop {

WorkingTreeFileListModel::WorkingTreeFileListModel(QObject* parent)
    : QAbstractListModel(parent) {}

int WorkingTreeFileListModel::rowCount(const QModelIndex& parent) const {
    if (parent.isValid()) {
        return 0;
    }
    return static_cast<int>(m_items.size());
}

QVariant WorkingTreeFileListModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid() || index.row() < 0 ||
        index.row() >= static_cast<int>(m_items.size())) {
        return {};
    }

    const auto& item = m_items[static_cast<std::size_t>(index.row())];

    switch (role) {
    case PathRole:
    case Qt::DisplayRole:
        return QString::fromStdString(item.path);

    case OriginalPathRole:
        return item.originalPath.empty()
                   ? QString{}
                   : QString::fromStdString(item.originalPath);

    case KindRole:
        switch (item.kind) {
        case gf::git::bo::FileChangeKind::Modified:   return QStringLiteral("Modified");
        case gf::git::bo::FileChangeKind::Added:      return QStringLiteral("Added");
        case gf::git::bo::FileChangeKind::Deleted:    return QStringLiteral("Deleted");
        case gf::git::bo::FileChangeKind::Renamed:    return QStringLiteral("Renamed");
        case gf::git::bo::FileChangeKind::Copied:     return QStringLiteral("Copied");
        case gf::git::bo::FileChangeKind::Untracked:  return QStringLiteral("Untracked");
        case gf::git::bo::FileChangeKind::Conflicted: return QStringLiteral("Conflicted");
        }
        return QStringLiteral("Unknown");

    case StatusCodeRole:
        return QString(QChar(item.statusCode));

    case BadgeColorRole:
        switch (item.kind) {
        case gf::git::bo::FileChangeKind::Added:
        case gf::git::bo::FileChangeKind::Untracked:
            return QStringLiteral("#34D399"); // Emerald green
        case gf::git::bo::FileChangeKind::Modified:
            return QStringLiteral("#FBBF24"); // Amber yellow
        case gf::git::bo::FileChangeKind::Deleted:
            return QStringLiteral("#F87171"); // Coral red
        case gf::git::bo::FileChangeKind::Renamed:
        case gf::git::bo::FileChangeKind::Copied:
            return QStringLiteral("#60A5FA"); // Sky blue
        case gf::git::bo::FileChangeKind::Conflicted:
            return QStringLiteral("#F43F5E"); // Rose magenta
        }
        return QStringLiteral("#94A3B8");

    default:
        return {};
    }
}

QHash<int, QByteArray> WorkingTreeFileListModel::roleNames() const {
    return {
        {PathRole, "path"},
        {OriginalPathRole, "originalPath"},
        {KindRole, "kind"},
        {StatusCodeRole, "statusCode"},
        {BadgeColorRole, "badgeColor"}
    };
}

void WorkingTreeFileListModel::setItems(std::vector<gf::git::bo::WorkingTreeFileItem> items) {
    beginResetModel();
    m_items = std::move(items);
    endResetModel();
}

void WorkingTreeFileListModel::clear() {
    beginResetModel();
    m_items.clear();
    endResetModel();
}

} // namespace gf::desktop
