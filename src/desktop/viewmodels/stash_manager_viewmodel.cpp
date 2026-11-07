#include "desktop/viewmodels/stash_manager_viewmodel.hpp"

namespace gf::desktop {

StashManagerViewModel::StashManagerViewModel(gf::git::bl::GitStashService& stashService,
                                             QObject* parent)
    : QObject(parent)
    , m_stashService(stashService)
    , m_stashesModel(this) {}

void StashManagerViewModel::setRepoPath(const QString& path) {
    if (m_repoPath == path) {
        return;
    }
    m_repoPath = path;
    emit repoPathChanged();
    refresh();
}

void StashManagerViewModel::setBusy(bool busy) {
    if (m_isBusy == busy) {
        return;
    }
    m_isBusy = busy;
    emit isBusyChanged();
}

void StashManagerViewModel::setError(const QString& message) {
    m_errorMessage = message;
    emit errorMessageChanged();
    emit hasErrorChanged();
}

void StashManagerViewModel::clearError() {
    if (!m_errorMessage.isEmpty()) {
        m_errorMessage.clear();
        emit errorMessageChanged();
        emit hasErrorChanged();
    }
}

void StashManagerViewModel::refresh() {
    if (m_repoPath.isEmpty()) {
        m_stashesModel.clear();
        return;
    }

    setBusy(true);
    clearError();

    const std::filesystem::path path(m_repoPath.toStdString());
    auto result = m_stashService.listStashes(path);

    if (result) {
        m_stashesModel.setStashes(std::move(*result));
    } else {
        setError(QString::fromStdString(result.error().message));
    }

    setBusy(false);
}

bool StashManagerViewModel::push(const QString& message, bool includeUntracked, bool keepIndex) {
    if (m_repoPath.isEmpty()) {
        setError("Nessun repository selezionato");
        return false;
    }

    setBusy(true);
    clearError();

    const std::filesystem::path path(m_repoPath.toStdString());
    gf::git::bo::StashPushOptions opts;
    if (!message.trimmed().isEmpty()) {
        opts.message = message.trimmed().toStdString();
    }
    opts.includeUntracked = includeUntracked;
    opts.keepIndex = keepIndex;

    auto result = m_stashService.stashPush(path, opts);
    if (!result) {
        setError(QString::fromStdString(result.error().message));
        setBusy(false);
        return false;
    }

    setBusy(false);
    refresh();
    emit operationSucceeded("push");
    return true;
}

bool StashManagerViewModel::pop(int index, bool restoreIndex) {
    if (m_repoPath.isEmpty()) {
        setError("Nessun repository selezionato");
        return false;
    }

    setBusy(true);
    clearError();

    const std::filesystem::path path(m_repoPath.toStdString());
    gf::git::bo::StashApplyOptions opts{
        .stashIndex = static_cast<std::size_t>(index >= 0 ? index : 0),
        .restoreIndex = restoreIndex
    };

    auto result = m_stashService.stashPop(path, opts);
    if (!result) {
        setError(QString::fromStdString(result.error().message));
        setBusy(false);
        return false;
    }

    setBusy(false);
    refresh();
    emit operationSucceeded("pop");
    return true;
}

bool StashManagerViewModel::apply(int index, bool restoreIndex) {
    if (m_repoPath.isEmpty()) {
        setError("Nessun repository selezionato");
        return false;
    }

    setBusy(true);
    clearError();

    const std::filesystem::path path(m_repoPath.toStdString());
    gf::git::bo::StashApplyOptions opts{
        .stashIndex = static_cast<std::size_t>(index >= 0 ? index : 0),
        .restoreIndex = restoreIndex
    };

    auto result = m_stashService.stashApply(path, opts);
    if (!result) {
        setError(QString::fromStdString(result.error().message));
        setBusy(false);
        return false;
    }

    setBusy(false);
    refresh();
    emit operationSucceeded("apply");
    return true;
}

bool StashManagerViewModel::drop(int index) {
    if (m_repoPath.isEmpty()) {
        setError("Nessun repository selezionato");
        return false;
    }

    setBusy(true);
    clearError();

    const std::filesystem::path path(m_repoPath.toStdString());
    auto result = m_stashService.stashDrop(path, static_cast<std::size_t>(index >= 0 ? index : 0));
    if (!result) {
        setError(QString::fromStdString(result.error().message));
        setBusy(false);
        return false;
    }

    setBusy(false);
    refresh();
    emit operationSucceeded("drop");
    return true;
}

bool StashManagerViewModel::clear() {
    if (m_repoPath.isEmpty()) {
        setError("Nessun repository selezionato");
        return false;
    }

    setBusy(true);
    clearError();

    const std::filesystem::path path(m_repoPath.toStdString());
    auto result = m_stashService.stashClear(path);
    if (!result) {
        setError(QString::fromStdString(result.error().message));
        setBusy(false);
        return false;
    }

    setBusy(false);
    refresh();
    emit operationSucceeded("clear");
    return true;
}

} // namespace gf::desktop
