#include "desktop/viewmodels/workspace_list_model.hpp"
#include "core/modules/workspaces/bo/workspace.hpp"
#include "core/modules/workspaces/bl/workspace_service.hpp"

#include <chrono>
#include <ctime>

namespace gf::desktop {

namespace {

QString formatTimestampMs(std::int64_t epochMs) {
    if (epochMs <= 0) {
        return QStringLiteral("-");
    }
    const auto time = static_cast<std::time_t>(epochMs / 1000);
    std::tm tmBuffer{};
#if defined(_WIN32)
    gmtime_s(&tmBuffer, &time);
#else
    gmtime_r(&time, &tmBuffer);
#endif
    return QString::asprintf("%04d-%02d-%02d %02d:%02d",
                             tmBuffer.tm_year + 1900,
                             tmBuffer.tm_mon + 1,
                             tmBuffer.tm_mday,
                             tmBuffer.tm_hour,
                             tmBuffer.tm_min);
}

QString formatSummary(int groupCount, int directRepoCount) {
    if (groupCount > 0 && directRepoCount > 0) {
        return QStringLiteral("%1 grupp%2, %3 repo singol%4")
            .arg(groupCount)
            .arg(groupCount > 1 ? QStringLiteral("i") : QStringLiteral("o"))
            .arg(directRepoCount)
            .arg(directRepoCount > 1 ? QStringLiteral("i") : QStringLiteral("o"));
    }
    if (groupCount > 0) {
        return QStringLiteral("%1 grupp%2")
            .arg(groupCount)
            .arg(groupCount > 1 ? QStringLiteral("i") : QStringLiteral("o"));
    }
    if (directRepoCount > 0) {
        return QStringLiteral("%1 repo singol%2")
            .arg(directRepoCount)
            .arg(directRepoCount > 1 ? QStringLiteral("i") : QStringLiteral("o"));
    }
    return QStringLiteral("Vuoto");
}

} // namespace

WorkspaceListModel::WorkspaceListModel(QObject* parent)
    : QAbstractListModel(parent) {}

int WorkspaceListModel::rowCount(const QModelIndex& parent) const {
    if (parent.isValid()) {
        return 0;
    }
    return static_cast<int>(m_items.size());
}

QVariant WorkspaceListModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid() || index.row() < 0 || static_cast<size_t>(index.row()) >= m_items.size()) {
        return {};
    }

    const auto& item = m_items[static_cast<size_t>(index.row())];

    switch (role) {
    case IdRole:
        return item.id;
    case NameRole:
        return item.name;
    case GroupCountRole:
        return item.groupCount;
    case DirectRepoCountRole:
        return item.directRepoCount;
    case TotalRepoCountRole:
        return item.totalRepoCount;
    case SummaryRole:
        return item.summary;
    case LastOpenedAtRole:
        return item.lastOpenedAt;
    case Qt::DisplayRole:
        return item.name;
    default:
        return {};
    }
}

QHash<int, QByteArray> WorkspaceListModel::roleNames() const {
    return {
        {IdRole, "id"},
        {NameRole, "name"},
        {GroupCountRole, "groupCount"},
        {DirectRepoCountRole, "directRepoCount"},
        {TotalRepoCountRole, "totalRepoCount"},
        {SummaryRole, "summary"},
        {LastOpenedAtRole, "lastOpenedAt"}
    };
}

void WorkspaceListModel::setWorkspaces(const std::vector<gf::workspaces::bo::Workspace>& workspaces,
                                       gf::workspaces::bl::WorkspaceService& workspaceService) {
    beginResetModel();
    m_items.clear();
    m_items.reserve(workspaces.size());

    for (const auto& ws : workspaces) {
        WorkspaceItem item;
        item.id = QString::fromStdString(ws.id.value());
        item.name = QString::fromStdString(ws.name);
        item.groupCount = static_cast<int>(ws.groupIds.size());
        item.directRepoCount = static_cast<int>(ws.directRepositoryIds.size());

        auto resolved = workspaceService.resolveWorkspaceRepositories(ws.id);
        if (resolved) {
            item.totalRepoCount = static_cast<int>(resolved->size());
        } else {
            item.totalRepoCount = item.directRepoCount;
        }

        item.summary = formatSummary(item.groupCount, item.directRepoCount);
        item.lastOpenedAt = formatTimestampMs(ws.updatedAtUnixEpochMs);

        m_items.push_back(std::move(item));
    }
    endResetModel();
}

const std::vector<WorkspaceItem>& WorkspaceListModel::items() const noexcept {
    return m_items;
}

std::optional<WorkspaceItem> WorkspaceListModel::findById(const QString& id) const {
    for (const auto& item : m_items) {
        if (item.id == id) {
            return item;
        }
    }
    return std::nullopt;
}

} // namespace gf::desktop
