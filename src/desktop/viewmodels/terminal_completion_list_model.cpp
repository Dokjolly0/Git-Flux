#include "desktop/viewmodels/terminal_completion_list_model.hpp"

namespace gf::desktop
{
    TerminalCompletionListModel::TerminalCompletionListModel(QObject* parent)
        : QAbstractListModel(parent)
    {
    }

    int TerminalCompletionListModel::rowCount(const QModelIndex& parent) const
    {
        if (parent.isValid()) return 0;
        return static_cast<int>(m_items.size());
    }

    QVariant TerminalCompletionListModel::data(const QModelIndex& index, const int role) const
    {
        if (!index.isValid() || index.row() < 0 || index.row() >= static_cast<int>(m_items.size()))
        {
            return {};
        }

        const auto& item = m_items[static_cast<std::size_t>(index.row())];
        switch (role)
        {
        case DisplayTextRole:
            return QString::fromStdString(item.displayText);
        case DetailRole:
            return QString::fromStdString(item.detail);
        case CategoryLabelRole:
            return QString::fromStdString(item.categoryLabel);
        case KindRole:
        {
            const auto sv = gf::terminal::bo::completionKindToString(item.kind);
            return QString::fromUtf8(sv.data(), static_cast<qsizetype>(sv.size()));
        }
        case MatchStartRole:
            return static_cast<int>(item.matchStart);
        case MatchLengthRole:
            return static_cast<int>(item.matchLength);
        default:
            return {};
        }
    }

    QHash<int, QByteArray> TerminalCompletionListModel::roleNames() const
    {
        return {
            {DisplayTextRole, "displayText"},
            {DetailRole, "detail"},
            {CategoryLabelRole, "categoryLabel"},
            {KindRole, "kind"},
            {MatchStartRole, "matchStart"},
            {MatchLengthRole, "matchLength"}
        };
    }

    void TerminalCompletionListModel::setItems(std::vector<gf::terminal::bo::CompletionItem> items)
    {
        beginResetModel();
        m_items = std::move(items);
        endResetModel();
        emit countChanged();
    }

    void TerminalCompletionListModel::clear()
    {
        if (m_items.empty()) return;
        beginResetModel();
        m_items.clear();
        endResetModel();
        emit countChanged();
    }

    const gf::terminal::bo::CompletionItem* TerminalCompletionListModel::itemAt(const int index) const noexcept
    {
        if (index < 0 || index >= static_cast<int>(m_items.size()))
        {
            return nullptr;
        }
        return &m_items[static_cast<std::size_t>(index)];
    }

    bool TerminalCompletionListModel::isExecuteAction(const int index) const noexcept
    {
        const auto* item = itemAt(index);
        return item != nullptr && item->kind == gf::terminal::bo::CompletionItemKind::ExecuteAction;
    }
} // namespace gf::desktop
