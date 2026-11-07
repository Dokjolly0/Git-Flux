#include "desktop/viewmodels/commit_history_list_model.hpp"

namespace gf::desktop {

CommitHistoryListModel::CommitHistoryListModel(QObject* parent)
    : QAbstractListModel(parent) {}

int CommitHistoryListModel::rowCount(const QModelIndex& parent) const {
    if (parent.isValid()) {
        return 0;
    }
    return static_cast<int>(m_commits.size());
}

QVariant CommitHistoryListModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid() || index.row() < 0 ||
        index.row() >= static_cast<int>(m_commits.size())) {
        return {};
    }

    const auto& c = m_commits[static_cast<std::size_t>(index.row())];

    switch (role) {
    case Qt::DisplayRole:
    case SubjectRole:
        return QString::fromStdString(c.subject);
    case ShaRole:
        return QString::fromStdString(c.sha);
    case ShortShaRole:
        return QString::fromStdString(c.shortSha);
    case BodyRole:
        return QString::fromStdString(c.body);
    case AuthorNameRole:
        return QString::fromStdString(c.authorName);
    case AuthorEmailRole:
        return QString::fromStdString(c.authorEmail);
    case AuthorDateRole:
        return QString::fromStdString(c.authorDateIso);
    case DecorationsRole: {
        QStringList list;
        for (const auto& dec : c.refDecorations) {
            list.append(QString::fromStdString(dec));
        }
        return list;
    }
    case ParentsRole: {
        QStringList list;
        for (const auto& p : c.parents) {
            list.append(QString::fromStdString(p));
        }
        return list;
    }
    default:
        return {};
    }
}

QHash<int, QByteArray> CommitHistoryListModel::roleNames() const {
    return {
        {ShaRole, "sha"},
        {ShortShaRole, "shortSha"},
        {SubjectRole, "subject"},
        {BodyRole, "body"},
        {AuthorNameRole, "authorName"},
        {AuthorEmailRole, "authorEmail"},
        {AuthorDateRole, "authorDate"},
        {DecorationsRole, "decorations"},
        {ParentsRole, "parents"}
    };
}

void CommitHistoryListModel::setCommits(std::vector<git::bo::GitCommitLogItem> commits) {
    beginResetModel();
    m_commits = std::move(commits);
    endResetModel();
}

} // namespace gf::desktop
