#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QSet>
#include <QVariantList>

namespace gf::groups::bl {
class GroupService;
}

namespace gf::repositories::bl {
class RepositoryService;
}

namespace gf::desktop {

class QuickOpenViewModel : public QObject {
    Q_OBJECT

    Q_PROPERTY(QStringList selectedGroupIds READ selectedGroupIds NOTIFY selectionChanged)
    Q_PROPERTY(int selectedGroupsCount READ selectedGroupsCount NOTIFY selectionChanged)
    Q_PROPERTY(bool hasSelection READ hasSelection NOTIFY selectionChanged)

    Q_PROPERTY(QVariantList resolvedRepositories READ resolvedRepositories NOTIFY resolvedRepositoriesChanged)
    Q_PROPERTY(int resolvedRepositoriesCount READ resolvedRepositoriesCount NOTIFY resolvedRepositoriesChanged)
    Q_PROPERTY(int deduplicatedCount READ deduplicatedCount NOTIFY resolvedRepositoriesChanged)

public:
    explicit QuickOpenViewModel(gf::groups::bl::GroupService& groupService,
                                gf::repositories::bl::RepositoryService& repoService,
                                QObject* parent = nullptr);
    ~QuickOpenViewModel() override = default;

    [[nodiscard]] QStringList selectedGroupIds() const;
    [[nodiscard]] int selectedGroupsCount() const noexcept;
    [[nodiscard]] bool hasSelection() const noexcept;

    [[nodiscard]] QVariantList resolvedRepositories() const noexcept;
    [[nodiscard]] int resolvedRepositoriesCount() const noexcept;
    [[nodiscard]] int deduplicatedCount() const noexcept;

    Q_INVOKABLE void toggleGroup(const QString& groupId);
    Q_INVOKABLE void selectGroup(const QString& groupId, bool selected);
    Q_INVOKABLE bool isGroupSelected(const QString& groupId) const;
    Q_INVOKABLE void clearSelection();
    Q_INVOKABLE void selectAllGroups();

    Q_INVOKABLE void recalculateContext();
    Q_INVOKABLE bool openQuickSession();

signals:
    void selectionChanged();
    void resolvedRepositoriesChanged();
    void quickSessionStarted(const QStringList& repoIds);
    void errorOccurred(const QString& message);

private:
    gf::groups::bl::GroupService& m_groupService;
    gf::repositories::bl::RepositoryService& m_repoService;

    QStringList m_selectedGroupIds;
    QVariantList m_resolvedRepositories;
    int m_deduplicatedCount{0};
};

} // namespace gf::desktop
