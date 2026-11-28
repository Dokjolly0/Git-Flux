#pragma once

#include <QAbstractListModel>
#include <QPointer>
#include <QString>
#include <vector>

class QQuickTextDocument;
class QTextDocument;

namespace gf::desktop
{
    struct TerminalOutputEntry
    {
        QString lineKind;     // "command", "stdout", "stderr", "system"
        QString richHtml;
        QString plainText;
        QString promptPrefix; // Used for "command" header entries
        int exitCode = -1;    // -1 = running or N/A, 0 = ok, >0 = error
        QString durationText;
        bool wasCancelled = false;
    };

    class TerminalOutputListModel final : public QAbstractListModel
    {
        Q_OBJECT
        Q_PROPERTY(int count READ count NOTIFY countChanged)

    public:
        static constexpr int kMaxLines = 5000;
        static constexpr int kMaxPagerLines = 15000;

        enum Roles
        {
            LineKindRole = Qt::UserRole + 1,
            RichHtmlRole,
            PlainTextRole,
            PromptPrefixRole,
            ExitCodeRole,
            DurationTextRole,
            WasCancelledRole
        };

        explicit TerminalOutputListModel(QObject* parent = nullptr,
                                         int maxLines = kMaxLines,
                                         bool dropOldest = true);
        ~TerminalOutputListModel() override;

        [[nodiscard]] int rowCount(const QModelIndex& parent = QModelIndex()) const override;
        [[nodiscard]] QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
        [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

        [[nodiscard]] int count() const noexcept { return static_cast<int>(m_entries.size()); }

        /// Attaches a QML TextEdit's QQuickTextDocument for native multi-line text selection & copy.
        /// Automatically detaches any previously attached model from the same document.
        Q_INVOKABLE void attachTextDocument(QQuickTextDocument* quickDoc);

        /// Appends a command header row and returns its unique monotonic entry id.
        int appendCommandHeader(const QString& promptPrefix, const QString& commandText);

        /// Updates exit code / duration on the command header identified by `entryId`.
        void finishCommandHeader(int entryId, int exitCode, const QString& durationText, bool wasCancelled);

        /// Appends an output line ("stdout", "stderr", "system").
        void appendLine(const QString& lineKind, const QString& richHtml, const QString& plainText);

        /// Overwrites the last output line in place (for `\r` progress bar updates) if it matches `lineKind`.
        void upsertProgressLine(const QString& lineKind, const QString& richHtml, const QString& plainText);

        /// Copies all entries from this model into `target` in order.
        void copyLinesTo(TerminalOutputListModel& target) const;

        void clear();

        [[nodiscard]] QString allPlainText() const;

    signals:
        void countChanged();

    private:
        void detachTextDocument();
        void trimIfNeeded();
        void syncFullDocument();
        void appendEntryToDocument(const TerminalOutputEntry& entry, bool isFirstBlock);
        void replaceBlockInDocument(int row, const TerminalOutputEntry& entry);
        [[nodiscard]] static QString formatEntryHtml(const TerminalOutputEntry& entry);

        struct StoredEntry
        {
            int id = 0;
            TerminalOutputEntry data;
        };

        std::vector<StoredEntry> m_entries;
        int m_nextId = 1;
        int m_maxLines = kMaxLines;
        bool m_dropOldest = true;
        bool m_truncated = false;
        QPointer<QTextDocument> m_attachedDoc;
    };
} // namespace gf::desktop
