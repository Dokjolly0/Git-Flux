#include "desktop/viewmodels/terminal_output_list_model.hpp"

#include <QStringList>

namespace gf::desktop
{
    TerminalOutputListModel::TerminalOutputListModel(QObject* parent)
        : QAbstractListModel(parent)
    {
    }

    int TerminalOutputListModel::rowCount(const QModelIndex& parent) const
    {
        if (parent.isValid()) return 0;
        return static_cast<int>(m_entries.size());
    }

    QVariant TerminalOutputListModel::data(const QModelIndex& index, const int role) const
    {
        if (!index.isValid() || index.row() < 0 || index.row() >= static_cast<int>(m_entries.size()))
        {
            return {};
        }

        const auto& e = m_entries[static_cast<std::size_t>(index.row())].data;
        switch (role)
        {
        case LineKindRole: return e.lineKind;
        case RichHtmlRole: return e.richHtml;
        case PlainTextRole: return e.plainText;
        case PromptPrefixRole: return e.promptPrefix;
        case ExitCodeRole: return e.exitCode;
        case DurationTextRole: return e.durationText;
        case WasCancelledRole: return e.wasCancelled;
        default: return {};
        }
    }

    QHash<int, QByteArray> TerminalOutputListModel::roleNames() const
    {
        return {
            {LineKindRole, "lineKind"},
            {RichHtmlRole, "richHtml"},
            {PlainTextRole, "plainText"},
            {PromptPrefixRole, "promptPrefix"},
            {ExitCodeRole, "exitCode"},
            {DurationTextRole, "durationText"},
            {WasCancelledRole, "wasCancelled"}
        };
    }

    void TerminalOutputListModel::trimIfNeeded()
    {
        if (static_cast<int>(m_entries.size()) <= kMaxLines) return;
        const int overflow = static_cast<int>(m_entries.size()) - kMaxLines;
        beginRemoveRows(QModelIndex(), 0, overflow - 1);
        m_entries.erase(m_entries.begin(), m_entries.begin() + overflow);
        endRemoveRows();
        emit countChanged();
    }

    int TerminalOutputListModel::appendCommandHeader(const QString& promptPrefix, const QString& commandText)
    {
        const int id = m_nextId++;
        const int row = static_cast<int>(m_entries.size());
        beginInsertRows(QModelIndex(), row, row);
        m_entries.push_back(StoredEntry{
            .id = id,
            .data = TerminalOutputEntry{
                .lineKind = QStringLiteral("command"),
                .richHtml = commandText.toHtmlEscaped(),
                .plainText = commandText,
                .promptPrefix = promptPrefix,
                .exitCode = -1,
                .durationText = {},
                .wasCancelled = false
            }
        });
        endInsertRows();
        emit countChanged();
        trimIfNeeded();
        return id;
    }

    void TerminalOutputListModel::finishCommandHeader(const int entryId,
                                                      const int exitCode,
                                                      const QString& durationText,
                                                      const bool wasCancelled)
    {
        for (int row = static_cast<int>(m_entries.size()) - 1; row >= 0; --row)
        {
            if (m_entries[static_cast<std::size_t>(row)].id == entryId)
            {
                auto& d = m_entries[static_cast<std::size_t>(row)].data;
                d.exitCode = exitCode;
                d.durationText = durationText;
                d.wasCancelled = wasCancelled;
                const QModelIndex idx = index(row, 0);
                emit dataChanged(idx, idx, {ExitCodeRole, DurationTextRole, WasCancelledRole});
                return;
            }
        }
    }

    void TerminalOutputListModel::appendLine(const QString& lineKind,
                                             const QString& richHtml,
                                             const QString& plainText)
    {
        const int row = static_cast<int>(m_entries.size());
        beginInsertRows(QModelIndex(), row, row);
        m_entries.push_back(StoredEntry{
            .id = m_nextId++,
            .data = TerminalOutputEntry{
                .lineKind = lineKind,
                .richHtml = richHtml,
                .plainText = plainText,
                .promptPrefix = {},
                .exitCode = -1,
                .durationText = {},
                .wasCancelled = false
            }
        });
        endInsertRows();
        emit countChanged();
        trimIfNeeded();
    }

    void TerminalOutputListModel::upsertProgressLine(const QString& lineKind,
                                                     const QString& richHtml,
                                                     const QString& plainText)
    {
        if (!m_entries.empty() && m_entries.back().data.lineKind == lineKind)
        {
            const int row = static_cast<int>(m_entries.size()) - 1;
            auto& d = m_entries.back().data;
            d.richHtml = richHtml;
            d.plainText = plainText;
            const QModelIndex idx = index(row, 0);
            emit dataChanged(idx, idx, {RichHtmlRole, PlainTextRole});
            return;
        }
        appendLine(lineKind, richHtml, plainText);
    }

    void TerminalOutputListModel::clear()
    {
        if (m_entries.empty()) return;
        beginResetModel();
        m_entries.clear();
        endResetModel();
        emit countChanged();
    }

    QString TerminalOutputListModel::allPlainText() const
    {
        QStringList lines;
        lines.reserve(static_cast<qsizetype>(m_entries.size()));
        for (const auto& e : m_entries)
        {
            if (e.data.lineKind == QStringLiteral("command"))
            {
                lines.append(e.data.promptPrefix + QStringLiteral(" ") + e.data.plainText);
            }
            else
            {
                lines.append(e.data.plainText);
            }
        }
        return lines.join(QLatin1Char('\n'));
    }
} // namespace gf::desktop
