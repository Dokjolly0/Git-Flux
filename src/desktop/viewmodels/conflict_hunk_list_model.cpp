#include "desktop/viewmodels/conflict_hunk_list_model.hpp"

namespace gf::desktop
{
    ConflictHunkListModel::ConflictHunkListModel(QObject* parent)
        : QAbstractListModel(parent)
    {
    }

    int ConflictHunkListModel::rowCount(const QModelIndex& parent) const
    {
        if (parent.isValid())
        {
            return 0;
        }
        return static_cast<int>(m_hunks.size());
    }

    QVariant ConflictHunkListModel::data(const QModelIndex& index, int role) const
    {
        if (!index.isValid() || index.row() < 0 || static_cast<std::size_t>(index.row()) >= m_hunks.size())
        {
            return {};
        }

        const auto& hunk = m_hunks[static_cast<std::size_t>(index.row())];

        switch (role)
        {
        case HunkIndexRole:
            return static_cast<qlonglong>(hunk.hunkIndex);
        case StartLineRole:
            return static_cast<qlonglong>(hunk.startLine);
        case BaseContentRole:
            return QString::fromStdString(hunk.baseContent);
        case OursContentRole:
            return QString::fromStdString(hunk.oursContent);
        case TheirsContentRole:
            return QString::fromStdString(hunk.theirsContent);
        case ChosenSideRole:
            if (!hunk.chosenSide.has_value())
            {
                return -1;
            }
            return static_cast<int>(*hunk.chosenSide);
        case CustomContentRole:
            return hunk.customContent.has_value() ? QString::fromStdString(*hunk.customContent) : QString{};
        case IsResolvedRole:
            return hunk.isResolved;
        case ResolvedPreviewRole:
            return QString::fromStdString(hunk.resolvedContent());
        default:
            return {};
        }
    }

    QHash<int, QByteArray> ConflictHunkListModel::roleNames() const
    {
        return {
            {HunkIndexRole, "hunkIndex"},
            {StartLineRole, "startLine"},
            {BaseContentRole, "baseContent"},
            {OursContentRole, "oursContent"},
            {TheirsContentRole, "theirsContent"},
            {ChosenSideRole, "chosenSide"},
            {CustomContentRole, "customContent"},
            {IsResolvedRole, "isResolved"},
            {ResolvedPreviewRole, "resolvedPreview"}
        };
    }

    void ConflictHunkListModel::setHunks(std::vector<gf::git::bo::ConflictHunk> hunks)
    {
        beginResetModel();
        m_hunks = std::move(hunks);
        endResetModel();
        emit countChanged();
        emit resolutionChanged();
    }

    void ConflictHunkListModel::clear()
    {
        beginResetModel();
        m_hunks.clear();
        endResetModel();
        emit countChanged();
        emit resolutionChanged();
    }

    void ConflictHunkListModel::resolveHunk(int hunkIndex, gf::git::bo::ConflictSide side, const QString& customText)
    {
        if (hunkIndex < 0 || static_cast<std::size_t>(hunkIndex) >= m_hunks.size())
        {
            return;
        }

        auto& hunk = m_hunks[static_cast<std::size_t>(hunkIndex)];
        hunk.chosenSide = side;
        hunk.isResolved = true;
        if (side == gf::git::bo::ConflictSide::Custom)
        {
            hunk.customContent = customText.toStdString();
        }
        else
        {
            hunk.customContent = std::nullopt;
        }

        const QModelIndex idx = index(hunkIndex, 0);
        emit dataChanged(idx, idx, {ChosenSideRole, CustomContentRole, IsResolvedRole, ResolvedPreviewRole});
        emit resolutionChanged();
    }

    int ConflictHunkListModel::resolvedCount() const noexcept
    {
        int count = 0;
        for (const auto& h : m_hunks)
        {
            if (h.isResolved)
            {
                ++count;
            }
        }
        return count;
    }

    bool ConflictHunkListModel::allResolved() const noexcept
    {
        if (m_hunks.empty())
        {
            return true;
        }
        for (const auto& h : m_hunks)
        {
            if (!h.isResolved)
            {
                return false;
            }
        }
        return true;
    }
} // namespace gf::desktop
