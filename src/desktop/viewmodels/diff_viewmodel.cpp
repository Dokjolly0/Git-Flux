#include "desktop/viewmodels/diff_viewmodel.hpp"

#include <filesystem>

namespace gf::desktop {

DiffViewModel::DiffViewModel(gf::git::bl::GitDiffService& diffService, QObject* parent)
    : QObject(parent)
    , m_diffService(diffService)
    , m_linesModel(this) {}

bool DiffViewModel::loadDiff(const QString& repoPath, const QString& filePath, const QString& scope) {
    if (repoPath.isEmpty() || filePath.isEmpty()) {
        clear();
        return false;
    }

    gf::git::bo::DiffScope diffScope = gf::git::bo::DiffScope::Unstaged;
    if (scope.compare(QStringLiteral("staged"), Qt::CaseInsensitive) == 0) {
        diffScope = gf::git::bo::DiffScope::Staged;
    } else if (scope.compare(QStringLiteral("untracked"), Qt::CaseInsensitive) == 0) {
        diffScope = gf::git::bo::DiffScope::Untracked;
    }

    std::filesystem::path rPath(repoPath.toStdString());
    std::string fPath = filePath.toStdString();

    auto result = m_diffService.getFileDiff(rPath, fPath, diffScope);
    if (!result) {
        clear();
        emit loadFailed(QString::fromStdString(result.error().message));
        return false;
    }

    const auto& diff = *result;
    m_filePath = filePath;
    m_oldPath = QString::fromStdString(diff.oldPath);
    m_additionsCount = diff.additionsCount;
    m_deletionsCount = diff.deletionsCount;
    m_isBinary = diff.isBinary;
    m_isNewFile = diff.isNewFile;
    m_isDeletedFile = diff.isDeletedFile;
    m_isRenamed = diff.isRenamed;
    m_isEmpty = diff.empty();

    m_linesModel.setDiff(diff);
    emit diffLoaded();
    return true;
}

void DiffViewModel::clear() {
    m_filePath.clear();
    m_oldPath.clear();
    m_additionsCount = 0;
    m_deletionsCount = 0;
    m_isBinary = false;
    m_isNewFile = false;
    m_isDeletedFile = false;
    m_isRenamed = false;
    m_isEmpty = true;

    m_linesModel.clear();
    emit diffLoaded();
}

} // namespace gf::desktop
