#include "desktop/viewmodels/remote_repo_list_model.hpp"

namespace gf::desktop::viewmodels {

RemoteRepoListModel::RemoteRepoListModel(QObject* parent)
    : QAbstractListModel(parent) {}

int RemoteRepoListModel::rowCount(const QModelIndex& parent) const {
    if (parent.isValid()) return 0;
    return static_cast<int>(m_filteredIndices.size());
}

QVariant RemoteRepoListModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid() || index.row() < 0 || static_cast<size_t>(index.row()) >= m_filteredIndices.size()) {
        return {};
    }

    const auto originalIndex = m_filteredIndices[static_cast<size_t>(index.row())];
    const auto& item = m_allRepos[originalIndex];

    switch (role) {
        case NameRole:
            return QString::fromStdString(item.name);
        case FullNameRole:
            return QString::fromStdString(item.fullName);
        case DescriptionRole:
            return QString::fromStdString(item.description);
        case IsPrivateRole:
            return item.isPrivate;
        case DefaultBranchRole:
            return QString::fromStdString(item.defaultBranch);
        case HttpsUrlRole:
            return QString::fromStdString(item.httpsUrl);
        case SshUrlRole:
            return QString::fromStdString(item.sshUrl);
        case ActiveCloneUrlRole: {
            if (m_selectedProtocol == QStringLiteral("ssh") && !item.sshUrl.empty()) {
                return QString::fromStdString(item.sshUrl);
            }
            if (!item.httpsUrl.empty()) {
                return QString::fromStdString(item.httpsUrl);
            }
            return QString::fromStdString(item.sshUrl);
        }
        case UpdatedAtRole:
            return QString::fromStdString(item.updatedAt);
        case StarsCountRole:
            return item.starsCount;
        default:
            return {};
    }
}

QHash<int, QByteArray> RemoteRepoListModel::roleNames() const {
    return {
        {NameRole, "name"},
        {FullNameRole, "fullName"},
        {DescriptionRole, "description"},
        {IsPrivateRole, "isPrivate"},
        {DefaultBranchRole, "defaultBranch"},
        {HttpsUrlRole, "httpsUrl"},
        {SshUrlRole, "sshUrl"},
        {ActiveCloneUrlRole, "activeCloneUrl"},
        {UpdatedAtRole, "updatedAt"},
        {StarsCountRole, "starsCount"}
    };
}

void RemoteRepoListModel::setSearchFilter(const QString& filter) {
    if (m_searchFilter == filter) return;
    m_searchFilter = filter;
    emit searchFilterChanged();
    applyFilter();
}

void RemoteRepoListModel::setSelectedProtocol(const QString& proto) {
    if (m_selectedProtocol == proto) return;
    m_selectedProtocol = proto;
    emit selectedProtocolChanged();

    if (!m_filteredIndices.empty()) {
        emit dataChanged(index(0, 0), index(static_cast<int>(m_filteredIndices.size()) - 1, 0), {ActiveCloneUrlRole});
    }
}

void RemoteRepoListModel::setRepositories(std::vector<core::git::bo::RemoteRepositoryItem> repos) {
    beginResetModel();
    m_allRepos = std::move(repos);
    m_filteredIndices.clear();
    m_filteredIndices.reserve(m_allRepos.size());
    for (size_t i = 0; i < m_allRepos.size(); ++i) {
        m_filteredIndices.push_back(i);
    }
    endResetModel();
    emit countChanged();

    if (!m_searchFilter.isEmpty()) {
        applyFilter();
    }
}

void RemoteRepoListModel::clear() {
    beginResetModel();
    m_allRepos.clear();
    m_filteredIndices.clear();
    m_searchFilter.clear();
    endResetModel();
    emit searchFilterChanged();
    emit countChanged();
}

void RemoteRepoListModel::applyFilter() {
    beginResetModel();
    m_filteredIndices.clear();

    const auto trimmed = m_searchFilter.trimmed();
    if (trimmed.isEmpty()) {
        m_filteredIndices.reserve(m_allRepos.size());
        for (size_t i = 0; i < m_allRepos.size(); ++i) {
            m_filteredIndices.push_back(i);
        }
    } else {
        for (size_t i = 0; i < m_allRepos.size(); ++i) {
            const auto& item = m_allRepos[i];
            const auto name = QString::fromStdString(item.name);
            const auto fullName = QString::fromStdString(item.fullName);
            const auto desc = QString::fromStdString(item.description);

            if (name.contains(trimmed, Qt::CaseInsensitive) ||
                fullName.contains(trimmed, Qt::CaseInsensitive) ||
                desc.contains(trimmed, Qt::CaseInsensitive)) {
                m_filteredIndices.push_back(i);
            }
        }
    }
    endResetModel();
    emit countChanged();
}

QVariantMap RemoteRepoListModel::getRepoAt(int index) const {
    if (index < 0 || static_cast<size_t>(index) >= m_filteredIndices.size()) {
        return {};
    }
    const auto origIdx = m_filteredIndices[static_cast<size_t>(index)];
    const auto& item = m_allRepos[origIdx];

    QVariantMap map;
    map[QStringLiteral("name")] = QString::fromStdString(item.name);
    map[QStringLiteral("fullName")] = QString::fromStdString(item.fullName);
    map[QStringLiteral("description")] = QString::fromStdString(item.description);
    map[QStringLiteral("isPrivate")] = item.isPrivate;
    map[QStringLiteral("httpsUrl")] = QString::fromStdString(item.httpsUrl);
    map[QStringLiteral("sshUrl")] = QString::fromStdString(item.sshUrl);
    map[QStringLiteral("activeCloneUrl")] = (m_selectedProtocol == QStringLiteral("ssh") && !item.sshUrl.empty())
                                               ? QString::fromStdString(item.sshUrl)
                                               : QString::fromStdString(item.httpsUrl);
    return map;
}

} // namespace gf::desktop::viewmodels
