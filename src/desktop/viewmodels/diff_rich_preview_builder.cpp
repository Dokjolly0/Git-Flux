#include "desktop/viewmodels/diff_viewmodel_internal.hpp"
#include "desktop/services/diff_image_provider.hpp"

#include "core/shared/utf8_path.hpp"
#include "core/modules/git/bl/git_diff_service.hpp"
#include "core/modules/syntax/markdown_parser.hpp"
#include "core/modules/syntax/syntax_highlighter.hpp"

#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFontDatabase>
#include <QStandardPaths>
#include <QStringList>
#include <QUrl>
#include <QVariantMap>
#include <filesystem>

namespace gf::desktop
{
    namespace
    {
        [[nodiscard]] QString blockKindToString(const gf::core::syntax::MarkdownBlockKind kind)
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

        [[nodiscard]] QString alertTypeToString(const gf::core::syntax::MarkdownAlertType alert)
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

        [[nodiscard]] QString changeStatusToString(const gf::core::syntax::MarkdownChangeStatus status)
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

        [[nodiscard]] std::string reconstructMarkdownFallbackFromDiff(const gf::git::bo::FileDiff& diff,
                                                                      bool& hasNewSideLines)
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
    } // namespace

    void DiffViewModel::releaseTemporaryFonts()
    {
        if (m_oldFontId >= 0)
        {
            QFontDatabase::removeApplicationFont(m_oldFontId);
            m_oldFontId = -1;
        }
        if (m_newFontId >= 0)
        {
            QFontDatabase::removeApplicationFont(m_newFontId);
            m_newFontId = -1;
        }
        m_oldFontFamily.clear();
        m_newFontFamily.clear();
    }

    void DiffViewModel::clearRichPreviewState()
    {
        releaseTemporaryFonts();
        DiffImageStore::instance().clear(m_instanceId);
        m_isSvgPreviewable = false;
        m_fileKind.clear();
        m_formatBadge.clear();
        m_formatDescription.clear();
        m_isImage = false;
        m_hasVisualPreview = false;
        m_oldImageUrl.clear();
        m_newImageUrl.clear();
        m_diffImageUrl.clear();
        m_oldImageWidth = 0;
        m_oldImageHeight = 0;
        m_newImageWidth = 0;
        m_newImageHeight = 0;
        m_imageDiffPercent = 0.0;
        m_hasOldBlob = false;
        m_hasNewBlob = false;
        m_oldFileSize = 0;
        m_newFileSize = 0;
        m_oldFileSizeText.clear();
        m_newFileSizeText.clear();
        m_fileSizeDeltaText.clear();
        m_metadataRows.clear();
        m_archiveEntries.clear();
        m_archiveAddedCount = 0;
        m_archiveRemovedCount = 0;
        m_archiveModifiedCount = 0;
        m_archiveUnchangedCount = 0;
        m_hexRows.clear();
        m_hexTruncated = false;
        m_hexChangedRows = 0;
        m_previewNotice.clear();
        m_canRevealInFileManager = false;
    }

    void DiffViewModel::applyPreparedRichPreview(PreparedDiff& prepared)
    {
        clearRichPreviewState();
        if (prepared.inspection && prepared.inspection->valid)
        {
            auto& insp = *prepared.inspection;
            m_fileKind = std::move(insp.fileKind);
            m_formatBadge = std::move(insp.formatBadge);
            m_formatDescription = std::move(insp.formatDescription);
            m_isImage = insp.isImage;
            m_hasVisualPreview = insp.hasVisualPreview;
            m_isSvgPreviewable = insp.isSvg && insp.hasVisualPreview && !m_isBinary;

            m_hasOldBlob = insp.hasOldBlob;
            m_hasNewBlob = insp.hasNewBlob;
            m_oldFileSize = insp.oldFileSize;
            m_newFileSize = insp.newFileSize;
            m_oldFileSizeText = m_hasOldBlob ? DiffFileInspector::formatByteSize(m_oldFileSize) : QString();
            m_newFileSizeText = m_hasNewBlob ? DiffFileInspector::formatByteSize(m_newFileSize) : QString();
            m_fileSizeDeltaText = DiffFileInspector::formatSizeDelta(
                m_oldFileSize, m_newFileSize, m_hasOldBlob, m_hasNewBlob);

            m_oldImageWidth = insp.oldImageWidth;
            m_oldImageHeight = insp.oldImageHeight;
            m_newImageWidth = insp.newImageWidth;
            m_newImageHeight = insp.newImageHeight;
            m_imageDiffPercent = insp.imageDiffPercent;

            if (m_hasVisualPreview)
            {
                const bool hasOldImg = !insp.oldImage.isNull();
                const bool hasNewImg = !insp.newImage.isNull();
                const bool hasDiffImg = !insp.diffImage.isNull();
                const std::uint64_t gen = DiffImageStore::instance().setImages(
                    m_instanceId, std::move(insp.oldImage), std::move(insp.newImage), std::move(insp.diffImage));
                if (hasOldImg) m_oldImageUrl = DiffImageStore::buildUrl(m_instanceId, gen, QStringLiteral("old"));
                if (hasNewImg) m_newImageUrl = DiffImageStore::buildUrl(m_instanceId, gen, QStringLiteral("new"));
                if (hasDiffImg) m_diffImageUrl = DiffImageStore::buildUrl(m_instanceId, gen, QStringLiteral("diff"));
            }

            auto registerTempFont = [](const QByteArray& bytes, int& outId, QString& outFamily)
            {
                if (bytes.isEmpty()) return;
                outId = QFontDatabase::addApplicationFontFromData(bytes);
                if (outId >= 0)
                {
                    const auto families = QFontDatabase::applicationFontFamilies(outId);
                    if (!families.isEmpty()) outFamily = families.first();
                }
            };
            registerTempFont(insp.oldFontBytes, m_oldFontId, m_oldFontFamily);
            registerTempFont(insp.newFontBytes, m_newFontId, m_newFontFamily);

            m_metadataRows = std::move(insp.metadataRows);
            m_archiveEntries = std::move(insp.archiveEntries);
            m_archiveAddedCount = insp.archiveAddedCount;
            m_archiveRemovedCount = insp.archiveRemovedCount;
            m_archiveModifiedCount = insp.archiveModifiedCount;
            m_archiveUnchangedCount = insp.archiveUnchangedCount;
            m_hexRows = std::move(insp.hexRows);
            m_hexTruncated = insp.hexTruncated;
            m_hexChangedRows = insp.hexChangedRows;
            m_previewNotice = std::move(insp.notice);
        }
        else if (m_isBinary)
        {
            m_fileKind = QStringLiteral("binary");
            m_formatBadge = QStringLiteral("BINARY");
            m_formatDescription = QStringLiteral("File binario");
        }
    }

    void DiffViewModel::openPreviewExternally(const QString& side)
    {
        if (m_currentRepoPath.isEmpty() || m_filePath.isEmpty()) return;

        const bool wantOld = (side.compare(QStringLiteral("old"), Qt::CaseInsensitive) == 0);
        const std::string targetPath = wantOld
            ? (m_oldPath.isEmpty() ? m_filePath.toStdString() : m_oldPath.toStdString())
            : m_filePath.toStdString();
        const auto repoFs = gf::core::utf8ToPath(m_currentRepoPath.toStdString());

        if (!wantOld && m_currentRevision.isEmpty() &&
            m_lastScope.compare(QStringLiteral("staged"), Qt::CaseInsensitive) != 0)
        {
            const auto fullPath = repoFs / gf::core::utf8ToPath(targetPath);
            std::error_code ec;
            if (std::filesystem::exists(fullPath, ec) && std::filesystem::is_regular_file(fullPath, ec))
            {
                const std::string utf8 = gf::core::pathToUtf8(fullPath);
                QDesktopServices::openUrl(QUrl::fromLocalFile(
                    QString::fromUtf8(utf8.data(), static_cast<qsizetype>(utf8.size()))));
                return;
            }
        }

        std::string primaryRev;
        std::string fallbackRev;
        if (!m_currentRevision.isEmpty())
        {
            primaryRev = wantOld ? (m_currentRevision.toStdString() + "^") : m_currentRevision.toStdString();
        }
        else if (m_lastScope.compare(QStringLiteral("staged"), Qt::CaseInsensitive) == 0)
        {
            primaryRev = wantOld ? "HEAD" : ":0";
        }
        else
        {
            primaryRev = wantOld ? ":0" : "WORKTREE";
            if (wantOld) fallbackRev = "HEAD";
        }

        auto& service = m_diffService;
        const QString tag = wantOld ? QStringLiteral("before") : QStringLiteral("after");
        m_readExecutor->run(
            this,
            [&service, repoFs, primaryRev, fallbackRev, targetPath, tag]() -> QString
            {
                constexpr std::size_t kMaxExternalOpenBytes = 64 * 1024 * 1024;
                auto res = service.getFileBlob(repoFs, primaryRev, targetPath, kMaxExternalOpenBytes);
                if (!res && !fallbackRev.empty())
                {
                    res = service.getFileBlob(repoFs, fallbackRev, targetPath, kMaxExternalOpenBytes);
                }
                if (!res || res->truncated) return {};

                const QString tempBase = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
                QDir dir(tempBase + QStringLiteral("/git-flux-preview"));
                if (!dir.exists()) (void)dir.mkpath(QStringLiteral("."));

                const QFileInfo fi(QString::fromUtf8(targetPath.data(), static_cast<qsizetype>(targetPath.size())));
                const QString safeName = fi.fileName().isEmpty() ? QStringLiteral("preview.bin") : fi.fileName();
                const QString outPath = dir.filePath(tag + QLatin1Char('_') + safeName);
                QFile f(outPath);
                if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) return {};
                f.write(res->bytes.data(), static_cast<qint64>(res->bytes.size()));
                f.close();
                return outPath;
            },
            [](QString tempFilePath)
            {
                if (!tempFilePath.isEmpty())
                {
                    QDesktopServices::openUrl(QUrl::fromLocalFile(tempFilePath));
                }
            });
    }

    void DiffViewModel::revealInFileManager() const
    {
        if (m_currentRepoPath.isEmpty() || m_filePath.isEmpty()) return;
        const auto fullPath = gf::core::utf8ToPath(m_currentRepoPath.toStdString()) /
                              gf::core::utf8ToPath(m_filePath.toStdString());
        const auto parentDir = fullPath.parent_path();
        std::error_code ec;
        if (std::filesystem::exists(parentDir, ec))
        {
            const std::string utf8 = gf::core::pathToUtf8(parentDir);
            QDesktopServices::openUrl(QUrl::fromLocalFile(
                QString::fromUtf8(utf8.data(), static_cast<qsizetype>(utf8.size()))));
        }
    }

    void DiffViewModel::buildMarkdownDocument(const std::string& content,
                                              const gf::git::bo::FileDiff& diff,
                                              PreparedDiff& out)
    {
        out.markdownSource = QString::fromStdString(content);
        auto mdDoc = gf::core::syntax::MarkdownParser::parseWithDiff(content, &diff);

        out.markdownBlocks.clear();
        out.markdownBlocks.reserve(static_cast<qsizetype>(mdDoc.blocks.size()));
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

            out.markdownBlocks.append(map);
        }

        out.markdownToc.clear();
        out.markdownToc.reserve(static_cast<qsizetype>(mdDoc.toc.size()));
        for (const auto& t : mdDoc.toc)
        {
            QVariantMap tm;
            tm[QStringLiteral("level")] = t.level;
            tm[QStringLiteral("title")] = QString::fromStdString(t.title);
            tm[QStringLiteral("anchor")] = QString::fromStdString(t.anchor);
            tm[QStringLiteral("blockIndex")] = t.blockIndex;
            tm[QStringLiteral("startLine")] = t.startLine;
            out.markdownToc.append(tm);
        }
    }

    void DiffViewModel::prepareMarkdown(
        const std::expected<std::string, gf::git::ports::GitOpsError>& contentRes,
        const std::string& deletedFallbackNotice,
        const gf::git::bo::FileDiff& diff,
        PreparedDiff& out,
        std::vector<gf::core::syntax::MarkdownLineState>& outSeedStates)
    {
        outSeedStates.clear();
        out.hasMarkdownDocument = true;

        if (contentRes)
        {
            std::string content = *contentRes;
            if (content.empty() && !diff.isDeletedFile && !diff.hunks.empty())
            {
                bool hasNewSideLines = false;
                std::string reconstructed = reconstructMarkdownFallbackFromDiff(diff, hasNewSideLines);
                if (hasNewSideLines) content = std::move(reconstructed);
            }

            if (diff.isDeletedFile && !deletedFallbackNotice.empty())
            {
                out.markdownNotice = QString::fromStdString(deletedFallbackNotice);
            }

            if (!diff.isDeletedFile)
            {
                outSeedStates = gf::core::syntax::SyntaxHighlighter::computeMarkdownLineStates(content);
            }
            buildMarkdownDocument(content, diff, out);
            return;
        }

        bool hasNewSideLines = false;
        if (std::string fallback = reconstructMarkdownFallbackFromDiff(diff, hasNewSideLines); !fallback.empty())
        {
            buildMarkdownDocument(fallback, diff, out);
        }
        out.markdownNotice = QStringLiteral("Impossibile caricare il file completo (%1) — anteprima parziale.")
            .arg(QString::fromStdString(contentRes.error().message));
    }
} // namespace gf::desktop
