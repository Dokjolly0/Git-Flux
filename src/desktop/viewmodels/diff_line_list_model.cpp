#include "desktop/viewmodels/diff_line_list_model.hpp"
#include "core/modules/syntax/syntax_highlighter.hpp"

namespace gf::desktop
{
    namespace
    {
        constexpr std::size_t kMaxRenderedDiffLines = 5000;
    } // namespace

    DiffLineListModel::DiffLineListModel(QObject* parent)
        : QAbstractListModel(parent)
    {
    }

    int DiffLineListModel::rowCount(const QModelIndex& parent) const
    {
        if (parent.isValid())
        {
            return 0;
        }
        return static_cast<int>(m_lines.size());
    }

    QVariant DiffLineListModel::data(const QModelIndex& index, int role) const
    {
        if (!index.isValid() || index.row() < 0 ||
            index.row() >= static_cast<int>(m_lines.size()))
        {
            return {};
        }

        std::size_t rowIdx = static_cast<std::size_t>(index.row());
        const auto& line = m_lines[rowIdx];

        switch (role)
        {
        case LineKindRole:
            switch (line.kind)
            {
            case gf::git::bo::DiffLineKind::Context: return QStringLiteral("context");
            case gf::git::bo::DiffLineKind::Addition: return QStringLiteral("addition");
            case gf::git::bo::DiffLineKind::Deletion: return QStringLiteral("deletion");
            case gf::git::bo::DiffLineKind::Header: return QStringLiteral("header");
            case gf::git::bo::DiffLineKind::NoNewline: return QStringLiteral("noNewline");
            }
            return QStringLiteral("context");

        case OldLineNumberRole:
            return line.oldLineNumber.has_value()
                       ? QString::number(*line.oldLineNumber)
                       : QString{};

        case NewLineNumberRole:
            return line.newLineNumber.has_value()
                       ? QString::number(*line.newLineNumber)
                       : QString{};

        case ContentRole:
        case Qt::DisplayRole:
            return QString::fromStdString(line.content);

        case RichContentRole:
            if (rowIdx < m_richLines.size())
            {
                return QString::fromStdString(m_richLines[rowIdx]);
            }
            return QString::fromStdString(line.content);

        case BgColorRole:
            switch (line.kind)
            {
            case gf::git::bo::DiffLineKind::Addition:
                return QStringLiteral("#143525"); // Subtle dark green
            case gf::git::bo::DiffLineKind::Deletion:
                return QStringLiteral("#3E181C"); // Subtle dark red
            case gf::git::bo::DiffLineKind::Header:
                return QStringLiteral("#1E293B"); // Dark slate blue
            case gf::git::bo::DiffLineKind::Context:
            case gf::git::bo::DiffLineKind::NoNewline:
            default:
                return QStringLiteral("transparent");
            }

        case FgColorRole:
            switch (line.kind)
            {
            case gf::git::bo::DiffLineKind::Addition:
                return QStringLiteral("#4ADE80"); // Bright green
            case gf::git::bo::DiffLineKind::Deletion:
                return QStringLiteral("#F87171"); // Salmon red
            case gf::git::bo::DiffLineKind::Header:
                return QStringLiteral("#93C5FD"); // Sky blue
            case gf::git::bo::DiffLineKind::NoNewline:
                return QStringLiteral("#64748B"); // Slate grey
            case gf::git::bo::DiffLineKind::Context:
            default:
                return QStringLiteral("#E2E8F0"); // Standard light text
            }

        default:
            return {};
        }
    }

    QHash<int, QByteArray> DiffLineListModel::roleNames() const
    {
        return {
            {LineKindRole, "lineKind"},
            {OldLineNumberRole, "oldLineNumber"},
            {NewLineNumberRole, "newLineNumber"},
            {ContentRole, "content"},
            {RichContentRole, "richContent"},
            {BgColorRole, "bgColor"},
            {FgColorRole, "fgColor"}
        };
    }

    void DiffLineListModel::setDiff(const gf::git::bo::FileDiff& diff,
                                    gf::core::syntax::Language lang,
                                    const std::vector<gf::core::syntax::MarkdownLineState>* newFileSeedStates)
    {
        beginResetModel();
        m_lines.clear();
        m_richLines.clear();

        std::size_t renderedLines = 0;
        bool truncated = false;
        const bool isMarkdown = (lang == gf::core::syntax::Language::Markdown);

        for (const auto& hunk : diff.hunks)
        {
            if (renderedLines >= kMaxRenderedDiffLines)
            {
                truncated = true;
                break;
            }
            // Add hunk header line
            m_lines.push_back(gf::git::bo::DiffLine{
                .kind = gf::git::bo::DiffLineKind::Header,
                .oldLineNumber = std::nullopt,
                .newLineNumber = std::nullopt,
                .content = hunk.header
            });
            m_richLines.push_back(gf::core::syntax::SyntaxHighlighter::escapeHtml(hunk.header));

            gf::core::syntax::MarkdownLineState oldMdState{};
            gf::core::syntax::MarkdownLineState newMdState{};
            if (isMarkdown && newFileSeedStates != nullptr && hunk.newStartLine > 0)
            {
                const std::size_t seedIdx = static_cast<std::size_t>(hunk.newStartLine - 1);
                if (seedIdx < newFileSeedStates->size())
                {
                    newMdState = (*newFileSeedStates)[seedIdx];
                    oldMdState = (*newFileSeedStates)[seedIdx];
                }
            }

            // Add all lines of the hunk
            for (const auto& l : hunk.lines)
            {
                if (renderedLines >= kMaxRenderedDiffLines)
                {
                    truncated = true;
                    break;
                }
                m_lines.push_back(l);
                if (!isMarkdown)
                {
                    m_richLines.push_back(gf::core::syntax::SyntaxHighlighter::highlightLine(l.content, lang));
                }
                else if (l.kind == gf::git::bo::DiffLineKind::Deletion)
                {
                    m_richLines.push_back(
                        gf::core::syntax::SyntaxHighlighter::highlightLine(l.content, lang, oldMdState));
                }
                else
                {
                    if (newFileSeedStates != nullptr && l.newLineNumber.has_value() && *l.newLineNumber > 0)
                    {
                        const std::size_t idx = static_cast<std::size_t>(*l.newLineNumber - 1);
                        if (idx < newFileSeedStates->size())
                        {
                            newMdState = (*newFileSeedStates)[idx];
                        }
                    }
                    m_richLines.push_back(
                        gf::core::syntax::SyntaxHighlighter::highlightLine(l.content, lang, newMdState));
                    if (l.kind == gf::git::bo::DiffLineKind::Context)
                    {
                        oldMdState = newMdState;
                    }
                }
                ++renderedLines;
            }
        }

        if (truncated)
        {
            const std::string notice =
                "... Diff troncato a 5000 righe per preservare la reattività dell'interfaccia ...";
            m_lines.push_back(gf::git::bo::DiffLine{
                .kind = gf::git::bo::DiffLineKind::Header,
                .oldLineNumber = std::nullopt,
                .newLineNumber = std::nullopt,
                .content = notice
            });
            m_richLines.push_back(gf::core::syntax::SyntaxHighlighter::escapeHtml(notice));
        }

        endResetModel();
    }

    void DiffLineListModel::clear()
    {
        beginResetModel();
        m_lines.clear();
        m_richLines.clear();
        endResetModel();
    }
} // namespace gf::desktop
