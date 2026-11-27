#include "desktop/viewmodels/terminal_output_list_model.hpp"

#include <QColor>
#include <QQuickTextDocument>
#include <QStringList>
#include <QTextBlock>
#include <QTextBlockFormat>
#include <QTextCursor>
#include <QTextDocument>

#include <unordered_map>

namespace gf::desktop
{
    namespace
    {
        std::unordered_map<QTextDocument*, TerminalOutputListModel*>& docOwnerRegistry()
        {
            static std::unordered_map<QTextDocument*, TerminalOutputListModel*> registry;
            return registry;
        }

        [[nodiscard]] QTextBlockFormat blockFormatForKind(const QString& lineKind)
        {
            QTextBlockFormat fmt;
            fmt.setLeftMargin(4.0);
            fmt.setRightMargin(4.0);
            if (lineKind == QStringLiteral("command"))
            {
                fmt.setTopMargin(7.0);
                fmt.setBottomMargin(3.0);
                fmt.setBackground(QColor(QStringLiteral("#0E1623")));
            }
            else
            {
                fmt.setTopMargin(0.0);
                fmt.setBottomMargin(1.0);
                fmt.setBackground(Qt::transparent);
            }
            return fmt;
        }
    } // namespace

    TerminalOutputListModel::TerminalOutputListModel(QObject* parent,
                                                     const int maxLines,
                                                     const bool dropOldest)
        : QAbstractListModel(parent)
        , m_maxLines(maxLines > 0 ? maxLines : kMaxLines)
        , m_dropOldest(dropOldest)
    {
    }

    TerminalOutputListModel::~TerminalOutputListModel()
    {
        detachTextDocument();
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

    void TerminalOutputListModel::attachTextDocument(QQuickTextDocument* quickDoc)
    {
        QTextDocument* doc = quickDoc ? quickDoc->textDocument() : nullptr;
        if (m_attachedDoc == doc)
        {
            return;
        }

        detachTextDocument();

        if (!doc)
        {
            return;
        }

        auto& reg = docOwnerRegistry();
        if (const auto it = reg.find(doc); it != reg.end() && it->second && it->second != this)
        {
            it->second->detachTextDocument();
        }

        m_attachedDoc = doc;
        reg[doc] = this;
        doc->setDocumentMargin(4.0);
        syncFullDocument();
    }

    void TerminalOutputListModel::detachTextDocument()
    {
        if (!m_attachedDoc) return;
        auto& reg = docOwnerRegistry();
        if (const auto it = reg.find(m_attachedDoc.data()); it != reg.end() && it->second == this)
        {
            reg.erase(it);
        }
        m_attachedDoc = nullptr;
    }

    QString TerminalOutputListModel::formatEntryHtml(const TerminalOutputEntry& entry)
    {
        if (entry.lineKind == QStringLiteral("command"))
        {
            QString statusHtml;
            if (entry.exitCode == -1)
            {
                statusHtml = QStringLiteral("&nbsp;&nbsp;<span style=\"color:#38BDF8;font-size:10px;\">[⏳ in esecuzione]</span>");
            }
            else if (entry.wasCancelled)
            {
                statusHtml = QStringLiteral("&nbsp;&nbsp;<span style=\"color:#FBBF24;font-size:10px;\">[^C · %1]</span>")
                                 .arg(entry.durationText.toHtmlEscaped());
            }
            else if (entry.exitCode == 0)
            {
                statusHtml = QStringLiteral("&nbsp;&nbsp;<span style=\"color:#34D399;font-size:10px;\">[✓ %1]</span>")
                                 .arg(entry.durationText.toHtmlEscaped());
            }
            else
            {
                statusHtml = QStringLiteral("&nbsp;&nbsp;<span style=\"color:#F87171;font-size:10px;\">[✕ %1 · %2]</span>")
                                 .arg(entry.exitCode)
                                 .arg(entry.durationText.toHtmlEscaped());
            }

            return QStringLiteral("<span style=\"color:#38BDF8;font-weight:700;\">%1</span>&nbsp;<span style=\"color:#F8FAFC;font-weight:700;\">%2</span>%3")
                .arg(entry.promptPrefix.toHtmlEscaped(), entry.plainText.toHtmlEscaped(), statusHtml);
        }

        if (entry.richHtml.isEmpty())
        {
            return {};
        }

        if (entry.lineKind == QStringLiteral("stderr"))
        {
            return QStringLiteral("<span style=\"color:#FCA5A5;\">%1</span>").arg(entry.richHtml);
        }
        if (entry.lineKind == QStringLiteral("system"))
        {
            return QStringLiteral("<span style=\"color:#64748B;font-style:italic;\">%1</span>").arg(entry.richHtml);
        }
        return entry.richHtml;
    }

    void TerminalOutputListModel::syncFullDocument()
    {
        if (!m_attachedDoc) return;
        m_attachedDoc->clear();
        if (m_entries.empty()) return;

        QTextCursor cursor(m_attachedDoc.data());
        cursor.beginEditBlock();
        for (std::size_t i = 0; i < m_entries.size(); ++i)
        {
            const auto& entry = m_entries[i].data;
            const QTextBlockFormat fmt = blockFormatForKind(entry.lineKind);
            if (i == 0)
            {
                cursor.setBlockFormat(fmt);
            }
            else
            {
                cursor.insertBlock(fmt);
            }
            const QString html = formatEntryHtml(entry);
            if (!html.isEmpty())
            {
                cursor.insertHtml(html);
            }
        }
        cursor.endEditBlock();
    }

    void TerminalOutputListModel::appendEntryToDocument(const TerminalOutputEntry& entry, const bool isFirstBlock)
    {
        if (!m_attachedDoc) return;
        QTextCursor cursor(m_attachedDoc.data());
        cursor.movePosition(QTextCursor::End);
        cursor.beginEditBlock();
        const QTextBlockFormat fmt = blockFormatForKind(entry.lineKind);
        if (isFirstBlock)
        {
            cursor.setBlockFormat(fmt);
        }
        else
        {
            cursor.insertBlock(fmt);
        }
        const QString html = formatEntryHtml(entry);
        if (!html.isEmpty())
        {
            cursor.insertHtml(html);
        }
        cursor.endEditBlock();
    }

    void TerminalOutputListModel::replaceBlockInDocument(const int row, const TerminalOutputEntry& entry)
    {
        if (!m_attachedDoc || row < 0) return;
        const QTextBlock block = m_attachedDoc->findBlockByNumber(row);
        if (!block.isValid()) return;

        QTextCursor cursor(block);
        cursor.beginEditBlock();
        cursor.movePosition(QTextCursor::StartOfBlock);
        cursor.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
        cursor.removeSelectedText();
        cursor.setBlockFormat(blockFormatForKind(entry.lineKind));
        const QString html = formatEntryHtml(entry);
        if (!html.isEmpty())
        {
            cursor.insertHtml(html);
        }
        cursor.endEditBlock();
    }

    void TerminalOutputListModel::trimIfNeeded()
    {
        if (!m_dropOldest || static_cast<int>(m_entries.size()) <= m_maxLines) return;
        const int overflow = static_cast<int>(m_entries.size()) - m_maxLines;
        beginRemoveRows(QModelIndex(), 0, overflow - 1);
        m_entries.erase(m_entries.begin(), m_entries.begin() + overflow);
        endRemoveRows();

        if (m_attachedDoc)
        {
            const QTextBlock targetBlock = m_attachedDoc->findBlockByNumber(overflow);
            if (targetBlock.isValid())
            {
                QTextCursor cursor(m_attachedDoc.data());
                cursor.beginEditBlock();
                cursor.movePosition(QTextCursor::Start);
                cursor.setPosition(targetBlock.position(), QTextCursor::KeepAnchor);
                cursor.removeSelectedText();
                cursor.endEditBlock();
            }
            else
            {
                syncFullDocument();
            }
        }
        emit countChanged();
    }

    int TerminalOutputListModel::appendCommandHeader(const QString& promptPrefix, const QString& commandText)
    {
        const int id = m_nextId++;
        const int row = static_cast<int>(m_entries.size());
        const bool isFirst = m_entries.empty();
        TerminalOutputEntry entry{
            .lineKind = QStringLiteral("command"),
            .richHtml = commandText.toHtmlEscaped(),
            .plainText = commandText,
            .promptPrefix = promptPrefix,
            .exitCode = -1,
            .durationText = {},
            .wasCancelled = false
        };

        beginInsertRows(QModelIndex(), row, row);
        m_entries.push_back(StoredEntry{.id = id, .data = entry});
        endInsertRows();
        appendEntryToDocument(m_entries.back().data, isFirst);
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
                replaceBlockInDocument(row, d);
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
        if (!m_dropOldest)
        {
            if (m_truncated)
            {
                return;
            }
            if (static_cast<int>(m_entries.size()) >= m_maxLines)
            {
                m_truncated = true;
                const QString notice = QStringLiteral("… Output limitato a %1 righe nel pager (usa filtri come -n per restringere).").arg(m_maxLines);
                const int row = static_cast<int>(m_entries.size());
                const bool isFirst = m_entries.empty();
                TerminalOutputEntry noticeEntry{
                    .lineKind = QStringLiteral("system"),
                    .richHtml = notice.toHtmlEscaped(),
                    .plainText = notice,
                    .promptPrefix = {},
                    .exitCode = -1,
                    .durationText = {},
                    .wasCancelled = false
                };
                beginInsertRows(QModelIndex(), row, row);
                m_entries.push_back(StoredEntry{.id = m_nextId++, .data = std::move(noticeEntry)});
                endInsertRows();
                appendEntryToDocument(m_entries.back().data, isFirst);
                emit countChanged();
                return;
            }
        }

        const int row = static_cast<int>(m_entries.size());
        const bool isFirst = m_entries.empty();
        TerminalOutputEntry entry{
            .lineKind = lineKind,
            .richHtml = richHtml,
            .plainText = plainText,
            .promptPrefix = {},
            .exitCode = -1,
            .durationText = {},
            .wasCancelled = false
        };
        beginInsertRows(QModelIndex(), row, row);
        m_entries.push_back(StoredEntry{.id = m_nextId++, .data = std::move(entry)});
        endInsertRows();
        appendEntryToDocument(m_entries.back().data, isFirst);
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
            replaceBlockInDocument(row, d);
            const QModelIndex idx = index(row, 0);
            emit dataChanged(idx, idx, {RichHtmlRole, PlainTextRole});
            return;
        }
        appendLine(lineKind, richHtml, plainText);
    }

    void TerminalOutputListModel::copyLinesTo(TerminalOutputListModel& target) const
    {
        if (m_entries.empty()) return;
        for (const auto& e : m_entries)
        {
            target.appendLine(e.data.lineKind, e.data.richHtml, e.data.plainText);
        }
    }

    void TerminalOutputListModel::clear()
    {
        m_truncated = false;
        if (m_entries.empty())
        {
            if (m_attachedDoc) m_attachedDoc->clear();
            return;
        }
        beginResetModel();
        m_entries.clear();
        endResetModel();
        if (m_attachedDoc)
        {
            m_attachedDoc->clear();
        }
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
