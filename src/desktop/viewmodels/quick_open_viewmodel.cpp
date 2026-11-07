#include "desktop/viewmodels/quick_open_viewmodel.hpp"

#include "core/modules/groups/bl/group_service.hpp"
#include "core/modules/repositories/bl/repository_service.hpp"

#include <QVariantMap>

namespace gf::desktop {

QuickOpenViewModel::QuickOpenViewModel(gf::groups::bl::GroupService& groupService,
                                       gf::repositories::bl::RepositoryService& repoService,
                                       QObject* parent)
    : QObject(parent),
      m_groupService(groupService),
      m_repoService(repoService) {}

QStringList QuickOpenViewModel::selectedGroupIds() const {
    return m_selectedGroupIds;
}

int QuickOpenViewModel::selectedGroupsCount() const noexcept {
    return static_cast<int>(m_selectedGroupIds.size());
}

bool QuickOpenViewModel::hasSelection() const noexcept {
    return !m_selectedGroupIds.isEmpty();
}

QVariantList QuickOpenViewModel::resolvedRepositories() const noexcept {
    return m_resolvedRepositories;
}

int QuickOpenViewModel::resolvedRepositoriesCount() const noexcept {
    return static_cast<int>(m_resolvedRepositories.size());
}

int QuickOpenViewModel::deduplicatedCount() const noexcept {
    return m_deduplicatedCount;
}

void QuickOpenViewModel::toggleGroup(const QString& groupId) {
    if (m_selectedGroupIds.contains(groupId)) {
        m_selectedGroupIds.removeAll(groupId);
    } else {
        m_selectedGroupIds.append(groupId);
    }
    emit selectionChanged();
    recalculateContext();
}

void QuickOpenViewModel::selectGroup(const QString& groupId, bool selected) {
    if (selected) {
        if (!m_selectedGroupIds.contains(groupId)) {
            m_selectedGroupIds.append(groupId);
        }
    } else {
        m_selectedGroupIds.removeAll(groupId);
    }
    emit selectionChanged();
    recalculateContext();
}

bool QuickOpenViewModel::isGroupSelected(const QString& groupId) const {
    return m_selectedGroupIds.contains(groupId);
}

void QuickOpenViewModel::clearSelection() {
    if (!m_selectedGroupIds.isEmpty()) {
        m_selectedGroupIds.clear();
        emit selectionChanged();
        recalculateContext();
    }
}

void QuickOpenViewModel::selectAllGroups() {
    const auto allGroupsResult = m_groupService.getAllGroups();
    m_selectedGroupIds.clear();
    if (allGroupsResult) {
        for (const auto& grp : *allGroupsResult) {
            m_selectedGroupIds.append(QString::fromStdString(grp.id.value()));
        }
    }
    emit selectionChanged();
    recalculateContext();
}

void QuickOpenViewModel::recalculateContext() {
    m_resolvedRepositories.clear();
    m_deduplicatedCount = 0;

    if (m_selectedGroupIds.isEmpty()) {
        emit resolvedRepositoriesChanged();
        return;
    }

    std::vector<gf::groups::bo::GroupId> groupIds;
    groupIds.reserve(static_cast<size_t>(m_selectedGroupIds.size()));

    for (const auto& gidStr : m_selectedGroupIds) {
        groupIds.push_back(gf::groups::bo::GroupId{gidStr.toStdString()});
    }

    auto resolved = m_groupService.resolveQuickOpenContext(groupIds);
    if (resolved) {
        for (const auto& repo : resolved->repositories) {
            QVariantMap map;
            map[QStringLiteral("id")] = QString::fromStdString(repo.id.value());
            map[QStringLiteral("displayName")] = QString::fromStdString(repo.displayName);
            map[QStringLiteral("localPath")] = QString::fromStdString(repo.localPath.string());
            map[QStringLiteral("isMissing")] = (repo.pathStatus == gf::repositories::bo::RepositoryPathStatus::Missing);
            m_resolvedRepositories.append(map);
        }

        if (resolved->totalRepositoryReferences > resolved->repositories.size()) {
            m_deduplicatedCount = static_cast<int>(resolved->totalRepositoryReferences - resolved->repositories.size());
        } else {
            m_deduplicatedCount = 0;
        }
    }

    emit resolvedRepositoriesChanged();
}

bool QuickOpenViewModel::openQuickSession() {
    if (m_resolvedRepositories.isEmpty()) {
        emit errorOccurred(QStringLiteral("Seleziona almeno un gruppo con repository validi per aprire una sessione rapida"));
        return false;
    }

    QStringList repoIds;
    repoIds.reserve(m_resolvedRepositories.size());
    for (const auto& itemVar : m_resolvedRepositories) {
        const auto map = itemVar.toMap();
        repoIds.append(map.value(QStringLiteral("id")).toString());
    }

    emit quickSessionStarted(repoIds);
    return true;
}

} // namespace gf::desktop
