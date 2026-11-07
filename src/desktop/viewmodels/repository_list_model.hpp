#pragma once

#include "core/modules/repositories/bo/repository.hpp"

#include <QAbstractListModel>
#include <QString>
#include <vector>
#include <optional>

namespace gf::desktop {

struct RepositoryItem {
    QString id;
    QString displayName;
    QString localPath;
    QString status;
    bool isMissing{false};
    QString lastAccessedAt;
    QString branch;
};

class RepositoryListModel : public QAbstractListModel {
    Q_OBJECT

public:
    enum RepositoryRoles {
        IdRole = Qt::UserRole + 1,
        DisplayNameRole,
        LocalPathRole,
        StatusRole,
        IsMissingRole,
        LastAccessedAtRole,
        BranchRole
    };

    explicit RepositoryListModel(QObject* parent = nullptr);
    ~RepositoryListModel() override = default;

    [[nodiscard]] int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    [[nodiscard]] QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

    void setRepositories(const std::vector<gf::repositories::bo::Repository>& repositories);
    [[nodiscard]] const std::vector<RepositoryItem>& items() const noexcept;
    [[nodiscard]] std::optional<RepositoryItem> findById(const QString& id) const;

    void updateStatus(const QString& id, const QString& status, bool isMissing);

private:
    std::vector<RepositoryItem> m_items;
};

} // namespace gf::desktop
