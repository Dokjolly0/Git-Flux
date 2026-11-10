#pragma once

#include <QObject>
#include <QString>
#include <QVariantList>
#include <filesystem>
#include <string>
#include <vector>

#include "core/modules/syntax/syntax_highlighter.hpp"
#include "desktop/viewmodels/diff_line_list_model.hpp"

namespace gf::git::bl {
class GitDiffService;
}

namespace gf::desktop {

class DiffViewModel final : public QObject {
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
    Q_PROPERTY(QVariantList blameLines READ blameLines NOTIFY blameLoaded)
    Q_PROPERTY(bool isBlameLoading READ isBlameLoading NOTIFY blameLoaded)

    // Diff context control (`-U<n>`, default 10, -1 for full file)
    Q_PROPERTY(int contextLines READ contextLines WRITE setContextLines NOTIFY contextLinesChanged)
    Q_PROPERTY(bool isFullContext READ isFullContext NOTIFY contextLinesChanged)

    // Markdown preview & Rich Diff support
    Q_PROPERTY(bool isMarkdown READ isMarkdown NOTIFY diffLoaded)
    Q_PROPERTY(bool isMarkdownRendered READ isMarkdownRendered WRITE setMarkdownRendered NOTIFY markdownRenderedChanged)
    Q_PROPERTY(QVariantList markdownBlocks READ markdownBlocks NOTIFY markdownDocumentChanged)
    Q_PROPERTY(QVariantList markdownToc READ markdownToc NOTIFY markdownDocumentChanged)
    Q_PROPERTY(QString markdownSource READ markdownSource NOTIFY markdownDocumentChanged)
    Q_PROPERTY(QString markdownNotice READ markdownNotice NOTIFY markdownDocumentChanged)
    Q_PROPERTY(bool showMarkdownRichDiff READ showMarkdownRichDiff WRITE setShowMarkdownRichDiff NOTIFY showMarkdownRichDiffChanged)

public:
    explicit DiffViewModel(gf::git::bl::GitDiffService& diffService, QObject* parent = nullptr);
    ~DiffViewModel() override = default;

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
    [[nodiscard]] QVariantList blameLines() const noexcept { return m_blameLines; }
    [[nodiscard]] bool isBlameLoading() const noexcept { return m_isBlameLoading; }

    [[nodiscard]] int contextLines() const noexcept { return m_contextLines; }
    [[nodiscard]] bool isFullContext() const noexcept { return m_contextLines < 0; }
    [[nodiscard]] bool isMarkdown() const noexcept { return m_isMarkdown; }
    [[nodiscard]] bool isMarkdownRendered() const noexcept { return m_isMarkdown && m_preferMarkdownRendered; }
    [[nodiscard]] QVariantList markdownBlocks() const noexcept { return m_markdownBlocks; }
    [[nodiscard]] QVariantList markdownToc() const noexcept { return m_markdownToc; }
    [[nodiscard]] QString markdownSource() const noexcept { return m_markdownSource; }
    [[nodiscard]] QString markdownNotice() const noexcept { return m_markdownNotice; }
    [[nodiscard]] bool showMarkdownRichDiff() const noexcept { return m_showMarkdownRichDiff; }

    Q_INVOKABLE void setViewMode(const QString& mode);
    Q_INVOKABLE void setContextLines(int lines);
    Q_INVOKABLE void expandContext(int delta = 15);
    Q_INVOKABLE void toggleFullContext();
    Q_INVOKABLE void setMarkdownRendered(bool rendered);
    Q_INVOKABLE void toggleMarkdownRendered();
    Q_INVOKABLE void setShowMarkdownRichDiff(bool show);
    Q_INVOKABLE bool reloadCurrentDiff();
    Q_INVOKABLE void copyToClipboard(const QString& text) const;

    Q_INVOKABLE bool loadDiff(const QString& repoPath, const QString& filePath, const QString& scope);
    Q_INVOKABLE bool loadCommitFileDiff(const QString& repoPath, const QString& commitSha, const QString& filePath);
    Q_INVOKABLE bool loadBlame(const QString& repoPath, const QString& revision, const QString& filePath);
    Q_INVOKABLE void clear();

signals:
    void diffLoaded();
    void blameLoaded();
    void viewModeChanged();
    void contextLinesChanged();
    void markdownRenderedChanged();
    void markdownDocumentChanged();
    void showMarkdownRichDiffChanged();
    void loadFailed(const QString& errorMessage);

private:
    void processFileDiff(const gf::git::bo::FileDiff& diff,
                         const std::vector<gf::core::syntax::MarkdownLineState>* newFileSeedStates = nullptr);
    void populateMarkdownDocument(const std::string& content, const gf::git::bo::FileDiff& diff);
    void updateMarkdownStateForDiff(const std::filesystem::path& repoPath,
                                    const std::string& filePath,
                                    const std::string& primaryRevision,
                                    const std::string& fallbackRevision,
                                    const std::string& deletedFallbackNotice,
                                    const gf::git::bo::FileDiff& diff,
                                    std::vector<gf::core::syntax::MarkdownLineState>& outSeedStates,
                                    bool& outHasSeedStates);

    gf::git::bl::GitDiffService& m_diffService;
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
    bool m_hasLastDiff = false;
    gf::git::bo::FileDiff m_lastDiff;

    QVariantList m_splitLines;
    QVariantList m_hunks;
    QVariantList m_blameLines;
    QVariantList m_markdownBlocks;
    QVariantList m_markdownToc;
    QString m_markdownSource;
    QString m_markdownNotice;
};

} // namespace gf::desktop
