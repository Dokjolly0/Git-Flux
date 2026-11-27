#pragma once

#include <QAbstractListModel>
#include <vector>

#ifndef Q_MOC_RUN
#include "core/modules/terminal/bo/git_completion_models.hpp"
#else
namespace gf::terminal::bo
{
    struct CompletionItem;
}
#endif

namespace gf::desktop
{
    class TerminalCompletionListModel final : public QAbstractListModel
    {
        Q_OBJECT
        Q_PROPERTY(int count READ count NOTIFY countChanged)

    public:
        enum Roles
        {
            InsertTextRole = Qt::UserRole + 1,
            DisplayTextRole,
            DetailRole,
            CategoryLabelRole,
            KindRole,
            MatchStartRole,
            MatchLengthRole,
            AppendSpaceRole
        };

        explicit TerminalCompletionListModel(QObject* parent = nullptr);
        ~TerminalCompletionListModel() override = default;

        [[nodiscard]] int rowCount(const QModelIndex& parent = QModelIndex()) const override;
        [[nodiscard]] QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
        [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

        [[nodiscard]] int count() const noexcept { return static_cast<int>(m_items.size()); }

        void setItems(std::vector<gf::terminal::bo::CompletionItem> items);
        void clear();

        [[nodiscard]] const gf::terminal::bo::CompletionItem* itemAt(int index) const noexcept;

    signals:
        void countChanged();

    private:
        std::vector<gf::terminal::bo::CompletionItem> m_items;
    };
} // namespace gf::desktop
