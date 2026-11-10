#include "desktop/viewmodels/diff_viewmodel.hpp"
#include "desktop/viewmodels/commit_history_list_model.hpp"

#include "core/shared/utf8_path.hpp"
#include "core/modules/git/bl/git_diff_service.hpp"
#include "core/modules/syntax/language_detector.hpp"
#include "core/modules/syntax/markdown_parser.hpp"
#include "core/modules/syntax/syntax_highlighter.hpp"

#include <QClipboard>
#include <QCryptographicHash>
#include <QGuiApplication>
#include <QHash>
#include <QStringList>
#include <QVariantMap>
#include <algorithm>
#include <filesystem>

namespace gf::desktop
{
    namespace
    {
        constexpr std::size_t kMaxRenderedDiffLines = 5000;
        constexpr int kMaxVisualLineLength = 4096;

        const QStringList kBlameColors = {
            QStringLiteral("#6366F1"),
            QStringLiteral("#06B6D4"),
            QStringLiteral("#10B981"),
            QStringLiteral("#F59E0B"),
            QStringLiteral("#EC4899"),
            QStringLiteral("#8B5CF6"),
            QStringLiteral("#14B8A6"),
            QStringLiteral("#3B82F6"),
            QStringLiteral("#E11D48"),
            QStringLiteral("#84CC16")
        };

        QString computeBlameAvatar(const QString& rawName, const QString& rawEmail)
        {
            QString email = rawEmail.trimmed().toLower();
            QString cleanName = rawName.trimmed();
            if (!cleanName.isEmpty() && !cleanName.contains(QLatin1Char(' ')) &&
                cleanName.compare(QLatin1String("GitHub"), Qt::CaseInsensitive) != 0)
            {
                return QStringLiteral("https://github.com/%1.png?size=64").arg(cleanName);
            }
            if (email.isEmpty())
            {
                return QStringLiteral("https://www.gravatar.com/avatar/?d=mp");
            }
            if (email.endsWith(QLatin1String("users.noreply.github.com")))
            {
                int atIdx = email.indexOf(QLatin1Char('@'));
                if (atIdx > 0)
                {
                    QString userPart = email.left(atIdx);
                    int plusIdx = userPart.indexOf(QLatin1Char('+'));
                    if (plusIdx >= 0) userPart = userPart.mid(plusIdx + 1);
                    if (!userPart.isEmpty())
                    {
                        return QStringLiteral("https://github.com/%1.png?size=64").arg(userPart);
                    }
                }
            }
            QString cached = CommitHistoryListModel::cachedAvatarForEmail(email);
            if (!cached.isEmpty())
            {
                return cached;
            }
            QByteArray hash = QCryptographicHash::hash(email.toUtf8(), QCryptographicHash::Md5).toHex();
            return QStringLiteral("https://www.gravatar.com/avatar/%1?s=64&d=identicon").arg(QString::fromLatin1(hash));
        }

        QString computeBlameInitials(const QString& name)
        {
            QString clean = name.trimmed();
            if (clean.isEmpty()) return QStringLiteral("??");
            QStringList parts = clean.split(QLatin1Char(' '), Qt::SkipEmptyParts);
            if (parts.size() >= 2)
            {
                return QString(parts[0].front().toUpper()) + QString(parts[1].front().toUpper());
            }
            if (clean.length() >= 2) return clean.left(2).toUpper();
            return clean.toUpper();
        }

        QString computeBlameColor(const QString& key)
        {
            const size_t hash = qHash(key);
            const int idx = static_cast<int>(hash % kBlameColors.size());
            return kBlameColors[idx];
        }

        QString blockKindToString(gf::core::syntax::MarkdownBlockKind kind)
        {
            using K = gf::core::syntax::MarkdownBlockKind;
            switch (kind)
            {
            case K::Heading: return QStringLiteral("heading");
            case K::Paragraph: return QStringLiteral("paragraph");
            case K::CodeBlock: return QStringLiteral("code");
            case K::Blockquote: return QStringLiteral("blockquote");
            case K::ListItem: return QStringLiteral("listItem");
            case K::Table: return QStringLiteral("table");
            case K::ThematicBreak: return QStringLiteral("thematicBreak");
            case K::BadgeRow: return QStringLiteral("badgeRow");
            case K::DeletedSnippet: return QStringLiteral("deletedSnippet");
            }
            return QStringLiteral("paragraph");
        }

        QString alertTypeToString(gf::core::syntax::MarkdownAlertType alert)
        {
            using A = gf::core::syntax::MarkdownAlertType;
            switch (alert)
            {
            case A::Note: return QStringLiteral("note");
            case A::Tip: return QStringLiteral("tip");
            case A::Important: return QStringLiteral("important");
            case A::Warning: return QStringLiteral("warning");
            case A::Caution: return QStringLiteral("caution");
            case A::None:
            default: return QStringLiteral("none");
            }
        }

        QString changeStatusToString(gf::core::syntax::MarkdownChangeStatus status)
        {
            using S = gf::core::syntax::MarkdownChangeStatus;
            switch (status)
            {
            case S::Added: return QStringLiteral("added");
            case S::Modified: return QStringLiteral("modified");
            case S::Deleted: return QStringLiteral("deleted");
            case S::Unchanged:
            default: return QStringLiteral("unchanged");
            }
        }

        std::string reconstructMarkdownFallbackFromDiff(const gf::git::bo::FileDiff& diff, bool& hasNewSideLines)
        {
            std::string out;
            hasNewSideLines = false;
            for (const auto& hunk : diff.hunks)
            {
                for (const auto& line : hunk.lines)
                {
                    if (line.kind == gf::git::bo::DiffLineKind::Context ||
                        line.kind == gf::git::bo::DiffLineKind::Addition)
                    {
                        hasNewSideLines = true;
                        out.append(line.content);
                        out.push_back('\n');
                    }
                }
            }
            return out;
        }

        struct HighlightedDiffLine
        {
            const gf::git::bo::DiffLine* raw = nullptr;
            QString content;
            QString richContent;
        };
    } // namespace

    DiffViewModel::DiffViewModel(gf::git::bl::GitDiffService& diffService, QObject* parent)
        : QObject(parent)
          , m_diffService(diffService)
          , m_linesModel(this)
    {
    }

    void DiffViewModel::setViewMode(const QString& mode)
    {
        if (m_viewMode == mode) return;
        m_viewMode = mode;
        emit viewModeChanged();

        if (m_viewMode == QStringLiteral("blame") && m_blameLines.isEmpty() && !m_filePath.isEmpty())
        {
            loadBlame(m_currentRepoPath, m_currentRevision, m_filePath);
        }
    }

    void DiffViewModel::setContextLines(int lines)
    {
        int normalized = (lines < 0) ? -1 : std::min(lines, 5000);
        if (m_contextLines == normalized) return;
        m_contextLines = normalized;
        if (m_contextLines >= 0)
        {
            m_lastNonFullContextLines = m_contextLines;
        }
        emit contextLinesChanged();
        (void)reloadCurrentDiff();
    }

    void DiffViewModel::expandContext(int delta)
    {
        if (m_contextLines < 0 || delta <= 0) return;
        setContextLines(std::min(m_contextLines + delta, 5000));
    }

    void DiffViewModel::toggleFullContext()
    {
        if (m_contextLines < 0)
        {
            setContextLines(m_lastNonFullContextLines > 0 ? m_lastNonFullContextLines : 10);
        }
        else
        {
            m_lastNonFullContextLines = m_contextLines;
            setContextLines(-1);
        }
    }

    void DiffViewModel::setMarkdownRendered(bool rendered)
    {
        if (m_preferMarkdownRendered == rendered) return;
        m_preferMarkdownRendered = rendered;
        emit markdownRenderedChanged();
    }

    void DiffViewModel::toggleMarkdownRendered()
    {
        setMarkdownRendered(!m_preferMarkdownRendered);
    }

    void DiffViewModel::setShowMarkdownRichDiff(bool show)
    {
        if (m_showMarkdownRichDiff == show) return;
        m_showMarkdownRichDiff = show;
        emit showMarkdownRichDiffChanged();
    }

    bool DiffViewModel::reloadCurrentDiff()
    {
        if (m_currentRepoPath.isEmpty() || m_filePath.isEmpty())
        {
            return false;
        }
        if (!m_currentRevision.isEmpty())
        {
            return loadCommitFileDiff(m_currentRepoPath, m_currentRevision, m_filePath);
        }
        const QString scope = m_lastScope.isEmpty() ? QStringLiteral("unstaged") : m_lastScope;
        return loadDiff(m_currentRepoPath, m_filePath, scope);
    }

    void DiffViewModel::copyToClipboard(const QString& text) const
    {
        if (qGuiApp)
        {
            if (auto* cb = QGuiApplication::clipboard())
            {
                cb->setText(text);
            }
        }
    }

    void DiffViewModel::populateMarkdownDocument(const std::string& content, const gf::git::bo::FileDiff& diff)
    {
        m_markdownSource = QString::fromStdString(content);
        auto mdDoc = gf::core::syntax::MarkdownParser::parseWithDiff(content, &diff);

        m_markdownBlocks.clear();
        m_markdownBlocks.reserve(static_cast<qsizetype>(mdDoc.blocks.size()));
        for (const auto& b : mdDoc.blocks)
        {
            QVariantMap map;
            map[QStringLiteral("kind")] = blockKindToString(b.kind);
            map[QStringLiteral("level")] = b.level;
            map[QStringLiteral("indentLevel")] = b.indentLevel;
            map[QStringLiteral("ordered")] = b.ordered;
            map[QStringLiteral("isTask")] = b.isTask;
            map[QStringLiteral("taskChecked")] = b.taskChecked;
            map[QStringLiteral("centered")] = b.centered;
            map[QStringLiteral("isDiagram")] = b.isDiagram;
            map[QStringLiteral("listMarker")] = QString::fromStdString(b.listMarker);
            map[QStringLiteral("alertType")] = alertTypeToString(b.alertType);
            map[QStringLiteral("rawText")] = QString::fromStdString(b.rawText);
            map[QStringLiteral("html")] = QString::fromStdString(b.html);
            map[QStringLiteral("anchor")] = QString::fromStdString(b.anchor);
            map[QStringLiteral("languageTag")] = QString::fromStdString(b.languageTag);

            QStringList headers;
            for (const auto& h : b.tableHeaders) headers.append(QString::fromStdString(h));
            map[QStringLiteral("tableHeaders")] = headers;

            QStringList aligns;
            for (const auto& a : b.tableAlignments) aligns.append(QString::fromStdString(a));
            map[QStringLiteral("tableAlignments")] = aligns;

            QVariantList rows;
            for (const auto& r : b.tableRows)
            {
                QStringList cells;
                for (const auto& c : r) cells.append(QString::fromStdString(c));
                rows.append(cells);
            }
            map[QStringLiteral("tableRows")] = rows;

            QVariantList badges;
            for (const auto& badge : b.badges)
            {
                QVariantMap bm;
                bm[QStringLiteral("label")] = QString::fromStdString(badge.label);
                bm[QStringLiteral("href")] = QString::fromStdString(badge.href);
                bm[QStringLiteral("color")] = QString::fromStdString(badge.color);
                badges.append(bm);
            }
            map[QStringLiteral("badges")] = badges;

            map[QStringLiteral("startLine")] = b.startLine;
            map[QStringLiteral("endLine")] = b.endLine;
            map[QStringLiteral("changeStatus")] = changeStatusToString(b.changeStatus);
            map[QStringLiteral("addedLineCount")] = b.addedLineCount;
            map[QStringLiteral("deletedLineCount")] = b.deletedLineCount;

            QStringList delList;
            for (const auto& dl : b.deletedLines) delList.append(QString::fromStdString(dl));
            map[QStringLiteral("deletedLinesText")] = delList.join(QLatin1Char('\n'));

            m_markdownBlocks.append(map);
        }

        m_markdownToc.clear();
        m_markdownToc.reserve(static_cast<qsizetype>(mdDoc.toc.size()));
        for (const auto& t : mdDoc.toc)
        {
            QVariantMap tm;
            tm[QStringLiteral("level")] = t.level;
            tm[QStringLiteral("title")] = QString::fromStdString(t.title);
            tm[QStringLiteral("anchor")] = QString::fromStdString(t.anchor);
            tm[QStringLiteral("blockIndex")] = t.blockIndex;
            tm[QStringLiteral("startLine")] = t.startLine;
            m_markdownToc.append(tm);
        }
    }

    void DiffViewModel::updateMarkdownStateForDiff(const std::filesystem::path& repoPath,
                                                   const std::string& filePath,
                                                   const std::string& primaryRevision,
                                                   const std::string& fallbackRevision,
                                                   const std::string& deletedFallbackNotice,
                                                   const gf::git::bo::FileDiff& diff,
                                                   std::vector<gf::core::syntax::MarkdownLineState>& outSeedStates,
                                                   bool& outHasSeedStates)
    {
        outSeedStates.clear();
        outHasSeedStates = false;
        m_markdownBlocks.clear();
        m_markdownToc.clear();
        m_markdownSource.clear();
        m_markdownNotice.clear();

        const std::string targetPath = diff.isDeletedFile
                                           ? (diff.oldPath.empty() ? filePath : diff.oldPath)
                                           : (diff.newPath.empty() ? filePath : diff.newPath);

        auto contentRes = m_diffService.getFileContent(repoPath, primaryRevision, targetPath);
        if (!contentRes && !fallbackRevision.empty())
        {
            contentRes = m_diffService.getFileContent(repoPath, fallbackRevision, targetPath);
        }

        if (contentRes)
        {
            std::string content = std::move(*contentRes);
            if (content.empty() && !diff.isDeletedFile && !diff.hunks.empty())
            {
                bool hasNewSideLines = false;
                std::string reconstructed = reconstructMarkdownFallbackFromDiff(diff, hasNewSideLines);
                if (hasNewSideLines)
                {
                    content = std::move(reconstructed);
                }
            }

            if (diff.isDeletedFile && !deletedFallbackNotice.empty())
            {
                m_markdownNotice = QString::fromStdString(deletedFallbackNotice);
            }

            if (!diff.isDeletedFile)
            {
                outSeedStates = gf::core::syntax::SyntaxHighlighter::computeMarkdownLineStates(content);
                outHasSeedStates = !outSeedStates.empty();
            }
            populateMarkdownDocument(content, diff);
            emit markdownDocumentChanged();
            return;
        }

        // Fallback: build partial preview from hunk lines when full file cannot be read
        bool hasNewSideLines = false;
        std::string fallback = reconstructMarkdownFallbackFromDiff(diff, hasNewSideLines);
        if (!fallback.empty())
        {
            populateMarkdownDocument(fallback, diff);
        }
        m_markdownNotice = QStringLiteral("Impossibile caricare il file completo (%1) — anteprima parziale.")
            .arg(QString::fromStdString(contentRes.error().message));
        emit markdownDocumentChanged();
    }

    void DiffViewModel::processFileDiff(const gf::git::bo::FileDiff& diff,
                                        const std::vector<gf::core::syntax::MarkdownLineState>* newFileSeedStates)
    {
        m_oldPath = QString::fromStdString(diff.oldPath);
        m_additionsCount = diff.additionsCount;
        m_deletionsCount = diff.deletionsCount;
        m_isBinary = diff.isBinary;
        m_isNewFile = diff.isNewFile;
        m_isDeletedFile = diff.isDeletedFile;
        m_isRenamed = diff.isRenamed;
        m_isEmpty = diff.empty();

        std::string pathForDetect = diff.displayPath();
        auto langInfo = gf::core::syntax::LanguageDetector::detect(pathForDetect);
        m_languageName = QString::fromStdString(langInfo.name);
        m_languageIcon = QString::fromStdString(langInfo.icon);

        const bool wasMarkdown = m_isMarkdown;
        const bool isMd = (langInfo.language == gf::core::syntax::Language::Markdown);
        m_isMarkdown = isMd;
        if (!isMd)
        {
            m_markdownBlocks.clear();
            m_markdownToc.clear();
            m_markdownSource.clear();
            m_markdownNotice.clear();
            if (wasMarkdown)
            {
                emit markdownDocumentChanged();
            }
        }

        m_linesModel.setDiff(diff, langInfo.language, newFileSeedStates);
        m_blameLines.clear();

        m_maxLineLength = 0;
        m_hunks.clear();
        m_splitLines.clear();

        std::size_t totalProcessedLines = 0;
        bool truncated = false;

        for (const auto& hunk : diff.hunks)
        {
            if (totalProcessedLines >= kMaxRenderedDiffLines)
            {
                truncated = true;
                break;
            }

            // Precompute highlighted strings once for all lines in this hunk (up to cap)
            std::vector<HighlightedDiffLine> preparedLines;
            preparedLines.reserve(std::min(hunk.lines.size(), kMaxRenderedDiffLines - totalProcessedLines));

            gf::core::syntax::MarkdownLineState oldMdState{};
            gf::core::syntax::MarkdownLineState newMdState{};
            if (isMd && newFileSeedStates != nullptr && hunk.newStartLine > 0)
            {
                const std::size_t seedIdx = static_cast<std::size_t>(hunk.newStartLine - 1);
                if (seedIdx < newFileSeedStates->size())
                {
                    newMdState = (*newFileSeedStates)[seedIdx];
                    oldMdState = (*newFileSeedStates)[seedIdx];
                }
            }

            for (const auto& line : hunk.lines)
            {
                if (totalProcessedLines >= kMaxRenderedDiffLines)
                {
                    truncated = true;
                    break;
                }
                int visualLen = 0;
                for (char c : line.content)
                {
                    if (c == '\t')
                    {
                        visualLen += 4 - (visualLen % 4);
                    }
                    else
                    {
                        visualLen += 1;
                    }
                    if (visualLen >= kMaxVisualLineLength)
                    {
                        visualLen = kMaxVisualLineLength;
                        break;
                    }
                }
                if (visualLen > m_maxLineLength)
                {
                    m_maxLineLength = visualLen;
                }

                std::string highlightedHtml;
                if (!isMd)
                {
                    highlightedHtml = gf::core::syntax::SyntaxHighlighter::highlightLine(
                        line.content, langInfo.language);
                }
                else if (line.kind == gf::git::bo::DiffLineKind::Deletion)
                {
                    highlightedHtml = gf::core::syntax::SyntaxHighlighter::highlightLine(
                        line.content, langInfo.language, oldMdState);
                }
                else
                {
                    if (newFileSeedStates != nullptr && line.newLineNumber.has_value() && *line.newLineNumber > 0)
                    {
                        const std::size_t idx = static_cast<std::size_t>(*line.newLineNumber - 1);
                        if (idx < newFileSeedStates->size())
                        {
                            newMdState = (*newFileSeedStates)[idx];
                        }
                    }
                    highlightedHtml = gf::core::syntax::SyntaxHighlighter::highlightLine(
                        line.content, langInfo.language, newMdState);
                    if (line.kind == gf::git::bo::DiffLineKind::Context)
                    {
                        oldMdState = newMdState;
                    }
                }

                preparedLines.push_back(HighlightedDiffLine{
                    .raw = &line,
                    .content = QString::fromStdString(line.content),
                    .richContent = QString::fromStdString(highlightedHtml)
                });
                ++totalProcessedLines;
            }

            const QString hunkHeaderStr = QString::fromStdString(hunk.header);

            // 1. Build Hunk Entry
            QVariantMap hMap;
            hMap[QStringLiteral("header")] = hunkHeaderStr;
            hMap[QStringLiteral("oldStartLine")] = hunk.oldStartLine;
            hMap[QStringLiteral("oldCount")] = hunk.oldCount;
            hMap[QStringLiteral("newStartLine")] = hunk.newStartLine;
            hMap[QStringLiteral("newCount")] = hunk.newCount;

            QVariantList hLines;
            hLines.reserve(static_cast<qsizetype>(preparedLines.size()));
            for (const auto& prep : preparedLines)
            {
                const auto& line = *prep.raw;
                QVariantMap lMap;
                QString kindStr;
                QString bgCol = QStringLiteral("transparent");
                QString fgCol = QStringLiteral("#E6EDF3");

                switch (line.kind)
                {
                case gf::git::bo::DiffLineKind::Addition:
                    kindStr = QStringLiteral("addition");
                    bgCol = QStringLiteral("#12261E");
                    fgCol = QStringLiteral("#7EE787");
                    break;
                case gf::git::bo::DiffLineKind::Deletion:
                    kindStr = QStringLiteral("deletion");
                    bgCol = QStringLiteral("#291518");
                    fgCol = QStringLiteral("#FFA198");
                    break;
                case gf::git::bo::DiffLineKind::Header:
                    kindStr = QStringLiteral("header");
                    bgCol = QStringLiteral("#161B22");
                    fgCol = QStringLiteral("#7D8590");
                    break;
                default:
                    kindStr = QStringLiteral("context");
                    break;
                }

                lMap[QStringLiteral("kind")] = kindStr;
                lMap[QStringLiteral("oldLineNumber")] = line.oldLineNumber.has_value()
                                                            ? QString::number(*line.oldLineNumber)
                                                            : QString();
                lMap[QStringLiteral("newLineNumber")] = line.newLineNumber.has_value()
                                                            ? QString::number(*line.newLineNumber)
                                                            : QString();
                lMap[QStringLiteral("content")] = prep.content;
                lMap[QStringLiteral("richContent")] = prep.richContent;
                lMap[QStringLiteral("bgColor")] = bgCol;
                lMap[QStringLiteral("fgColor")] = fgCol;
                hLines.append(lMap);
            }
            hMap[QStringLiteral("lines")] = hLines;
            m_hunks.append(hMap);

            // 2. Build Split Lines (side-by-side alignment) reusing preparedLines
            QVariantMap headerRow;
            headerRow[QStringLiteral("isHunkHeader")] = true;
            headerRow[QStringLiteral("hunkHeader")] = hunkHeaderStr;
            m_splitLines.append(headerRow);

            std::vector<const HighlightedDiffLine*> pendingDeletions;
            std::vector<const HighlightedDiffLine*> pendingAdditions;

            auto flushPending = [this, &pendingDeletions, &pendingAdditions]()
            {
                std::size_t maxLen = std::max(pendingDeletions.size(), pendingAdditions.size());
                for (std::size_t i = 0; i < maxLen; ++i)
                {
                    QVariantMap row;
                    row[QStringLiteral("isHunkHeader")] = false;

                    if (i < pendingDeletions.size())
                    {
                        const auto& del = *pendingDeletions[i];
                        row[QStringLiteral("leftLine")] = del.raw->oldLineNumber.has_value()
                                                              ? QString::number(*del.raw->oldLineNumber)
                                                              : QString();
                        row[QStringLiteral("leftContent")] = del.content;
                        row[QStringLiteral("leftRichContent")] = del.richContent;
                        row[QStringLiteral("leftKind")] = QStringLiteral("deletion");
                        row[QStringLiteral("leftBgColor")] = QStringLiteral("#3B181E");
                        row[QStringLiteral("leftFgColor")] = QStringLiteral("#FFA198");
                    }
                    else
                    {
                        row[QStringLiteral("leftLine")] = QString();
                        row[QStringLiteral("leftContent")] = QString();
                        row[QStringLiteral("leftRichContent")] = QString();
                        row[QStringLiteral("leftKind")] = QStringLiteral("empty");
                        row[QStringLiteral("leftBgColor")] = QStringLiteral("#0D1117");
                        row[QStringLiteral("leftFgColor")] = QStringLiteral("#484F58");
                    }

                    if (i < pendingAdditions.size())
                    {
                        const auto& add = *pendingAdditions[i];
                        row[QStringLiteral("rightLine")] = add.raw->newLineNumber.has_value()
                                                               ? QString::number(*add.raw->newLineNumber)
                                                               : QString();
                        row[QStringLiteral("rightContent")] = add.content;
                        row[QStringLiteral("rightRichContent")] = add.richContent;
                        row[QStringLiteral("rightKind")] = QStringLiteral("addition");
                        row[QStringLiteral("rightBgColor")] = QStringLiteral("#143220");
                        row[QStringLiteral("rightFgColor")] = QStringLiteral("#7EE787");
                    }
                    else
                    {
                        row[QStringLiteral("rightLine")] = QString();
                        row[QStringLiteral("rightContent")] = QString();
                        row[QStringLiteral("rightRichContent")] = QString();
                        row[QStringLiteral("rightKind")] = QStringLiteral("empty");
                        row[QStringLiteral("rightBgColor")] = QStringLiteral("#0D1117");
                        row[QStringLiteral("rightFgColor")] = QStringLiteral("#484F58");
                    }
                    m_splitLines.append(row);
                }
                pendingDeletions.clear();
                pendingAdditions.clear();
            };

            for (const auto& prep : preparedLines)
            {
                const auto& line = *prep.raw;
                if (line.kind == gf::git::bo::DiffLineKind::Deletion)
                {
                    pendingDeletions.push_back(&prep);
                }
                else if (line.kind == gf::git::bo::DiffLineKind::Addition)
                {
                    pendingAdditions.push_back(&prep);
                }
                else if (line.kind == gf::git::bo::DiffLineKind::Context)
                {
                    flushPending();
                    QVariantMap row;
                    row[QStringLiteral("isHunkHeader")] = false;
                    row[QStringLiteral("leftLine")] = line.oldLineNumber.has_value()
                                                          ? QString::number(*line.oldLineNumber)
                                                          : QString();
                    row[QStringLiteral("leftContent")] = prep.content;
                    row[QStringLiteral("leftRichContent")] = prep.richContent;
                    row[QStringLiteral("leftKind")] = QStringLiteral("context");
                    row[QStringLiteral("leftBgColor")] = QStringLiteral("transparent");
                    row[QStringLiteral("leftFgColor")] = QStringLiteral("#E6EDF3");

                    row[QStringLiteral("rightLine")] = line.newLineNumber.has_value()
                                                           ? QString::number(*line.newLineNumber)
                                                           : QString();
                    row[QStringLiteral("rightContent")] = prep.content;
                    row[QStringLiteral("rightRichContent")] = prep.richContent;
                    row[QStringLiteral("rightKind")] = QStringLiteral("context");
                    row[QStringLiteral("rightBgColor")] = QStringLiteral("transparent");
                    row[QStringLiteral("rightFgColor")] = QStringLiteral("#E6EDF3");
                    m_splitLines.append(row);
                }
            }
            flushPending();
        }

        if (truncated)
        {
            QVariantMap noticeRow;
            noticeRow[QStringLiteral("isHunkHeader")] = true;
            noticeRow[QStringLiteral("hunkHeader")] = QStringLiteral(
                "... Diff troncato a 5000 righe per preservare la reattività dell'interfaccia ...");
            m_splitLines.append(noticeRow);
        }

        emit diffLoaded();
        if (wasMarkdown != m_isMarkdown)
        {
            emit markdownRenderedChanged();
        }
    }

    bool DiffViewModel::loadDiff(const QString& repoPath, const QString& filePath, const QString& scope)
    {
        if (repoPath.isEmpty() || filePath.isEmpty())
        {
            clear();
            return false;
        }

        gf::git::bo::DiffScope diffScope = gf::git::bo::DiffScope::Unstaged;
        if (scope.compare(QStringLiteral("staged"), Qt::CaseInsensitive) == 0)
        {
            diffScope = gf::git::bo::DiffScope::Staged;
        }
        else if (scope.compare(QStringLiteral("untracked"), Qt::CaseInsensitive) == 0)
        {
            diffScope = gf::git::bo::DiffScope::Untracked;
        }

        std::filesystem::path rPath = gf::core::utf8ToPath(repoPath.toStdString());
        std::string fPath = filePath.toStdString();

        auto result = m_diffService.getFileDiff(rPath, fPath, diffScope, m_contextLines);
        if (!result)
        {
            clear();
            emit loadFailed(QString::fromStdString(result.error().message));
            return false;
        }

        if (m_hasLastDiff&&
            m_currentRepoPath == repoPath &&
            m_filePath == filePath &&
            m_currentRevision.isEmpty() &&
            m_lastScope.compare(scope, Qt::CaseInsensitive) == 0 &&
            m_lastContextLines == m_contextLines &&
            m_lastDiff == *result)
        {
            return true;
        }

        m_currentRepoPath = repoPath;
        m_currentRevision.clear();
        m_filePath = filePath;
        m_lastScope = scope;
        m_lastContextLines = m_contextLines;
        m_lastDiff = *result;
        m_hasLastDiff = true;

        const auto langInfo = gf::core::syntax::LanguageDetector::detect(
            result->displayPath().empty() ? fPath : result->displayPath());
        std::vector<gf::core::syntax::MarkdownLineState> mdSeedStates;
        bool hasMdSeeds = false;
        if (langInfo.language == gf::core::syntax::Language::Markdown && !result->isBinary)
        {
            if (diffScope == gf::git::bo::DiffScope::Staged)
            {
                const std::string primary = result->isDeletedFile ? "HEAD" : ":0";
                updateMarkdownStateForDiff(rPath,
                                           fPath,
                                           primary,
                                           "",
                                           "File eliminato nello stage — visualizzata la versione in HEAD.",
                                           *result,
                                           mdSeedStates,
                                           hasMdSeeds);
            }
            else
            {
                const std::string primary = result->isDeletedFile ? ":0" : "";
                const std::string fallback = result->isDeletedFile ? "HEAD" : "";
                updateMarkdownStateForDiff(rPath,
                                           fPath,
                                           primary,
                                           fallback,
                                           "File eliminato nella working tree — visualizzata l'ultima versione tracciata.",
                                           *result,
                                           mdSeedStates,
                                           hasMdSeeds);
            }
        }

        processFileDiff(*result, hasMdSeeds ? &mdSeedStates : nullptr);

        if (m_viewMode == QStringLiteral("blame"))
        {
            loadBlame(repoPath, QString(), filePath);
        }
        return true;
    }

    bool DiffViewModel::loadCommitFileDiff(const QString& repoPath, const QString& commitSha, const QString& filePath)
    {
        if (repoPath.isEmpty() || commitSha.isEmpty() || filePath.isEmpty())
        {
            clear();
            return false;
        }

        std::filesystem::path rPath = gf::core::utf8ToPath(repoPath.toStdString());
        std::string fPath = filePath.toStdString();
        std::string shaStr = commitSha.toStdString();

        auto result = m_diffService.getCommitFileDiff(rPath, shaStr, fPath, m_contextLines);
        if (!result)
        {
            clear();
            emit loadFailed(QString::fromStdString(result.error().message));
            return false;
        }

        if (m_hasLastDiff&&
            m_currentRepoPath == repoPath &&
            m_filePath == filePath &&
            m_currentRevision == commitSha &&
            m_lastContextLines == m_contextLines &&
            m_lastDiff == *result)
        {
            return true;
        }

        m_currentRepoPath = repoPath;
        m_currentRevision = commitSha;
        m_filePath = filePath;
        m_lastScope.clear();
        m_lastContextLines = m_contextLines;
        m_lastDiff = *result;
        m_hasLastDiff = true;

        const auto langInfo = gf::core::syntax::LanguageDetector::detect(
            result->displayPath().empty() ? fPath : result->displayPath());
        std::vector<gf::core::syntax::MarkdownLineState> mdSeedStates;
        bool hasMdSeeds = false;
        if (langInfo.language == gf::core::syntax::Language::Markdown && !result->isBinary)
        {
            const std::string primary = result->isDeletedFile ? (shaStr + "^") : shaStr;
            updateMarkdownStateForDiff(rPath,
                                       fPath,
                                       primary,
                                       "",
                                       "File eliminato in questo commit — visualizzata la versione del commit precedente.",
                                       *result,
                                       mdSeedStates,
                                       hasMdSeeds);
        }

        processFileDiff(*result, hasMdSeeds ? &mdSeedStates : nullptr);

        if (m_viewMode == QStringLiteral("blame"))
        {
            loadBlame(repoPath, commitSha, filePath);
        }
        return true;
    }

    bool DiffViewModel::loadBlame(const QString& repoPath, const QString& revision, const QString& filePath)
    {
        if (repoPath.isEmpty() || filePath.isEmpty())
        {
            m_blameLines.clear();
            emit blameLoaded();
            return false;
        }

        m_isBlameLoading = true;
        emit blameLoaded();

        std::filesystem::path rPath = gf::core::utf8ToPath(repoPath.toStdString());
        auto result = m_diffService.getFileBlame(rPath, revision.toStdString(), filePath.toStdString());
        m_isBlameLoading = false;

        if (!result)
        {
            m_blameLines.clear();
            emit blameLoaded();
            return false;
        }

        auto langInfo = gf::core::syntax::LanguageDetector::detect(filePath.toStdString());
        m_languageName = QString::fromStdString(langInfo.name);
        m_languageIcon = QString::fromStdString(langInfo.icon);

        const std::size_t count = std::min(result->size(), kMaxRenderedDiffLines);
        m_blameLines.clear();
        m_blameLines.reserve(static_cast<qsizetype>(count));
        m_maxLineLength = 0;
        gf::core::syntax::MarkdownLineState mdState{};

        for (std::size_t idx = 0; idx < count; ++idx)
        {
            const auto& bl = (*result)[idx];
            int visualLen = 0;
            for (char c : bl.content)
            {
                if (c == '\t')
                {
                    visualLen += 4 - (visualLen % 4);
                }
                else
                {
                    visualLen += 1;
                }
                if (visualLen >= kMaxVisualLineLength)
                {
                    visualLen = kMaxVisualLineLength;
                    break;
                }
            }
            if (visualLen > m_maxLineLength)
            {
                m_maxLineLength = visualLen;
            }
            QVariantMap map;
            map[QStringLiteral("commitSha")] = QString::fromStdString(bl.commitSha);
            map[QStringLiteral("shortSha")] = QString::fromStdString(bl.shortSha);
            map[QStringLiteral("author")] = QString::fromStdString(bl.author);
            map[QStringLiteral("authorEmail")] = QString::fromStdString(bl.authorEmail);
            map[QStringLiteral("date")] = QString::fromStdString(bl.date);
            map[QStringLiteral("lineNumber")] = bl.lineNumber;
            map[QStringLiteral("content")] = QString::fromStdString(bl.content);
            map[QStringLiteral("richContent")] = QString::fromStdString(
                gf::core::syntax::SyntaxHighlighter::highlightLine(bl.content, langInfo.language, mdState));
            map[QStringLiteral("avatarUrl")] = computeBlameAvatar(QString::fromStdString(bl.author),
                                                                  QString::fromStdString(bl.authorEmail));
            map[QStringLiteral("initials")] = computeBlameInitials(QString::fromStdString(bl.author));
            map[QStringLiteral("color")] = computeBlameColor(QString::fromStdString(bl.author));
            m_blameLines.append(map);
        }

        emit blameLoaded();
        return true;
    }

    void DiffViewModel::clear()
    {
        const bool hadMarkdown = m_isMarkdown;
        m_filePath.clear();
        m_oldPath.clear();
        m_languageName.clear();
        m_languageIcon.clear();
        m_currentRepoPath.clear();
        m_currentRevision.clear();
        m_lastScope.clear();
        m_hasLastDiff = false;
        m_lastDiff = {};
        m_additionsCount = 0;
        m_deletionsCount = 0;
        m_isBinary = false;
        m_isNewFile = false;
        m_isDeletedFile = false;
        m_isRenamed = false;
        m_isEmpty = true;
        m_isMarkdown = false;
        m_maxLineLength = 0;
        m_isBlameLoading = false;

        m_linesModel.clear();
        m_splitLines.clear();
        m_hunks.clear();
        m_blameLines.clear();
        m_markdownBlocks.clear();
        m_markdownToc.clear();
        m_markdownSource.clear();
        m_markdownNotice.clear();

        emit markdownDocumentChanged();
        if (hadMarkdown)
        {
            emit markdownRenderedChanged();
        }
        emit diffLoaded();
    }
} // namespace gf::desktop
