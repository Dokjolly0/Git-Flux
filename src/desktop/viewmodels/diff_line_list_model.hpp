#pragma once

#include <QAbstractListModel>
#include <QString>
#include <vector>

#ifndef Q_MOC_RUN
#include "core/modules/git/bo/diff_models.hpp"
#else
namespace gf::git::bo
{
    struct DiffLine;
}
#endif

namespace gf::desktop
{
    class DiffLineListModel final : public QAbstractListModel
    {
        Q_OBJECT

    public:
        enum Roles
        {
            LineKindRole = Qt::UserRole + 1,
            OldLineNumberRole,
            NewLineNumberRole,
            ContentRole,
            RichContentRole,
            BgColorRole,
            FgColorRole
        };

        Q_ENUM(Roles)

        explicit DiffLineListModel(QObject* parent = nullptr);
        ~DiffLineListModel() override = default;

        [[nodiscard]] int rowCount(const QModelIndex& parent = QModelIndex()) const override;
        [[nodiscard]] QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
        [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

        /// Rows prepared off the GUI thread (headers included, rich text already highlighted).
        void setRows(std::vector<gf::git::bo::DiffLine> lines, std::vector<QString> richLines);
        void clear();

        [[nodiscard]] const std::vector<gf::git::bo::DiffLine>& flatLines() const noexcept
        {
            return m_lines;
        }

    private:
        std::vector<gf::git::bo::DiffLine> m_lines;
        std::vector<QString> m_richLines;
    };
} // namespace gf::desktop
