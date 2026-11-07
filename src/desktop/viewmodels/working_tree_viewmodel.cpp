#include "desktop/viewmodels/working_tree_viewmodel.hpp"

#include <filesystem>

namespace gf::desktop {

WorkingTreeViewModel::WorkingTreeViewModel(gf::git::bl::GitWorkingTreeService& statusService,
                                           gf::git::bl::GitStagingService& stagingService,
                                           QObject* parent)
    : QObject(parent)
    , m_statusService(statusService)
    , m_stagingService(stagingService)
    , m_stagedModel(this)
    , m_unstagedModel(this)
    , m_untrackedModel(this)
    , m_conflictedModel(this) {}

bool WorkingTreeViewModel::refresh(const QString& repoPath) {
    if (repoPath.trimmed().isEmpty()) {
        m_stagedModel.clear();
        m_unstagedModel.clear();
        m_untrackedModel.clear();
        m_conflictedModel.clear();
        m_stagedCount = 0;
        m_unstagedCount = 0;
        m_untrackedCount = 0;
        m_conflictedCount = 0;
        m_isClean = true;
        m_branchName.clear();
        m_aheadCount = 0;
        m_behindCount = 0;
        m_isDetached = false;
        m_isUnborn = false;
        emit statusChanged();
        return false;
    }

    std::filesystem::path rPath(repoPath.toStdString());
    auto result = m_statusService.getWorkingTreeStatus(rPath);
    if (!result) {
        emit operationFailed(QString::fromStdString(result.error().message));
        return false;
    }

    const auto& status = *result;
    m_stagedModel.setItems(status.stagedFiles);
    m_unstagedModel.setItems(status.unstagedFiles);
    m_untrackedModel.setItems(status.untrackedFiles);
    m_conflictedModel.setItems(status.conflictedFiles);

    m_stagedCount = status.stagedCount();
    m_unstagedCount = status.unstagedCount();
    m_untrackedCount = status.untrackedCount();
    m_conflictedCount = status.conflictedCount();
    m_isClean = status.isClean();

    m_branchName = QString::fromStdString(status.branch.headName);
    m_aheadCount = status.branch.ahead;
    m_behindCount = status.branch.behind;
    m_isDetached = status.branch.detached;
    m_isUnborn = status.branch.unborn;

    emit statusChanged();
    return true;
}

bool WorkingTreeViewModel::stageFile(const QString& repoPath, const QString& filePath) {
    std::filesystem::path rPath(repoPath.toStdString());
    auto res = m_stagingService.stageFiles(rPath, {filePath.toStdString()});
    if (!res) {
        emit operationFailed(QString::fromStdString(res.error().message));
        return false;
    }
    return refresh(repoPath);
}

bool WorkingTreeViewModel::stageAll(const QString& repoPath) {
    std::filesystem::path rPath(repoPath.toStdString());
    auto res = m_stagingService.stageAll(rPath);
    if (!res) {
        emit operationFailed(QString::fromStdString(res.error().message));
        return false;
    }
    return refresh(repoPath);
}

bool WorkingTreeViewModel::unstageFile(const QString& repoPath, const QString& filePath) {
    std::filesystem::path rPath(repoPath.toStdString());
    auto res = m_stagingService.unstageFiles(rPath, {filePath.toStdString()});
    if (!res) {
        emit operationFailed(QString::fromStdString(res.error().message));
        return false;
    }
    return refresh(repoPath);
}

bool WorkingTreeViewModel::unstageAll(const QString& repoPath) {
    std::filesystem::path rPath(repoPath.toStdString());
    auto res = m_stagingService.unstageAll(rPath);
    if (!res) {
        emit operationFailed(QString::fromStdString(res.error().message));
        return false;
    }
    return refresh(repoPath);
}

bool WorkingTreeViewModel::discardFile(const QString& repoPath, const QString& filePath) {
    std::filesystem::path rPath(repoPath.toStdString());
    auto res = m_stagingService.discardFiles(rPath, {filePath.toStdString()});
    if (!res) {
        emit operationFailed(QString::fromStdString(res.error().message));
        return false;
    }
    return refresh(repoPath);
}

QVariantList WorkingTreeViewModel::getStagedFiles() const {
    QVariantList list;
    const auto& items = m_stagedModel.items();
    list.reserve(static_cast<qsizetype>(items.size()));
    for (const auto& item : items) {
        QVariantMap map;
        map[QStringLiteral("path")] = QString::fromStdString(item.path);
        map[QStringLiteral("oldPath")] = QString::fromStdString(item.originalPath);
        QString statusChar;
        switch (item.kind) {
        case git::bo::FileChangeKind::Added:
        case git::bo::FileChangeKind::Untracked:
            statusChar = QStringLiteral("A");
            break;
        case git::bo::FileChangeKind::Deleted:
            statusChar = QStringLiteral("D");
            break;
        case git::bo::FileChangeKind::Renamed:
        case git::bo::FileChangeKind::Copied:
            statusChar = QStringLiteral("R");
            break;
        default:
            statusChar = QStringLiteral("M");
            break;
        }
        map[QStringLiteral("status")] = statusChar;
        map[QStringLiteral("kind")] = QString::fromLatin1(git::bo::fileChangeKindToString(item.kind).data());
        map[QStringLiteral("isStaged")] = true;
        map[QStringLiteral("additions")] = 0;
        map[QStringLiteral("deletions")] = 0;
        list.append(map);
    }
    return list;
}

QVariantList WorkingTreeViewModel::getUnstagedFiles() const {
    QVariantList list;
    auto appendItems = [&list](const std::vector<git::bo::WorkingTreeFileItem>& items, bool isUntracked) {
        for (const auto& item : items) {
            QVariantMap map;
            map[QStringLiteral("path")] = QString::fromStdString(item.path);
            map[QStringLiteral("oldPath")] = QString::fromStdString(item.originalPath);
            QString statusChar;
            if (isUntracked || item.kind == git::bo::FileChangeKind::Added || item.kind == git::bo::FileChangeKind::Untracked) {
                statusChar = QStringLiteral("A");
            } else if (item.kind == git::bo::FileChangeKind::Deleted) {
                statusChar = QStringLiteral("D");
            } else if (item.kind == git::bo::FileChangeKind::Renamed || item.kind == git::bo::FileChangeKind::Copied) {
                statusChar = QStringLiteral("R");
            } else {
                statusChar = QStringLiteral("M");
            }
            map[QStringLiteral("status")] = statusChar;
            map[QStringLiteral("kind")] = QString::fromLatin1(git::bo::fileChangeKindToString(item.kind).data());
            map[QStringLiteral("isStaged")] = false;
            map[QStringLiteral("additions")] = 0;
            map[QStringLiteral("deletions")] = 0;
            list.append(map);
        }
    };
    appendItems(m_unstagedModel.items(), false);
    appendItems(m_untrackedModel.items(), true);
    appendItems(m_conflictedModel.items(), false);
    return list;
}

QVariantList WorkingTreeViewModel::getAllWorkingFiles() const {
    QVariantList list = getStagedFiles();
    list.append(getUnstagedFiles());
    return list;
}

} // namespace gf::desktop
