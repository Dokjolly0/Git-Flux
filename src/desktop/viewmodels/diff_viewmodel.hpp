#pragma once

#include "core/modules/git/bl/git_diff_service.hpp"
#include "desktop/viewmodels/diff_line_list_model.hpp"

#include <QObject>
#include <QString>

namespace gf::desktop {

class DiffViewModel final : public QObject {
    Q_OBJECT

    Q_PROPERTY(DiffLineListModel* linesModel READ linesModel CONSTANT)
    Q_PROPERTY(QString filePath READ filePath NOTIFY diffLoaded)
    Q_PROPERTY(QString oldPath READ oldPath NOTIFY diffLoaded)
    Q_PROPERTY(int additionsCount READ additionsCount NOTIFY diffLoaded)
    Q_PROPERTY(int deletionsCount READ deletionsCount NOTIFY diffLoaded)
    Q_PROPERTY(bool isBinary READ isBinary NOTIFY diffLoaded)
    Q_PROPERTY(bool isNewFile READ isNewFile NOTIFY diffLoaded)
    Q_PROPERTY(bool isDeletedFile READ isDeletedFile NOTIFY diffLoaded)
    Q_PROPERTY(bool isRenamed READ isRenamed NOTIFY diffLoaded)
    Q_PROPERTY(bool isEmpty READ isEmpty NOTIFY diffLoaded)
    Q_PROPERTY(QString viewMode READ viewMode WRITE setViewMode NOTIFY viewModeChanged)
    Q_PROPERTY(QVariantList splitLines READ splitLines NOTIFY diffLoaded)
    Q_PROPERTY(QVariantList hunks READ hunks NOTIFY diffLoaded)
    Q_PROPERTY(QVariantList blameLines READ blameLines NOTIFY blameLoaded)
    Q_PROPERTY(bool isBlameLoading READ isBlameLoading NOTIFY blameLoaded)

public:
    explicit DiffViewModel(gf::git::bl::GitDiffService& diffService, QObject* parent = nullptr);
    ~DiffViewModel() override = default;

    [[nodiscard]] DiffLineListModel* linesModel() noexcept { return &m_linesModel; }
    [[nodiscard]] QString filePath() const noexcept { return m_filePath; }
    [[nodiscard]] QString oldPath() const noexcept { return m_oldPath; }
    [[nodiscard]] int additionsCount() const noexcept { return m_additionsCount; }
    [[nodiscard]] int deletionsCount() const noexcept { return m_deletionsCount; }
    [[nodiscard]] bool isBinary() const noexcept { return m_isBinary; }
    [[nodiscard]] bool isNewFile() const noexcept { return m_isNewFile; }
    [[nodiscard]] bool isDeletedFile() const noexcept { return m_isDeletedFile; }
    [[nodiscard]] bool isRenamed() const noexcept { return m_isRenamed; }
    [[nodiscard]] bool isEmpty() const noexcept { return m_isEmpty; }
    [[nodiscard]] QString viewMode() const noexcept { return m_viewMode; }
    [[nodiscard]] QVariantList splitLines() const noexcept { return m_splitLines; }
    [[nodiscard]] QVariantList hunks() const noexcept { return m_hunks; }
    [[nodiscard]] QVariantList blameLines() const noexcept { return m_blameLines; }
    [[nodiscard]] bool isBlameLoading() const noexcept { return m_isBlameLoading; }

    void setViewMode(const QString& mode);

    Q_INVOKABLE bool loadDiff(const QString& repoPath, const QString& filePath, const QString& scope);
    Q_INVOKABLE bool loadCommitFileDiff(const QString& repoPath, const QString& commitSha, const QString& filePath);
    Q_INVOKABLE bool loadBlame(const QString& repoPath, const QString& revision, const QString& filePath);
    Q_INVOKABLE void clear();

signals:
    void diffLoaded();
    void blameLoaded();
    void viewModeChanged();
    void loadFailed(const QString& errorMessage);

private:
    void processFileDiff(const gf::git::bo::FileDiff& diff);

    gf::git::bl::GitDiffService& m_diffService;
    DiffLineListModel m_linesModel;

    QString m_filePath;
    QString m_oldPath;
    QString m_currentRepoPath;
    QString m_currentRevision;
    QString m_viewMode = QStringLiteral("inline");
    int m_additionsCount = 0;
    int m_deletionsCount = 0;
    bool m_isBinary = false;
    bool m_isNewFile = false;
    bool m_isDeletedFile = false;
    bool m_isRenamed = false;
    bool m_isEmpty = true;
    bool m_isBlameLoading = false;

    QVariantList m_splitLines;
    QVariantList m_hunks;
    QVariantList m_blameLines;
};

} // namespace gf::desktop
