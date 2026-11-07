#include "desktop/viewmodels/repository_list_model.hpp"

#include "core/modules/repositories/bo/repository.hpp"

#include <chrono>
#include <ctime>
#include <fstream>
#include <string_view>

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

QString readCurrentBranch(const std::filesystem::path& repoPath) {
    std::ifstream headFile(repoPath / ".git" / "HEAD");
    if (!headFile.is_open()) {
        return QStringLiteral("main");
    }
    std::string line;
    if (std::getline(headFile, line)) {
        while (!line.empty() && (line.back() == '\r' || line.back() == '\n' || line.back() == ' ')) {
            line.pop_back();
        }
        constexpr std::string_view kRefPrefix = "ref: refs/heads/";
        if (line.starts_with(kRefPrefix)) {
            return QString::fromStdString(line.substr(kRefPrefix.size()));
        }
        if (line.size() >= 7) {
            return QString::fromStdString(line.substr(0, 7));
        }
    }
    return QStringLiteral("main");
}

} // namespace

RepositoryListModel::RepositoryListModel(QObject* parent)
    : QAbstractListModel(parent) {}

int RepositoryListModel::rowCount(const QModelIndex& parent) const {
    if (parent.isValid()) {
        return 0;
    }
    return static_cast<int>(m_items.size());
}

QVariant RepositoryListModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid() || index.row() < 0 || static_cast<size_t>(index.row()) >= m_items.size()) {
        return {};
    }

    const auto& item = m_items[static_cast<size_t>(index.row())];

    switch (role) {
    case IdRole:
        return item.id;
    case DisplayNameRole:
        return item.displayName;
    case LocalPathRole:
        return item.localPath;
    case StatusRole:
        return item.status;
    case IsMissingRole:
        return item.isMissing;
    case LastAccessedAtRole:
        return item.lastAccessedAt;
    case BranchRole:
        return item.branch;
    case Qt::DisplayRole:
        return item.displayName;
    default:
        return {};
    }
}

QHash<int, QByteArray> RepositoryListModel::roleNames() const {
    return {
        {IdRole, "id"},
        {DisplayNameRole, "displayName"},
        {LocalPathRole, "localPath"},
        {StatusRole, "status"},
        {IsMissingRole, "isMissing"},
        {LastAccessedAtRole, "lastAccessedAt"},
        {BranchRole, "branch"}
    };
}

void RepositoryListModel::setRepositories(const std::vector<gf::repositories::bo::Repository>& repositories) {
    beginResetModel();
    m_items.clear();
    m_items.reserve(repositories.size());

    for (const auto& repo : repositories) {
        RepositoryItem item;
        item.id = QString::fromStdString(repo.id.value());
        item.displayName = QString::fromStdString(repo.displayName);
        item.localPath = QString::fromStdString(repo.localPath.string());
        item.isMissing = (repo.pathStatus == gf::repositories::bo::RepositoryPathStatus::Missing);
        item.branch = item.isMissing ? QString() : readCurrentBranch(repo.localPath);
        
        switch (repo.pathStatus) {
        case gf::repositories::bo::RepositoryPathStatus::Valid:
            item.status = QStringLiteral("valid");
            break;
        case gf::repositories::bo::RepositoryPathStatus::Missing:
            item.status = QStringLiteral("missing");
            break;
        case gf::repositories::bo::RepositoryPathStatus::NotAGitRepository:
            item.status = QStringLiteral("not_a_git_repo");
            break;
        }

        item.lastAccessedAt = formatTimestampMs(repo.lastAccessedAtUnixEpochMs);
        m_items.push_back(std::move(item));
    }
    endResetModel();
}

const std::vector<RepositoryItem>& RepositoryListModel::items() const noexcept {
    return m_items;
}

std::optional<RepositoryItem> RepositoryListModel::findById(const QString& id) const {
    for (const auto& item : m_items) {
        if (item.id == id) {
            return item;
        }
    }
    return std::nullopt;
}

void RepositoryListModel::updateStatus(const QString& id, const QString& status, bool isMissing) {
    for (size_t i = 0; i < m_items.size(); ++i) {
        if (m_items[i].id == id) {
            m_items[i].status = status;
            m_items[i].isMissing = isMissing;
            const auto modelIdx = index(static_cast<int>(i));
            emit dataChanged(modelIdx, modelIdx, {StatusRole, IsMissingRole});
            break;
        }
    }
}

} // namespace gf::desktop
