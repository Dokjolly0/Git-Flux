#pragma once

#include "core/modules/git/bo/diff_models.hpp"
#include "core/modules/syntax/language_detector.hpp"

#include <QAbstractListModel>
#include <string>
#include <vector>

namespace gf::desktop {

class DiffLineListModel final : public QAbstractListModel {
    Q_OBJECT

public:
    enum Roles {
        LineKindRole = Qt::UserRole + 1,
        OldLineNumberRole,
        NewLineNumberRole,
        ContentRole,
        RichContentRole,
        BgColorRole,
        FgColorRole
    };
    Q_ENUM(Roles)

    explicit DiffLineListModel(QObject* parent = nullptr);
    ~DiffLineListModel() override = default;

    [[nodiscard]] int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    [[nodiscard]] QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

    void setDiff(const gf::git::bo::FileDiff& diff, gf::core::syntax::Language lang = gf::core::syntax::Language::Unknown);
    void clear();

    [[nodiscard]] const std::vector<gf::git::bo::DiffLine>& flatLines() const noexcept {
        return m_lines;
    }

private:
    std::vector<gf::git::bo::DiffLine> m_lines;
    std::vector<std::string> m_richLines;
};

} // namespace gf::desktop
