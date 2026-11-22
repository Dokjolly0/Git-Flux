#pragma once

#include <QObject>
#include <QString>
#include <QVariantList>
#include <expected>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "core/modules/syntax/syntax_highlighter.hpp"
#include "core/modules/git/ports/repository_git_ops.hpp"
#include "desktop/services/diff_file_inspector.hpp"
#include "desktop/services/serial_git_executor.hpp"
#include "desktop/viewmodels/diff_line_list_model.hpp"

namespace gf::git::bl
{
    class GitDiffService;
}

namespace gf::desktop
{
    class DiffViewModel final : public QObject
    {
        Q_OBJECT

        Q_PROPERTY(DiffLineListModel* linesModel READ linesModel CONSTANT)
        Q_PROPERTY(QString filePath READ filePath NOTIFY diffLoaded)
        Q_PROPERTY(QString oldPath READ oldPath NOTIFY diffLoaded)
        Q_PROPERTY(QString languageName READ languageName NOTIFY diffLoaded)
        Q_PROPERTY(QString languageIcon READ languageIcon NOTIFY diffLoaded)
        Q_PROPERTY(int additionsCount READ additionsCount NOTIFY diffLoaded)
        Q_PROPERTY(int deletionsCount READ deletionsCount NOTIFY diffLoaded)
        Q_PROPERTY(bool isBinary READ isBinary NOTIFY diffLoaded)
        Q_PROPERTY(bool isNewFile READ isNewFile NOTIFY diffLoaded)
        Q_PROPERTY(bool isDeletedFile READ isDeletedFile NOTIFY diffLoaded)
        Q_PROPERTY(bool isRenamed READ isRenamed NOTIFY diffLoaded)
        Q_PROPERTY(bool isEmpty READ isEmpty NOTIFY diffLoaded)
        Q_PROPERTY(QString currentRevision READ currentRevision NOTIFY diffLoaded)
        Q_PROPERTY(int maxLineLength READ maxLineLength NOTIFY diffLoaded)
        Q_PROPERTY(QString viewMode READ viewMode WRITE setViewMode NOTIFY viewModeChanged)
        Q_PROPERTY(QVariantList splitLines READ splitLines NOTIFY diffLoaded)
        Q_PROPERTY(QVariantList hunks READ hunks NOTIFY diffLoaded)
        Q_PROPERTY(QVariantList splitOverviewMarkers READ splitOverviewMarkers NOTIFY diffLoaded)
        Q_PROPERTY(QVariantList inlineOverviewMarkers READ inlineOverviewMarkers NOTIFY diffLoaded)
        Q_PROPERTY(QVariantList hunkOverviewMarkers READ hunkOverviewMarkers NOTIFY diffLoaded)
        Q_PROPERTY(QVariantList blameLines READ blameLines NOTIFY blameLoaded)
        Q_PROPERTY(bool isBlameLoading READ isBlameLoading NOTIFY blameLoaded)
        // A diff requested by loadDiff / loadCommitFileDiff is being read (the previous one stays
        // visible meanwhile).
        Q_PROPERTY(bool isLoading READ isLoading NOTIFY isLoadingChanged)

        // Diff context control (`-U<n>`, default 10, -1 for full file)
        Q_PROPERTY(int contextLines READ contextLines WRITE setContextLines NOTIFY contextLinesChanged)
        Q_PROPERTY(bool isFullContext READ isFullContext NOTIFY contextLinesChanged)

        // Markdown preview & Rich Diff support
        Q_PROPERTY(bool isMarkdown READ isMarkdown NOTIFY diffLoaded)
        Q_PROPERTY(
            bool isMarkdownRendered READ isMarkdownRendered WRITE setMarkdownRendered NOTIFY markdownRenderedChanged)
        Q_PROPERTY(QVariantList markdownBlocks READ markdownBlocks NOTIFY markdownDocumentChanged)
        Q_PROPERTY(QVariantList markdownToc READ markdownToc NOTIFY markdownDocumentChanged)
        Q_PROPERTY(QString markdownSource READ markdownSource NOTIFY markdownDocumentChanged)
        Q_PROPERTY(QString markdownNotice READ markdownNotice NOTIFY markdownDocumentChanged)
        Q_PROPERTY(
            bool
 showMarkdownRichDiff READ showMarkdownRichDiff WRITE setShowMarkdownRichDiff NOTIFY showMarkdownRichDiffChanged)

        // Rich Binary / Image / Media / Archive / Font / Hex Preview support
        Q_PROPERTY(bool hasRichPreview READ hasRichPreview NOTIFY diffLoaded)
        Q_PROPERTY(bool isSvgPreviewable READ isSvgPreviewable NOTIFY diffLoaded)
        Q_PROPERTY(bool isRichPreviewRendered READ isRichPreviewRendered WRITE setRichPreviewRendered NOTIFY richPreviewRenderedChanged)
        Q_PROPERTY(QString fileKind READ fileKind NOTIFY diffLoaded)
        Q_PROPERTY(QString formatBadge READ formatBadge NOTIFY diffLoaded)
        Q_PROPERTY(QString formatDescription READ formatDescription NOTIFY diffLoaded)
        Q_PROPERTY(bool isImage READ isImage NOTIFY diffLoaded)
        Q_PROPERTY(bool hasVisualPreview READ hasVisualPreview NOTIFY diffLoaded)
        Q_PROPERTY(QString oldImageUrl READ oldImageUrl NOTIFY diffLoaded)
        Q_PROPERTY(QString newImageUrl READ newImageUrl NOTIFY diffLoaded)
        Q_PROPERTY(QString diffImageUrl READ diffImageUrl NOTIFY diffLoaded)
        Q_PROPERTY(int oldImageWidth READ oldImageWidth NOTIFY diffLoaded)
        Q_PROPERTY(int oldImageHeight READ oldImageHeight NOTIFY diffLoaded)
        Q_PROPERTY(int newImageWidth READ newImageWidth NOTIFY diffLoaded)
        Q_PROPERTY(int newImageHeight READ newImageHeight NOTIFY diffLoaded)
        Q_PROPERTY(double imageDiffPercent READ imageDiffPercent NOTIFY diffLoaded)
        Q_PROPERTY(bool hasOldBlob READ hasOldBlob NOTIFY diffLoaded)
        Q_PROPERTY(bool hasNewBlob READ hasNewBlob NOTIFY diffLoaded)
        Q_PROPERTY(qint64 oldFileSize READ oldFileSize NOTIFY diffLoaded)
        Q_PROPERTY(qint64 newFileSize READ newFileSize NOTIFY diffLoaded)
        Q_PROPERTY(QString oldFileSizeText READ oldFileSizeText NOTIFY diffLoaded)
        Q_PROPERTY(QString newFileSizeText READ newFileSizeText NOTIFY diffLoaded)
        Q_PROPERTY(QString fileSizeDeltaText READ fileSizeDeltaText NOTIFY diffLoaded)
        Q_PROPERTY(QString oldSha256 READ oldSha256 NOTIFY diffLoaded)
        Q_PROPERTY(QString newSha256 READ newSha256 NOTIFY diffLoaded)
        Q_PROPERTY(QString oldFontFamily READ oldFontFamily NOTIFY diffLoaded)
        Q_PROPERTY(QString newFontFamily READ newFontFamily NOTIFY diffLoaded)
        Q_PROPERTY(QVariantList metadataRows READ metadataRows NOTIFY diffLoaded)
        Q_PROPERTY(QVariantList archiveEntries READ archiveEntries NOTIFY diffLoaded)
        Q_PROPERTY(int archiveAddedCount READ archiveAddedCount NOTIFY diffLoaded)
        Q_PROPERTY(int archiveRemovedCount READ archiveRemovedCount NOTIFY diffLoaded)
        Q_PROPERTY(int archiveModifiedCount READ archiveModifiedCount NOTIFY diffLoaded)
        Q_PROPERTY(int archiveUnchangedCount READ archiveUnchangedCount NOTIFY diffLoaded)
        Q_PROPERTY(QVariantList hexRows READ hexRows NOTIFY diffLoaded)
        Q_PROPERTY(bool hexTruncated READ hexTruncated NOTIFY diffLoaded)
        Q_PROPERTY(int hexChangedRows READ hexChangedRows NOTIFY diffLoaded)
        Q_PROPERTY(QString previewNotice READ previewNotice NOTIFY diffLoaded)
        Q_PROPERTY(bool canRevealInFileManager READ canRevealInFileManager NOTIFY diffLoaded)

    public:
        explicit DiffViewModel(gf::git::bl::GitDiffService& diffService, QObject* parent = nullptr);
        ~DiffViewModel() override;

        [[nodiscard]] DiffLineListModel* linesModel() noexcept { return &m_linesModel; }
        [[nodiscard]] QString filePath() const noexcept { return m_filePath; }
        [[nodiscard]] QString oldPath() const noexcept { return m_oldPath; }
        [[nodiscard]] QString languageName() const noexcept { return m_languageName; }
        [[nodiscard]] QString languageIcon() const noexcept { return m_languageIcon; }
        [[nodiscard]] int additionsCount() const noexcept { return m_additionsCount; }
        [[nodiscard]] int deletionsCount() const noexcept { return m_deletionsCount; }
        [[nodiscard]] bool isBinary() const noexcept { return m_isBinary; }
        [[nodiscard]] bool isNewFile() const noexcept { return m_isNewFile; }
        [[nodiscard]] bool isDeletedFile() const noexcept { return m_isDeletedFile; }
        [[nodiscard]] bool isRenamed() const noexcept { return m_isRenamed; }
        [[nodiscard]] bool isEmpty() const noexcept { return m_isEmpty; }
        [[nodiscard]] QString currentRevision() const noexcept { return m_currentRevision; }
        [[nodiscard]] int maxLineLength() const noexcept { return m_maxLineLength; }
        [[nodiscard]] QString viewMode() const noexcept { return m_viewMode; }
        [[nodiscard]] QVariantList splitLines() const noexcept { return m_splitLines; }
        [[nodiscard]] QVariantList hunks() const noexcept { return m_hunks; }
        [[nodiscard]] QVariantList splitOverviewMarkers() const noexcept { return m_splitOverviewMarkers; }
        [[nodiscard]] QVariantList inlineOverviewMarkers() const noexcept { return m_inlineOverviewMarkers; }
        [[nodiscard]] QVariantList hunkOverviewMarkers() const noexcept { return m_hunkOverviewMarkers; }
        [[nodiscard]] QVariantList blameLines() const noexcept { return m_blameLines; }
        [[nodiscard]] bool isBlameLoading() const noexcept { return m_isBlameLoading; }
        [[nodiscard]] bool isLoading() const noexcept { return m_isLoading; }

        [[nodiscard]] int contextLines() const noexcept { return m_contextLines; }
        [[nodiscard]] bool isFullContext() const noexcept { return m_contextLines < 0; }
        [[nodiscard]] bool isMarkdown() const noexcept { return m_isMarkdown; }
        [[nodiscard]] bool isMarkdownRendered() const noexcept { return m_isMarkdown && m_preferMarkdownRendered; }
        [[nodiscard]] QVariantList markdownBlocks() const noexcept { return m_markdownBlocks; }
        [[nodiscard]] QVariantList markdownToc() const noexcept { return m_markdownToc; }
        [[nodiscard]] QString markdownSource() const noexcept { return m_markdownSource; }
        [[nodiscard]] QString markdownNotice() const noexcept { return m_markdownNotice; }
        [[nodiscard]] bool showMarkdownRichDiff() const noexcept { return m_showMarkdownRichDiff; }

        [[nodiscard]] bool hasRichPreview() const noexcept { return m_hasRichPreview; }
        [[nodiscard]] bool isSvgPreviewable() const noexcept { return m_isSvgPreviewable; }
        [[nodiscard]] bool isRichPreviewRendered() const noexcept
        {
            if (m_isBinary) return true;
            return m_isSvgPreviewable && m_preferRichPreview;
        }
        [[nodiscard]] QString fileKind() const noexcept { return m_fileKind; }
        [[nodiscard]] QString formatBadge() const noexcept { return m_formatBadge; }
        [[nodiscard]] QString formatDescription() const noexcept { return m_formatDescription; }
        [[nodiscard]] bool isImage() const noexcept { return m_isImage; }
        [[nodiscard]] bool hasVisualPreview() const noexcept { return m_hasVisualPreview; }
        [[nodiscard]] QString oldImageUrl() const noexcept { return m_oldImageUrl; }
        [[nodiscard]] QString newImageUrl() const noexcept { return m_newImageUrl; }
        [[nodiscard]] QString diffImageUrl() const noexcept { return m_diffImageUrl; }
        [[nodiscard]] int oldImageWidth() const noexcept { return m_oldImageWidth; }
        [[nodiscard]] int oldImageHeight() const noexcept { return m_oldImageHeight; }
        [[nodiscard]] int newImageWidth() const noexcept { return m_newImageWidth; }
        [[nodiscard]] int newImageHeight() const noexcept { return m_newImageHeight; }
        [[nodiscard]] double imageDiffPercent() const noexcept { return m_imageDiffPercent; }
        [[nodiscard]] bool hasOldBlob() const noexcept { return m_hasOldBlob; }
        [[nodiscard]] bool hasNewBlob() const noexcept { return m_hasNewBlob; }
        [[nodiscard]] qint64 oldFileSize() const noexcept { return m_oldFileSize; }
        [[nodiscard]] qint64 newFileSize() const noexcept { return m_newFileSize; }
        [[nodiscard]] QString oldFileSizeText() const noexcept { return m_oldFileSizeText; }
        [[nodiscard]] QString newFileSizeText() const noexcept { return m_newFileSizeText; }
        [[nodiscard]] QString fileSizeDeltaText() const noexcept { return m_fileSizeDeltaText; }
        [[nodiscard]] QString oldSha256() const noexcept { return m_oldSha256; }
        [[nodiscard]] QString newSha256() const noexcept { return m_newSha256; }
        [[nodiscard]] QString oldFontFamily() const noexcept { return m_oldFontFamily; }
        [[nodiscard]] QString newFontFamily() const noexcept { return m_newFontFamily; }
        [[nodiscard]] QVariantList metadataRows() const noexcept { return m_metadataRows; }
        [[nodiscard]] QVariantList archiveEntries() const noexcept { return m_archiveEntries; }
        [[nodiscard]] int archiveAddedCount() const noexcept { return m_archiveAddedCount; }
        [[nodiscard]] int archiveRemovedCount() const noexcept { return m_archiveRemovedCount; }
        [[nodiscard]] int archiveModifiedCount() const noexcept { return m_archiveModifiedCount; }
        [[nodiscard]] int archiveUnchangedCount() const noexcept { return m_archiveUnchangedCount; }
        [[nodiscard]] QVariantList hexRows() const noexcept { return m_hexRows; }
        [[nodiscard]] bool hexTruncated() const noexcept { return m_hexTruncated; }
        [[nodiscard]] int hexChangedRows() const noexcept { return m_hexChangedRows; }
        [[nodiscard]] QString previewNotice() const noexcept { return m_previewNotice; }
        [[nodiscard]] bool canRevealInFileManager() const noexcept { return m_canRevealInFileManager; }

    Q_INVOKABLE void setViewMode(const QString& mode);
    Q_INVOKABLE void setContextLines(int lines);
    Q_INVOKABLE void expandContext(int delta = 15);
    Q_INVOKABLE void toggleFullContext();
    Q_INVOKABLE void setMarkdownRendered(bool rendered);
    Q_INVOKABLE void toggleMarkdownRendered();
    Q_INVOKABLE void setShowMarkdownRichDiff(bool show);
    Q_INVOKABLE void setRichPreviewRendered(bool rendered);
    Q_INVOKABLE void toggleRichPreviewRendered();
    Q_INVOKABLE void openPreviewExternally(const QString& side = QStringLiteral("new"));
    Q_INVOKABLE void revealInFileManager() const;
    Q_INVOKABLE bool reloadCurrentDiff();
    Q_INVOKABLE void copyToClipboard(const QString& text) const;
    Q_INVOKABLE QString extractSelectedText(const QString& target,
                                            int startRow, int startCol,
                                            int endRow, int endCol,
                                            int hunkIndex = -1) const;

        // Asynchronous, "latest wins": git runs on a background thread, only the result of the
        // most recent request is applied (diffLoaded / blameLoaded), failures emit loadFailed.
        // They return whether a request was started.
        Q_INVOKABLE bool loadDiff(const QString& repoPath, const QString& filePath, const QString& scope);
        Q_INVOKABLE bool loadCommitFileDiff(const QString& repoPath, const QString& commitSha, const QString& filePath);
        Q_INVOKABLE bool loadBlame(const QString& repoPath, const QString& revision, const QString& filePath);
        Q_INVOKABLE void clear();

        /// Test helper: processes events until the requested diff and blame have been applied.
        bool waitForIdle(int timeoutMs = 10000);

        signals :

        void diffLoaded();
        void blameLoaded();
        void viewModeChanged();
        void contextLinesChanged();
        void markdownRenderedChanged();
        void markdownDocumentChanged();
        void showMarkdownRichDiffChanged();
        void richPreviewRenderedChanged();
        void isLoadingChanged();
        void loadFailed(const QString& errorMessage);

    private:
        struct PreparedDiff;
        struct DiffFetch;
        struct BlameFetch;

        // Worker thread (pure functions of their inputs): builds what the view shows for a fetched
        // diff, unless it equals fetch.comparedWith.
        static void prepareFetch(DiffFetch& fetch);
        static void prepareFileDiff(const gf::git::bo::FileDiff& diff,
                                    const std::vector<gf::core::syntax::MarkdownLineState>* newFileSeedStates,
                                    PreparedDiff& out);
        // `contentRes` is the full file content, read on the worker (see DiffFetch).
        static void prepareMarkdown(const std::expected<std::string, gf::git::ports::GitOpsError>& contentRes,
                                    const std::string& deletedFallbackNotice,
                                    const gf::git::bo::FileDiff& diff,
                                    PreparedDiff& out,
                                    std::vector<gf::core::syntax::MarkdownLineState>& outSeedStates);
        static void buildMarkdownDocument(const std::string& content, const gf::git::bo::FileDiff& diff,
                                          PreparedDiff& out);

        // GUI thread.
        // Applies a diff read on the worker: dedup, prepared rows, blame.
        void applyFetchedDiff(const QString& repoPath, const QString& filePath, const QString& revision,
                              const QString& scope, int contextLines, DiffFetch& fetch);
        void applyPreparedDiff(const gf::git::bo::FileDiff& diff, PreparedDiff& prepared);
        // The view currently shows the diff of this file/revision/scope/context.
        [[nodiscard]] bool isShowingDiff(const QString& repoPath, const QString& filePath, const QString& revision,
                                         const QString& scope, int contextLines) const;
        void setLoading(bool loading);
        void releaseTemporaryFonts();
        void clearRichPreviewState();

        gf::git::bl::GitDiffService& m_diffService;
        std::uint64_t m_instanceId = 0;
        DiffLineListModel m_linesModel;

        QString m_filePath;
        QString m_oldPath;
        QString m_languageName;
        QString m_languageIcon;
        QString m_currentRepoPath;
        QString m_currentRevision;
        QString m_lastScope;
        QString m_viewMode = QStringLiteral("split");
        int m_contextLines = 10;
        int m_lastNonFullContextLines = 10;
        int m_lastContextLines = 10;
        int m_additionsCount = 0;
        int m_deletionsCount = 0;
        bool m_isBinary = false;
        bool m_isNewFile = false;
        bool m_isDeletedFile = false;
        bool m_isRenamed = false;
        bool m_isEmpty = true;
        bool m_isMarkdown = false;
        bool m_preferMarkdownRendered = false;
        bool m_showMarkdownRichDiff = true;
        int m_maxLineLength = 0;
        bool m_isBlameLoading = false;
        // Diff shown (null when none); shared with the worker to skip unchanged reloads.
        std::shared_ptr<const gf::git::bo::FileDiff> m_lastDiff;

        QVariantList m_splitLines;
        QVariantList m_hunks;
        QVariantList m_splitOverviewMarkers;
        QVariantList m_inlineOverviewMarkers;
        QVariantList m_hunkOverviewMarkers;
        QVariantList m_blameLines;
        QVariantList m_markdownBlocks;
        QVariantList m_markdownToc;
        QString m_markdownSource;
        QString m_markdownNotice;

        // Rich preview state
        bool m_hasRichPreview = false;
        bool m_isSvgPreviewable = false;
        bool m_preferRichPreview = false;
        QString m_fileKind;
        QString m_formatBadge;
        QString m_formatDescription;
        bool m_isImage = false;
        bool m_hasVisualPreview = false;
        QString m_oldImageUrl;
        QString m_newImageUrl;
        QString m_diffImageUrl;
        int m_oldImageWidth = 0;
        int m_oldImageHeight = 0;
        int m_newImageWidth = 0;
        int m_newImageHeight = 0;
        double m_imageDiffPercent = 0.0;
        bool m_hasOldBlob = false;
        bool m_hasNewBlob = false;
        qint64 m_oldFileSize = 0;
        qint64 m_newFileSize = 0;
        QString m_oldFileSizeText;
        QString m_newFileSizeText;
        QString m_fileSizeDeltaText;
        QString m_oldSha256;
        QString m_newSha256;
        QString m_oldFontFamily;
        QString m_newFontFamily;
        int m_oldFontId = -1;
        int m_newFontId = -1;
        QVariantList m_metadataRows;
        QVariantList m_archiveEntries;
        int m_archiveAddedCount = 0;
        int m_archiveRemovedCount = 0;
        int m_archiveModifiedCount = 0;
        int m_archiveUnchangedCount = 0;
        QVariantList m_hexRows;
        bool m_hexTruncated = false;
        int m_hexChangedRows = 0;
        QString m_previewNotice;
        bool m_canRevealInFileManager = false;

        bool m_isLoading = false;
        LatestRequest m_diffRequest;
        LatestRequest m_blameRequest;
        // Private read lane: selection reads never wait behind the session's mutations.
        // Declared last: destroyed first, waiting for its tasks while this object is alive.
        std::unique_ptr<SerialGitExecutor> m_readExecutor;
    };
} // namespace gf::desktop
