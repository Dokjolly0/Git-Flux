#include "desktop/viewmodels/diff_line_list_model.hpp"

namespace gf::desktop {

DiffLineListModel::DiffLineListModel(QObject* parent)
    : QAbstractListModel(parent) {}

int DiffLineListModel::rowCount(const QModelIndex& parent) const {
    if (parent.isValid()) {
        return 0;
    }
    return static_cast<int>(m_lines.size());
}

QVariant DiffLineListModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid() || index.row() < 0 ||
        index.row() >= static_cast<int>(m_lines.size())) {
        return {};
    }

    const auto& line = m_lines[static_cast<std::size_t>(index.row())];

    switch (role) {
    case LineKindRole:
        switch (line.kind) {
        case gf::git::bo::DiffLineKind::Context:   return QStringLiteral("context");
        case gf::git::bo::DiffLineKind::Addition:  return QStringLiteral("addition");
        case gf::git::bo::DiffLineKind::Deletion:  return QStringLiteral("deletion");
        case gf::git::bo::DiffLineKind::Header:    return QStringLiteral("header");
        case gf::git::bo::DiffLineKind::NoNewline: return QStringLiteral("noNewline");
        }
        return QStringLiteral("context");

    case OldLineNumberRole:
        return line.oldLineNumber.has_value()
                   ? QString::number(*line.oldLineNumber)
                   : QString{};

    case NewLineNumberRole:
        return line.newLineNumber.has_value()
                   ? QString::number(*line.newLineNumber)
                   : QString{};

    case ContentRole:
    case Qt::DisplayRole:
        return QString::fromStdString(line.content);

    case BgColorRole:
        switch (line.kind) {
        case gf::git::bo::DiffLineKind::Addition:
            return QStringLiteral("#143525"); // Verde scuro discreto
        case gf::git::bo::DiffLineKind::Deletion:
            return QStringLiteral("#3E181C"); // Rosso scuro discreto
        case gf::git::bo::DiffLineKind::Header:
            return QStringLiteral("#1E293B"); // Slate blue scuro
        case gf::git::bo::DiffLineKind::Context:
        case gf::git::bo::DiffLineKind::NoNewline:
        default:
            return QStringLiteral("transparent");
        }

    case FgColorRole:
        switch (line.kind) {
        case gf::git::bo::DiffLineKind::Addition:
            return QStringLiteral("#4ADE80"); // Verde brillante
        case gf::git::bo::DiffLineKind::Deletion:
            return QStringLiteral("#F87171"); // Rosso salmone
        case gf::git::bo::DiffLineKind::Header:
            return QStringLiteral("#93C5FD"); // Blu cielo
        case gf::git::bo::DiffLineKind::NoNewline:
            return QStringLiteral("#64748B"); // Slate grigio
        case gf::git::bo::DiffLineKind::Context:
        default:
            return QStringLiteral("#E2E8F0"); // Testo chiaro standard
        }

    default:
        return {};
    }
}

QHash<int, QByteArray> DiffLineListModel::roleNames() const {
    return {
        {LineKindRole, "lineKind"},
        {OldLineNumberRole, "oldLineNumber"},
        {NewLineNumberRole, "newLineNumber"},
        {ContentRole, "content"},
        {BgColorRole, "bgColor"},
        {FgColorRole, "fgColor"}
    };
}

void DiffLineListModel::setDiff(const gf::git::bo::FileDiff& diff) {
    beginResetModel();
    m_lines.clear();

    for (const auto& hunk : diff.hunks) {
        // Aggiunge riga di intestazione hunk
        m_lines.push_back(gf::git::bo::DiffLine{
            .kind = gf::git::bo::DiffLineKind::Header,
            .oldLineNumber = std::nullopt,
            .newLineNumber = std::nullopt,
            .content = hunk.header
        });

        // Aggiunge tutte le righe dell'hunk
        for (const auto& l : hunk.lines) {
            m_lines.push_back(l);
        }
    }

    endResetModel();
}

void DiffLineListModel::clear() {
    beginResetModel();
    m_lines.clear();
    endResetModel();
}

} // namespace gf::desktop
