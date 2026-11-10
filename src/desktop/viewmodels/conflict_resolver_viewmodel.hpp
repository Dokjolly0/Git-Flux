#pragma once

#include <QObject>
#include <QString>
#include <filesystem>
#include <memory>

#include "desktop/viewmodels/conflict_hunk_list_model.hpp"

namespace gf::git::bl
{
    class GitConflictService;
}

namespace gf::desktop
{
    class ConflictResolverViewModel : public QObject
    {
        Q_OBJECT
        Q_PROPERTY(QString repoPath READ repoPath WRITE setRepoPath NOTIFY repoPathChanged)
        Q_PROPERTY(QString currentFilePath READ currentFilePath NOTIFY currentFilePathChanged)
        Q_PROPERTY(ConflictHunkListModel* hunksModel READ hunksModel CONSTANT)
        Q_PROPERTY(int totalHunks READ totalHunks NOTIFY totalHunksChanged)
        Q_PROPERTY(int resolvedHunks READ resolvedHunks NOTIFY resolvedHunksChanged)
        Q_PROPERTY(bool allResolved READ allResolved NOTIFY allResolvedChanged)
        Q_PROPERTY(bool isBinary READ isBinary NOTIFY isBinaryChanged)
        Q_PROPERTY(bool isBusy READ isBusy NOTIFY isBusyChanged)
        Q_PROPERTY(QString errorMessage READ errorMessage NOTIFY errorMessageChanged)
        Q_PROPERTY(bool hasError READ hasError NOTIFY hasErrorChanged)

    public:
        explicit ConflictResolverViewModel(gf::git::bl::GitConflictService& conflictService,
                                           QObject* parent = nullptr);
        ~ConflictResolverViewModel() override = default;

        [[nodiscard]] QString repoPath() const { return m_repoPath; }
        void setRepoPath(const QString& path);

        [[nodiscard]] QString currentFilePath() const { return m_currentFilePath; }
        [[nodiscard]] ConflictHunkListModel* hunksModel() noexcept { return &m_hunksModel; }
        [[nodiscard]] int totalHunks() const noexcept { return m_hunksModel.rowCount(); }
        [[nodiscard]] int resolvedHunks() const noexcept { return m_hunksModel.resolvedCount(); }
        [[nodiscard]] bool allResolved() const noexcept { return m_hunksModel.allResolved(); }
        [[nodiscard]] bool isBinary() const noexcept { return m_isBinary; }
        [[nodiscard]] bool isBusy() const noexcept { return m_isBusy; }
        [[nodiscard]] QString errorMessage() const { return m_errorMessage; }
        [[nodiscard]] bool hasError() const noexcept { return !m_errorMessage.isEmpty(); }

    Q_INVOKABLE bool loadFile(const QString& relativeFilePath);
    Q_INVOKABLE void resolveHunk(int hunkIndex, int side, const QString& customText = {});
    Q_INVOKABLE bool applyResolution(bool autoStage = true);
    Q_INVOKABLE void clear();

        signals :

        void repoPathChanged();
        void currentFilePathChanged();
        void totalHunksChanged();
        void resolvedHunksChanged();
        void allResolvedChanged();
        void isBinaryChanged();
        void isBusyChanged();
        void errorMessageChanged();
        void hasErrorChanged();
        void resolutionApplied(const QString& filePath);

    private:
        void setBusy(bool busy);
        void setError(const QString& message);
        void clearError();

        gf::git::bl::GitConflictService& m_conflictService;
        ConflictHunkListModel m_hunksModel;
        QString m_repoPath;
        QString m_currentFilePath;
        std::string m_rawOriginalContent;
        QString m_errorMessage;
        bool m_isBinary{false};
        bool m_isBusy{false};
    };
} // namespace gf::desktop
