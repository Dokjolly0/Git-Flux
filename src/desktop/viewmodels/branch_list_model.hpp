#pragma once

#include <QAbstractListModel>
#include <vector>

#ifndef Q_MOC_RUN
#include "core/modules/git/bo/branch_models.hpp"
#else
namespace gf::git::bo {
struct GitBranch;
}
#endif

namespace gf::desktop {

class BranchListModel final : public QAbstractListModel {
    Q_OBJECT

public:
    enum Roles {
        NameRole = Qt::UserRole + 1,
        ShortNameRole,
        IsCurrentRole,
        IsRemoteRole,
        UpstreamRole,
        AheadCountRole,
        BehindCountRole,
        TargetShaRole
    };

    explicit BranchListModel(QObject* parent = nullptr);
    ~BranchListModel() override = default;

    [[nodiscard]] int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    [[nodiscard]] QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

    void setBranches(std::vector<git::bo::GitBranch> branches);
    [[nodiscard]] const std::vector<git::bo::GitBranch>& branches() const noexcept { return m_branches; }

private:
    std::vector<git::bo::GitBranch> m_branches;
};

} // namespace gf::desktop
