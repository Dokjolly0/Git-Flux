#pragma once

#include "core/modules/git/bo/repo_working_tree_status.hpp"

#include <QAbstractListModel>
#include <vector>

namespace gf::desktop {

class WorkingTreeFileListModel final : public QAbstractListModel {
    Q_OBJECT

public:
    enum Roles {
        PathRole = Qt::UserRole + 1,
        OriginalPathRole,
        KindRole,
        StatusCodeRole,
        BadgeColorRole
    };
    Q_ENUM(Roles)

    explicit WorkingTreeFileListModel(QObject* parent = nullptr);
    ~WorkingTreeFileListModel() override = default;

    [[nodiscard]] int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    [[nodiscard]] QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

    void setItems(std::vector<gf::git::bo::WorkingTreeFileItem> items);
    void clear();

    [[nodiscard]] const std::vector<gf::git::bo::WorkingTreeFileItem>& items() const noexcept {
        return m_items;
    }

private:
    std::vector<gf::git::bo::WorkingTreeFileItem> m_items;
};

} // namespace gf::desktop
