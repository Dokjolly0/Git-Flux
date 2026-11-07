#pragma once

#include <QAbstractListModel>
#include <vector>

#ifndef Q_MOC_RUN
#include "core/modules/git/bo/stash_models.hpp"
#else
namespace gf::git::bo {
struct GitStashItem;
}
#endif

namespace gf::desktop {

class StashListModel : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(int count READ rowCount NOTIFY countChanged)

public:
    enum Roles {
        IndexRole = Qt::UserRole + 1,
        SelectorRole,
        CommitShaRole,
        DateIsoRole,
        MessageRole,
        BranchNameRole
    };
    Q_ENUM(Roles)

    explicit StashListModel(QObject* parent = nullptr);
    ~StashListModel() override = default;

    [[nodiscard]] int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    [[nodiscard]] QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

    void setStashes(std::vector<gf::git::bo::GitStashItem> stashes);
    void clear();

    [[nodiscard]] const std::vector<gf::git::bo::GitStashItem>& stashes() const noexcept { return m_stashes; }

signals:
    void countChanged();

private:
    std::vector<gf::git::bo::GitStashItem> m_stashes;
};

} // namespace gf::desktop
