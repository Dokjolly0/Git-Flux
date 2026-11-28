#pragma once

#include "core/modules/git/bo/diff_models.hpp"

#include <QByteArray>
#include <QImage>
#include <QString>
#include <QVariantList>
#include <optional>
#include <string>

namespace gf::desktop
{
    struct DiffFileInspection
    {
        bool valid = false;

        // Category: "image", "pdf", "archive", "font", "audio", "video",
        //           "database", "executable", "document", "binary"
        QString fileKind;
        QString formatBadge;        // e.g. "PNG", "SVG", "PDF", "ZIP", "TTF", "WAV", "SQLITE", "ELF", "BINARY"
        QString formatDescription;  // Human-readable Italian description

        bool isImage = false;
        bool isSvg = false;
        bool hasVisualPreview = false;

        bool hasOldBlob = false;
        bool hasNewBlob = false;
        qint64 oldFileSize = 0;
        qint64 newFileSize = 0;
        bool oldTruncated = false;
        bool newTruncated = false;
        QString oldSha256;
        QString newSha256;

        QImage oldImage;
        QImage newImage;
        QImage diffImage;
        int oldImageWidth = 0;
        int oldImageHeight = 0;
        int oldImageDepth = 0;
        bool oldImageHasAlpha = false;
        int newImageWidth = 0;
        int newImageHeight = 0;
        int newImageDepth = 0;
        bool newImageHasAlpha = false;
        double imageDiffPercent = 0.0;

        // Raw font data to be registered with QFontDatabase on the GUI thread when fileKind == "font"
        QByteArray oldFontBytes;
        QByteArray newFontBytes;

        // Structured metadata rows: [{ label, oldValue, newValue, changed }]
        QVariantList metadataRows;

        // Archive diff entries (ZIP / JAR / APK / DOCX / XLSX / PPTX / EPUB / WHL):
        // [{ path, status, oldSize, newSize, oldSizeText, newSizeText }]
        QVariantList archiveEntries;
        int archiveAddedCount = 0;
        int archiveRemovedCount = 0;
        int archiveModifiedCount = 0;
        int archiveUnchangedCount = 0;

        // Hex dump / diff rows (up to 4 KiB preview):
        // [{ offset, oldHex, newHex, oldRichHex, newRichHex, oldAscii, newAscii, hasDiff }]
        QVariantList hexRows;
        bool hexTruncated = false;
        int hexChangedRows = 0;

        QString notice;
    };

    class DiffFileInspector final
    {
    public:
        static constexpr std::size_t kMaxBlobBytes = 16 * 1024 * 1024;
        static constexpr std::size_t kHexPreviewBytes = 4096;

        [[nodiscard]] static bool isSvgPath(const std::string& filePath);
        [[nodiscard]] static bool canDecodeSvg();
        [[nodiscard]] static QString formatByteSize(qint64 bytes);
        [[nodiscard]] static QString formatSizeDelta(qint64 oldSize, qint64 newSize, bool hasOld, bool hasNew);

        /// Inspects raw before/after blobs on a worker thread, computing images, pixel diffs,
        /// archive diffs, format metadata, SHA-256 digests, and hex diff rows.
        [[nodiscard]] static DiffFileInspection inspect(
            const std::string& filePath,
            const std::optional<gf::git::bo::FileBlob>& oldBlob,
            const std::optional<gf::git::bo::FileBlob>& newBlob);
    };
} // namespace gf::desktop
