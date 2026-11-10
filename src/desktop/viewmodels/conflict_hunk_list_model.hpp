#pragma once

#include <QAbstractListModel>
#include <vector>

#ifndef Q_MOC_RUN
#include "core/modules/git/bo/conflict_models.hpp"
#else
namespace gf::git::bo
{
    struct ConflictHunk;
    enum class ConflictSide;
}
#endif

namespace gf::desktop
{
    class ConflictHunkListModel : public QAbstractListModel
    {
        Q_OBJECT
        Q_PROPERTY(int count READ rowCount NOTIFY countChanged)

    public:
        enum Roles
        {
            HunkIndexRole = Qt::UserRole + 1,
            StartLineRole,
            BaseContentRole,
            OursContentRole,
            TheirsContentRole,
            ChosenSideRole,
            CustomContentRole,
            IsResolvedRole,
            ResolvedPreviewRole
        };

        Q_ENUM(Roles)

        explicit ConflictHunkListModel(QObject* parent = nullptr);
        ~ConflictHunkListModel() override = default;

        [[nodiscard]] int rowCount(const QModelIndex& parent = QModelIndex()) const override;
        [[nodiscard]] QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
        [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

        void setHunks(std::vector<gf::git::bo::ConflictHunk> hunks);
        void clear();

        void resolveHunk(int hunkIndex, gf::git::bo::ConflictSide side, const QString& customText = {});

        [[nodiscard]] const std::vector<gf::git::bo::ConflictHunk>& hunks() const noexcept { return m_hunks; }
        [[nodiscard]] int resolvedCount() const noexcept;
        [[nodiscard]] bool allResolved() const noexcept;

        signals :

        void countChanged();
        void resolutionChanged();

    private:
        std::vector<gf::git::bo::ConflictHunk> m_hunks;
    };
} // namespace gf::desktop
