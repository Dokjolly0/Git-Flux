#include "desktop/viewmodels/conflict_resolver_viewmodel.hpp"

#include "core/modules/git/bl/git_conflict_service.hpp"
#include "core/modules/git/bl/conflict_marker_parser.hpp"

namespace gf::desktop {

ConflictResolverViewModel::ConflictResolverViewModel(gf::git::bl::GitConflictService& conflictService,
                                                     QObject* parent)
    : QObject(parent)
    , m_conflictService(conflictService)
    , m_hunksModel(this) {
    connect(&m_hunksModel, &ConflictHunkListModel::resolutionChanged, this, [this]() {
        emit resolvedHunksChanged();
        emit allResolvedChanged();
    });
    connect(&m_hunksModel, &ConflictHunkListModel::countChanged, this, [this]() {
        emit totalHunksChanged();
        emit resolvedHunksChanged();
        emit allResolvedChanged();
    });
}

void ConflictResolverViewModel::setRepoPath(const QString& path) {
    if (m_repoPath == path) {
        return;
    }
    m_repoPath = path;
    emit repoPathChanged();
    clear();
}

void ConflictResolverViewModel::setBusy(bool busy) {
    if (m_isBusy == busy) {
        return;
    }
    m_isBusy = busy;
    emit isBusyChanged();
}

void ConflictResolverViewModel::setError(const QString& message) {
    m_errorMessage = message;
    emit errorMessageChanged();
    emit hasErrorChanged();
}

void ConflictResolverViewModel::clearError() {
    if (!m_errorMessage.isEmpty()) {
        m_errorMessage.clear();
        emit errorMessageChanged();
        emit hasErrorChanged();
    }
}

void ConflictResolverViewModel::clear() {
    m_currentFilePath.clear();
    m_rawOriginalContent.clear();
    m_isBinary = false;
    m_hunksModel.clear();
    clearError();
    emit currentFilePathChanged();
    emit isBinaryChanged();
}

bool ConflictResolverViewModel::loadFile(const QString& relativeFilePath) {
    if (m_repoPath.isEmpty()) {
        setError("Nessun repository selezionato");
        return false;
    }

    if (relativeFilePath.isEmpty()) {
        setError("Percorso del file non specificato");
        return false;
    }

    setBusy(true);
    clearError();

    const std::filesystem::path path(m_repoPath.toStdString());
    auto result = m_conflictService.getConflictFile(path, relativeFilePath.toStdString());

    if (!result) {
        setError(QString::fromStdString(result.error().message));
        setBusy(false);
        return false;
    }

    m_currentFilePath = relativeFilePath;
    m_rawOriginalContent = result->fullContentWithMarkers;
    m_isBinary = result->isBinary;
    m_hunksModel.setHunks(std::move(result->hunks));

    setBusy(false);
    emit currentFilePathChanged();
    emit isBinaryChanged();
    return true;
}

void ConflictResolverViewModel::resolveHunk(int hunkIndex, int side, const QString& customText) {
    if (side < 0 || side > 3) {
        return;
    }
    const auto chosenSide = static_cast<gf::git::bo::ConflictSide>(side);
    m_hunksModel.resolveHunk(hunkIndex, chosenSide, customText);
}

bool ConflictResolverViewModel::applyResolution(bool autoStage) {
    if (m_repoPath.isEmpty() || m_currentFilePath.isEmpty()) {
        setError("Nessun file in conflitto caricato");
        return false;
    }

    if (!m_hunksModel.allResolved()) {
        setError("Non tutti i conflitti sono stati risolti");
        return false;
    }

    setBusy(true);
    clearError();

    auto reconstructed = gf::git::bl::ConflictMarkerParser::reconstruct(m_rawOriginalContent, m_hunksModel.hunks());
    if (!reconstructed) {
        setError(QString::fromStdString(reconstructed.error()));
        setBusy(false);
        return false;
    }

    const std::filesystem::path path(m_repoPath.toStdString());
    gf::git::bo::ApplyConflictResolutionOptions opts{
        .filePath = m_currentFilePath.toStdString(),
        .resolvedContent = *reconstructed,
        .autoStage = autoStage
    };

    auto applyRes = m_conflictService.applyConflictResolution(path, opts);
    if (!applyRes) {
        setError(QString::fromStdString(applyRes.error().message));
        setBusy(false);
        return false;
    }

    const QString resolvedPath = m_currentFilePath;
    clear();
    setBusy(false);
    emit resolutionApplied(resolvedPath);
    return true;
}

} // namespace gf::desktop
