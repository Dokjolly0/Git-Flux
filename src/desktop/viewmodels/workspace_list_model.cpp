#include "desktop/viewmodels/workspace_list_model.hpp"
#include "core/modules/workspaces/bo/workspace.hpp"
#include "core/modules/workspaces/bl/workspace_service.hpp"
#include "core/modules/groups/bl/group_service.hpp"
#include "core/modules/groups/bo/group.hpp"

#include <chrono>
#include <ctime>
#include <unordered_set>

namespace gf::desktop
{
    namespace
    {
        QString formatTimestampMs(std::int64_t epochMs)
        {
            if (epochMs <= 0)
            {
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

        QString formatSummary(int groupCount, int directRepoCount)
        {
            if (groupCount > 0 && directRepoCount > 0)
            {
                return QStringLiteral("%1 grupp%2, %3 repo singol%4")
                       .arg(groupCount)
                       .arg(groupCount > 1 ? QStringLiteral("i") : QStringLiteral("o"))
                       .arg(directRepoCount)
                       .arg(directRepoCount > 1 ? QStringLiteral("i") : QStringLiteral("o"));
            }
            if (groupCount > 0)
            {
                return QStringLiteral("%1 grupp%2")
                       .arg(groupCount)
                       .arg(groupCount > 1 ? QStringLiteral("i") : QStringLiteral("o"));
            }
            if (directRepoCount > 0)
            {
                return QStringLiteral("%1 repo singol%2")
                       .arg(directRepoCount)
                       .arg(directRepoCount > 1 ? QStringLiteral("i") : QStringLiteral("o"));
            }
            return QStringLiteral("Vuoto");
        }
    } // namespace

    WorkspaceListModel::WorkspaceListModel(QObject* parent)
        : QAbstractListModel(parent)
    {
    }

    int WorkspaceListModel::rowCount(const QModelIndex& parent) const
    {
        if (parent.isValid())
        {
            return 0;
        }
        return static_cast<int>(m_items.size());
    }

    QVariant WorkspaceListModel::data(const QModelIndex& index, int role) const
    {
        if (!index.isValid() || index.row() < 0 || static_cast<size_t>(index.row()) >= m_items.size())
        {
            return {};
        }

        const auto& item = m_items[static_cast<size_t>(index.row())];

        switch (role)
        {
        case IdRole:
            return item.id;
        case NameRole:
            return item.name;
        case ColorRole:
            return item.color;
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
        case RepositoryIdsRole:
            return item.repositoryIds;
        case RepositoryNamesRole:
            return item.repositoryNames;
        case RepositoryGroupColorsRole:
            return item.repositoryGroupColors;
        case RepositoryGroupNamesRole:
            return item.repositoryGroupNames;
        case GroupIdsRole:
            return item.groupIds;
        case GroupNamesRole:
            return item.groupNames;
        case GroupColorsRole:
            return item.groupColors;
        case Qt::DisplayRole:
            return item.name;
        default:
            return {};
        }
    }

    QHash<int, QByteArray> WorkspaceListModel::roleNames() const
    {
        return {
            {IdRole, "id"},
            {NameRole, "name"},
            {ColorRole, "workspaceColor"},
            {GroupCountRole, "groupCount"},
            {DirectRepoCountRole, "directRepoCount"},
            {TotalRepoCountRole, "totalRepoCount"},
            {SummaryRole, "summary"},
            {LastOpenedAtRole, "lastOpenedAt"},
            {RepositoryIdsRole, "repositoryIds"},
            {RepositoryNamesRole, "repositoryNames"},
            {RepositoryGroupColorsRole, "repositoryGroupColors"},
            {RepositoryGroupNamesRole, "repositoryGroupNames"},
            {GroupIdsRole, "groupIds"},
            {GroupNamesRole, "groupNames"},
            {GroupColorsRole, "groupColors"}
        };
    }

    void WorkspaceListModel::setWorkspaces(const std::vector<gf::workspaces::bo::Workspace>& workspaces,
                                           gf::workspaces::bl::WorkspaceService& workspaceService,
                                           gf::groups::bl::GroupService* groupService)
    {
        beginResetModel();
        m_items.clear();
        m_items.reserve(workspaces.size());

        struct WorkspaceGroupMeta
        {
            QString name;
            QString color;
            std::unordered_set<std::string> repoIds;
        };

        for (const auto& ws : workspaces)
        {
            WorkspaceItem item;
            item.id = QString::fromStdString(ws.id.value());
            item.name = QString::fromStdString(ws.name);
            const auto wsCol = ws.resolvedColor();
            item.color = QString::fromUtf8(wsCol.data(), static_cast<qsizetype>(wsCol.size()));
            item.groupCount = static_cast<int>(ws.groupIds.size());
            item.directRepoCount = static_cast<int>(ws.directRepositoryIds.size());

            std::vector<WorkspaceGroupMeta> workspaceGroups;
            workspaceGroups.reserve(ws.groupIds.size());

            for (const auto& gId : ws.groupIds)
            {
                item.groupIds.append(QString::fromStdString(gId.value()));
                WorkspaceGroupMeta meta;
                if (groupService)
                {
                    auto grpOpt = groupService->getGroup(gId);
                    if (grpOpt && grpOpt->has_value())
                    {
                        meta.name = QString::fromStdString((*grpOpt)->name);
                        const auto col = (*grpOpt)->resolvedColor();
                        meta.color = QString::fromUtf8(col.data(), static_cast<qsizetype>(col.size()));
                        meta.repoIds.reserve((*grpOpt)->repositoryIds.size());
                        for (const auto& rId : (*grpOpt)->repositoryIds)
                        {
                            meta.repoIds.insert(rId.value());
                        }
                    }
                    else
                    {
                        meta.name = QString::fromStdString(gId.value());
                        const auto col = gf::groups::bo::defaultGroupColorForId(gId.value());
                        meta.color = QString::fromUtf8(col.data(), static_cast<qsizetype>(col.size()));
                    }
                }
                else
                {
                    meta.name = QString::fromStdString(gId.value());
                    const auto col = gf::groups::bo::defaultGroupColorForId(gId.value());
                    meta.color = QString::fromUtf8(col.data(), static_cast<qsizetype>(col.size()));
                }
                item.groupNames.append(meta.name);
                item.groupColors.append(meta.color);
                workspaceGroups.push_back(std::move(meta));
            }

            auto resolved = workspaceService.resolveWorkspaceRepositories(ws.id);
            if (resolved)
            {
                item.totalRepoCount = static_cast<int>(resolved->size());
                for (const auto& r : *resolved)
                {
                    item.repositoryIds.append(QString::fromStdString(r.id.value()));
                    item.repositoryNames.append(QString::fromStdString(r.displayName));

                    QStringList repoGroupColors;
                    QStringList repoGroupNames;
                    for (const auto& gMeta : workspaceGroups)
                    {
                        if (gMeta.repoIds.contains(r.id.value()))
                        {
                            repoGroupColors.append(gMeta.color);
                            repoGroupNames.append(gMeta.name);
                        }
                    }
                    item.repositoryGroupColors.append(repoGroupColors);
                    item.repositoryGroupNames.append(repoGroupNames);
                }
            }
            else
            {
                item.totalRepoCount = item.directRepoCount;
            }

            item.summary = formatSummary(item.groupCount, item.directRepoCount);
            item.lastOpenedAt = formatTimestampMs(ws.updatedAtUnixEpochMs);

            m_items.push_back(std::move(item));
        }
        endResetModel();
    }

    std::optional<WorkspaceItem> WorkspaceListModel::findById(const QString& id) const
    {
        for (const auto& item : m_items)
        {
            if (item.id == id)
            {
                return item;
            }
        }
        return std::nullopt;
    }
} // namespace gf::desktop
