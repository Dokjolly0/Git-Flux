#include "desktop/viewmodels/branch_manager_viewmodel.hpp"

#include "core/modules/git/bl/git_history_service.hpp"
#include "desktop/services/serial_git_executor.hpp"

#include <QSet>
#include <QVariant>

#include <optional>
#include <vector>

namespace gf::desktop
{
    namespace
    {
        void populateFileStatusFields(QVariantMap& map, const git::bo::FileChangeStatus status)
        {
            switch (status)
            {
            case git::bo::FileChangeStatus::Added:
                map[QStringLiteral("status")] = QStringLiteral("A");
                map[QStringLiteral("statusLabel")] = QStringLiteral("Added");
                break;
            case git::bo::FileChangeStatus::Deleted:
                map[QStringLiteral("status")] = QStringLiteral("D");
                map[QStringLiteral("statusLabel")] = QStringLiteral("Deleted");
                break;
            case git::bo::FileChangeStatus::Renamed:
                map[QStringLiteral("status")] = QStringLiteral("R");
                map[QStringLiteral("statusLabel")] = QStringLiteral("Renamed");
                break;
            case git::bo::FileChangeStatus::Copied:
                map[QStringLiteral("status")] = QStringLiteral("C");
                map[QStringLiteral("statusLabel")] = QStringLiteral("Copied");
                break;
            default:
                map[QStringLiteral("status")] = QStringLiteral("M");
                map[QStringLiteral("statusLabel")] = QStringLiteral("Modified");
                break;
            }
        }

        // Worker-safe: QVariant containers are implicitly shared and safe to build off the GUI thread.
        template <typename Items>
        QVariantList commitFilesToVariantList(const Items& items)
        {
            QVariantList list;
            list.reserve(static_cast<qsizetype>(items.size()));
            for (const auto& item : items)
            {
                QVariantMap map;
                map[QStringLiteral("path")] = QString::fromStdString(item.path);
                map[QStringLiteral("oldPath")] = QString::fromStdString(item.oldPath);
                map[QStringLiteral("additions")] = item.additions;
                map[QStringLiteral("deletions")] = item.deletions;
                map[QStringLiteral("isBinary")] = item.isBinary;
                populateFileStatusFields(map, item.status);
                list.append(map);
            }
            return list;
        }

        QVariantList buildAggregatedFilesVariant(const std::vector<git::bo::MultiCommitFileChange>& aggregatedFiles)
        {
            QVariantList aggList;
            aggList.reserve(static_cast<qsizetype>(aggregatedFiles.size()));
            for (const auto& item : aggregatedFiles)
            {
                QVariantMap m;
                m[QStringLiteral("path")] = QString::fromStdString(item.path);
                m[QStringLiteral("oldPath")] = QString::fromStdString(item.oldPath);
                m[QStringLiteral("additions")] = item.additions;
                m[QStringLiteral("deletions")] = item.deletions;
                m[QStringLiteral("isBinary")] = item.isBinary;
                populateFileStatusFields(m, item.status);

                QVariantList commitsList;
                commitsList.reserve(static_cast<qsizetype>(item.commits.size()));
                for (const auto& attr : item.commits)
                {
                    QVariantMap cm;
                    cm[QStringLiteral("sha")] = QString::fromStdString(attr.commitSha);
                    cm[QStringLiteral("shortSha")] = QString::fromStdString(attr.shortSha);
                    cm[QStringLiteral("subject")] = QString::fromStdString(attr.subject);
                    cm[QStringLiteral("authorName")] = QString::fromStdString(attr.authorName);
                    cm[QStringLiteral("additions")] = attr.additions;
                    cm[QStringLiteral("deletions")] = attr.deletions;
                    cm[QStringLiteral("isBinary")] = attr.isBinary;
                    populateFileStatusFields(cm, attr.status);
                    commitsList.append(cm);
                }
                m[QStringLiteral("commitCount")] = static_cast<int>(item.commits.size());
                m[QStringLiteral("commits")] = commitsList;
                aggList.append(m);
            }
            return aggList;
        }

        QVariantList buildPerCommitGroupsVariant(const std::vector<git::bo::CommitChangesGroup>& groups)
        {
            QVariantList groupsList;
            groupsList.reserve(static_cast<qsizetype>(groups.size()));
            for (const auto& grp : groups)
            {
                QVariantMap gm;
                gm[QStringLiteral("sha")] = QString::fromStdString(grp.commitSha);
                gm[QStringLiteral("shortSha")] = QString::fromStdString(grp.shortSha);
                gm[QStringLiteral("subject")] = QString::fromStdString(grp.subject);
                gm[QStringLiteral("authorName")] = QString::fromStdString(grp.authorName);
                gm[QStringLiteral("authorDate")] = QString::fromStdString(grp.authorDateIso);
                gm[QStringLiteral("totalAdditions")] = grp.totalAdditions;
                gm[QStringLiteral("totalDeletions")] = grp.totalDeletions;
                gm[QStringLiteral("files")] = commitFilesToVariantList(grp.files);
                groupsList.append(gm);
            }
            return groupsList;
        }

        QVariantMap multiCommitSummaryToVariantMap(const git::bo::MultiCommitChangesSummary& summary)
        {
            QVariantMap out;
            out[QStringLiteral("isContiguousRange")] = summary.isContiguousRange;
            out[QStringLiteral("rangeBaseRevision")] = QString::fromStdString(summary.rangeBaseRevision);
            out[QStringLiteral("rangeHeadSha")] = QString::fromStdString(summary.rangeHeadSha);
            out[QStringLiteral("totalAdditions")] = summary.totalAdditions;
            out[QStringLiteral("totalDeletions")] = summary.totalDeletions;
            out[QStringLiteral("aggregatedFiles")] = buildAggregatedFilesVariant(summary.aggregatedFiles);
            out[QStringLiteral("perCommitGroups")] = buildPerCommitGroupsVariant(summary.perCommitGroups);
            return out;
        }
    } // namespace

    void BranchManagerViewModel::resetMultiSelectionState()
    {
        const bool hadLoading = m_isLoadingMultiCommitFiles;
        m_multiCommitFilesRequest.invalidate();
        m_isLoadingMultiCommitFiles = false;
        m_selectedCommitShas.clear();
        m_effectiveSelectedShas.clear();
        m_selectedShaSet.clear();
        m_effectiveShaSet.clear();
        m_multiSelectAnchorSha.clear();
        m_multiSelectReferenceBranch.clear();
        m_isMultiSelectContiguousRange = false;
        m_multiSelectRangeBase.clear();
        m_multiSelectRangeHead.clear();
        m_multiSelectBranchFilterActive = !m_allowCrossBranchMultiSelect;
        ++m_multiSelectRevision;
        emit multiSelectChanged();
        if (hadLoading)
        {
            emit multiSelectLoadingChanged();
        }
    }

    void BranchManagerViewModel::recomputeMultiSelectionState(const bool triggerReload)
    {
        // Keep only commits still present in the current commit list, ordered newest -> oldest
        const auto rawAnalysis = m_commitHistoryModel.analyzeSelection(m_selectedCommitShas, m_multiSelectAnchorSha);
        m_selectedCommitShas = rawAnalysis.shas;
        m_selectedShaSet = QSet<QString>(m_selectedCommitShas.begin(), m_selectedCommitShas.end());

        if (m_selectedCommitShas.isEmpty())
        {
            m_multiSelectAnchorSha.clear();
            m_effectiveSelectedShas.clear();
            m_effectiveShaSet.clear();
            m_multiSelectReferenceBranch.clear();
            m_isMultiSelectContiguousRange = false;
            m_multiSelectRangeBase.clear();
            m_multiSelectRangeHead.clear();
            ++m_multiSelectRevision;
            emit multiSelectChanged();
            return;
        }

        if (m_multiSelectAnchorSha.isEmpty() || !m_selectedShaSet.contains(m_multiSelectAnchorSha))
        {
            m_multiSelectAnchorSha = m_selectedCommitShas.front();
        }

        const bool applyBranchFilter = (!m_allowCrossBranchMultiSelect || m_multiSelectBranchFilterActive);
        const QStringList effectiveInput = applyBranchFilter
                                               ? m_commitHistoryModel.filterCommitsByFirstParentLine(
                                                     m_selectedCommitShas, m_multiSelectAnchorSha)
                                               : m_selectedCommitShas;

        const auto effAnalysis = m_commitHistoryModel.analyzeSelection(effectiveInput, m_multiSelectAnchorSha);
        m_effectiveSelectedShas = effAnalysis.shas;
        m_effectiveShaSet = QSet<QString>(m_effectiveSelectedShas.begin(), m_effectiveSelectedShas.end());
        m_multiSelectReferenceBranch = effAnalysis.referenceBranch;
        m_isMultiSelectContiguousRange = effAnalysis.isContiguousRange;
        m_multiSelectRangeBase = effAnalysis.baseRevision;
        m_multiSelectRangeHead = effAnalysis.headSha;
        ++m_multiSelectRevision;
        emit multiSelectChanged();

        if (triggerReload)
        {
            if (m_selectedCommitShas.size() > 1)
            {
                requestMultiCommitFiles();
            }
            else if (m_effectiveSelectedShas.size() == 1)
            {
                m_multiCommitFilesRequest.invalidate();
                if (m_isLoadingMultiCommitFiles)
                {
                    m_isLoadingMultiCommitFiles = false;
                    emit multiSelectLoadingChanged();
                }
                requestCommitFiles(m_effectiveSelectedShas.front());
            }
        }
    }

    void BranchManagerViewModel::setAllowCrossBranchMultiSelect(const bool allow)
    {
        if (m_allowCrossBranchMultiSelect == allow) return;
        m_allowCrossBranchMultiSelect = allow;
        m_multiSelectBranchFilterActive = !allow;
        emit multiSelectSettingsChanged();
        recomputeMultiSelectionState(m_selectedCommitShas.size() > 1);
    }

    void BranchManagerViewModel::setMultiSelectBranchFilterActive(const bool active)
    {
        const bool effectiveActive = (!m_allowCrossBranchMultiSelect) ? true : active;
        if (m_multiSelectBranchFilterActive == effectiveActive) return;
        m_multiSelectBranchFilterActive = effectiveActive;
        recomputeMultiSelectionState(m_selectedCommitShas.size() > 1);
    }

    void BranchManagerViewModel::setMultiSelectViewMode(const QString& mode)
    {
        const QString normalized = (mode == QStringLiteral("by_commit"))
                                       ? QStringLiteral("by_commit")
                                       : QStringLiteral("total");
        if (m_multiSelectViewMode == normalized) return;
        m_multiSelectViewMode = normalized;
        emit multiSelectViewModeChanged();
    }

    void BranchManagerViewModel::selectSingleCommit(const int rowIndex)
    {
        m_multiCommitFilesRequest.invalidate();
        if (m_isLoadingMultiCommitFiles)
        {
            m_isLoadingMultiCommitFiles = false;
            emit multiSelectLoadingChanged();
        }
        m_multiSelectBranchFilterActive = !m_allowCrossBranchMultiSelect;

        if (!m_commitHistoryModel.isSelectableCommitRow(rowIndex))
        {
            m_selectedCommitShas.clear();
            m_effectiveSelectedShas.clear();
            m_selectedShaSet.clear();
            m_effectiveShaSet.clear();
            m_multiSelectAnchorSha.clear();
            m_multiSelectReferenceBranch.clear();
            m_isMultiSelectContiguousRange = false;
            m_multiSelectRangeBase.clear();
            m_multiSelectRangeHead.clear();
            ++m_multiSelectRevision;
            emit multiSelectChanged();
            return;
        }

        const QVariantMap commit = m_commitHistoryModel.getCommit(rowIndex);
        const QString sha = commit.value(QStringLiteral("sha")).toString();
        m_multiSelectAnchorSha = sha;
        m_selectedCommitShas = {sha};
        recomputeMultiSelectionState(false);
    }

    void BranchManagerViewModel::setSelectionAnchorSha(const QString& sha)
    {
        if (sha.isEmpty() || sha == QStringLiteral("WIP"))
        {
            selectSingleCommit(-1);
            return;
        }
        const int idx = m_commitHistoryModel.indexOfSha(sha);
        if (idx >= 0)
        {
            selectSingleCommit(idx);
        }
    }

    bool BranchManagerViewModel::selectCommitRangeTo(const int targetIndex)
    {
        if (!m_commitHistoryModel.isSelectableCommitRow(targetIndex))
        {
            return false;
        }

        int anchorIdx = m_multiSelectAnchorSha.isEmpty() ? -1 : m_commitHistoryModel.indexOfSha(m_multiSelectAnchorSha);
        if (anchorIdx < 0 || !m_commitHistoryModel.isSelectableCommitRow(anchorIdx))
        {
            selectSingleCommit(targetIndex);
            return true;
        }

        const bool branchOnly = !m_allowCrossBranchMultiSelect;
        const auto resolved = m_commitHistoryModel.resolveRangeSelection(anchorIdx, targetIndex, branchOnly);
        if (resolved.blockedByBranchScope)
        {
            setStatusMessage(QStringLiteral(
                "Modalità solo branch: i due commit non appartengono alla stessa linea (--first-parent). "
                "Attiva la modalità cross-branch per confrontare branch diversi."));
            return false;
        }

        if (resolved.shas.isEmpty())
        {
            return false;
        }

        m_selectedCommitShas = resolved.shas;
        recomputeMultiSelectionState(true);
        return true;
    }

    bool BranchManagerViewModel::toggleCommitSelectionAt(const int rowIndex)
    {
        if (!m_commitHistoryModel.isSelectableCommitRow(rowIndex))
        {
            return false;
        }

        const QVariantMap commit = m_commitHistoryModel.getCommit(rowIndex);
        const QString sha = commit.value(QStringLiteral("sha")).toString();
        if (sha.isEmpty()) return false;

        if (m_selectedCommitShas.isEmpty())
        {
            selectSingleCommit(rowIndex);
            return true;
        }

        if (m_selectedShaSet.contains(sha))
        {
            if (m_selectedCommitShas.size() <= 1)
            {
                return true; // keep at least one selected commit
            }
            m_selectedCommitShas.removeAll(sha);
            if (m_multiSelectAnchorSha == sha && !m_selectedCommitShas.isEmpty())
            {
                m_multiSelectAnchorSha = m_selectedCommitShas.front();
            }
            recomputeMultiSelectionState(true);
            return true;
        }

        if (!m_allowCrossBranchMultiSelect && !m_multiSelectAnchorSha.isEmpty())
        {
            if (!m_commitHistoryModel.isOnFirstParentLine(m_multiSelectAnchorSha, sha))
            {
                setStatusMessage(QStringLiteral(
                    "Modalità solo branch: puoi aggiungere solo commit sulla stessa linea (--first-parent). "
                    "Attiva la modalità cross-branch per selezionare altri branch."));
                return false;
            }
        }

        m_selectedCommitShas.append(sha);
        recomputeMultiSelectionState(true);
        return true;
    }

    void BranchManagerViewModel::clearMultiSelection()
    {
        if (m_selectedCommitShas.size() <= 1) return;
        const QString anchor = !m_multiSelectAnchorSha.isEmpty()
                                   ? m_multiSelectAnchorSha
                                   : m_selectedCommitShas.front();
        m_multiCommitFilesRequest.invalidate();
        if (m_isLoadingMultiCommitFiles)
        {
            m_isLoadingMultiCommitFiles = false;
            emit multiSelectLoadingChanged();
        }
        m_multiSelectBranchFilterActive = !m_allowCrossBranchMultiSelect;
        m_multiSelectAnchorSha = anchor;
        m_selectedCommitShas = {anchor};
        recomputeMultiSelectionState(true);
    }

    bool BranchManagerViewModel::isShaSelected(const QString& sha) const
    {
        return m_selectedShaSet.contains(sha);
    }

    bool BranchManagerViewModel::isShaEffectivelySelected(const QString& sha) const
    {
        return m_effectiveShaSet.contains(sha);
    }

    void BranchManagerViewModel::requestMultiCommitFiles()
    {
        if (m_repoPath.empty() || m_effectiveSelectedShas.isEmpty())
        {
            m_multiCommitFilesRequest.invalidate();
            if (m_isLoadingMultiCommitFiles)
            {
                m_isLoadingMultiCommitFiles = false;
                emit multiSelectLoadingChanged();
            }
            return;
        }

        std::vector<std::string> shasStd;
        shasStd.reserve(static_cast<std::size_t>(m_effectiveSelectedShas.size()));
        for (const QString& s : m_effectiveSelectedShas)
        {
            shasStd.push_back(s.toStdString());
        }

        const bool isContiguous = m_isMultiSelectContiguousRange;
        const std::string baseRev = m_multiSelectRangeBase.toStdString();
        const std::string headSha = m_multiSelectRangeHead.toStdString();

        const auto id = m_multiCommitFilesRequest.next();
        if (!m_isLoadingMultiCommitFiles)
        {
            m_isLoadingMultiCommitFiles = true;
            emit multiSelectLoadingChanged();
        }

        auto& service = m_historyService;
        m_readExecutor->run(this,
                            [&service, isCurrent = m_multiCommitFilesRequest.guard(id), repoPath = m_repoPath,
                                shasStd = std::move(shasStd), isContiguous, baseRev, headSha]() -> std::optional<QVariantMap>
                            {
                                if (!isCurrent())
                                {
                                    return std::nullopt;
                                }
                                auto res = service.getMultiCommitChanges(repoPath, shasStd, isContiguous, baseRev, headSha);
                                if (!res)
                                {
                                    return QVariantMap{};
                                }
                                return multiCommitSummaryToVariantMap(*res);
                            },
                            [this, id](std::optional<QVariantMap> summary)
                            {
                                if (!m_multiCommitFilesRequest.isCurrent(id))
                                {
                                    return;
                                }
                                if (m_isLoadingMultiCommitFiles)
                                {
                                    m_isLoadingMultiCommitFiles = false;
                                    emit multiSelectLoadingChanged();
                                }
                                if (summary)
                                {
                                    emit multiCommitFilesLoaded(*summary);
                                }
                            });
    }

    void BranchManagerViewModel::requestCommitFiles(const QString& commitSha)
    {
        if (m_repoPath.empty() || commitSha.isEmpty())
        {
            m_commitFilesRequest.invalidate();
            return;
        }

        const auto id = m_commitFilesRequest.next();
        auto& service = m_historyService;
        m_readExecutor->run(this,
                            [&service, isCurrent = m_commitFilesRequest.guard(id), repoPath = m_repoPath,
                                sha = commitSha.toStdString()]() -> std::optional<QVariantList>
                            {
                                if (!isCurrent())
                                {
                                    return std::nullopt; // another commit was selected meanwhile
                                }
                                auto res = service.getCommitFiles(repoPath, sha);
                                return res ? commitFilesToVariantList(*res) : QVariantList{};
                            },
                            [this, id, commitSha](std::optional<QVariantList> files)
                            {
                                if (m_commitFilesRequest.isCurrent(id) && files)
                                {
                                    emit commitFilesLoaded(commitSha, *files);
                                }
                            });
    }
} // namespace gf::desktop
