#include "desktop/viewmodels/commit_history_list_model.hpp"

#include <QCryptographicHash>
#include <QHash>
#include <QRegularExpression>
#include <QVariantMap>

namespace gf::desktop {

namespace {

const QStringList kAuthorPalette = {
    QStringLiteral("#6366F1"), // Indigo
    QStringLiteral("#06B6D4"), // Cyan
    QStringLiteral("#10B981"), // Emerald
    QStringLiteral("#F59E0B"), // Amber
    QStringLiteral("#EC4899"), // Pink
    QStringLiteral("#8B5CF6"), // Purple
    QStringLiteral("#14B8A6"), // Teal
    QStringLiteral("#3B82F6"), // Blue
    QStringLiteral("#E11D48"), // Rose
    QStringLiteral("#84CC16")  // Lime
};

QString computeAvatarUrl(const QString& rawEmail) {
    QString email = rawEmail.trimmed().toLower();
    if (email.isEmpty()) {
        return QStringLiteral("https://www.gravatar.com/avatar/?d=mp");
    }

    // Check if it is a GitHub noreply address: e.g. "123456+username@users.noreply.github.com" or "username@users.noreply.github.com"
    if (email.endsWith(QLatin1String("users.noreply.github.com"))) {
        int atIdx = email.indexOf(QLatin1Char('@'));
        if (atIdx > 0) {
            QString userPart = email.left(atIdx);
            int plusIdx = userPart.indexOf(QLatin1Char('+'));
            if (plusIdx >= 0) {
                userPart = userPart.mid(plusIdx + 1);
            }
            if (!userPart.isEmpty()) {
                return QStringLiteral("https://github.com/%1.png?size=64").arg(userPart);
            }
        }
    }

    // Standard Gravatar hash
    QByteArray hash = QCryptographicHash::hash(email.toUtf8(), QCryptographicHash::Md5).toHex();
    return QStringLiteral("https://www.gravatar.com/avatar/%1?s=64&d=identicon").arg(QString::fromLatin1(hash));
}

QString computeInitials(const QString& name) {
    QString clean = name.trimmed();
    if (clean.isEmpty()) return QStringLiteral("??");

    QStringList parts = clean.split(QLatin1Char(' '), Qt::SkipEmptyParts);
    if (parts.size() >= 2) {
        return QString(parts[0].front().toUpper()) + QString(parts[1].front().toUpper());
    }
    if (clean.length() >= 2) {
        return clean.left(2).toUpper();
    }
    return clean.toUpper();
}

QString computeAuthorColor(const QString& name, const QString& email) {
    QString key = email.isEmpty() ? name : email;
    uint hash = qHash(key);
    int idx = static_cast<int>(hash % static_cast<uint>(kAuthorPalette.size()));
    return kAuthorPalette[idx];
}

QVariantList computeStructuredDecorations(const std::vector<std::string>& rawDecorations) {
    struct RawRef {
        QString name;
        bool isTag = false;
        bool isRemote = false;
        bool isHead = false;
    };

    QList<RawRef> tags;
    QList<RawRef> localBranches;
    QList<RawRef> remoteBranches;

    for (const auto& decStd : rawDecorations) {
        QString dec = QString::fromStdString(decStd).trimmed();
        if (dec.isEmpty()) continue;

        // Skip origin/HEAD, remote HEAD, or plain HEAD
        if (dec == QLatin1String("origin/HEAD") || dec.endsWith(QLatin1String("/HEAD")) || dec == QLatin1String("HEAD")) {
            continue;
        }

        if (dec.startsWith(QLatin1String("tag: "))) {
            tags.append(RawRef{
                .name = dec.mid(5).trimmed(),
                .isTag = true,
                .isRemote = false,
                .isHead = false
            });
            continue;
        }

        bool isHead = false;
        if (dec.startsWith(QLatin1String("HEAD -> "))) {
            isHead = true;
            dec = dec.mid(8).trimmed();
        }

        if (dec.contains(QLatin1Char('/'))) {
            remoteBranches.append(RawRef{
                .name = dec,
                .isTag = false,
                .isRemote = true,
                .isHead = isHead
            });
        } else {
            localBranches.append(RawRef{
                .name = dec,
                .isTag = false,
                .isRemote = false,
                .isHead = isHead
            });
        }
    }

    QVariantList result;

    // Process local branches with GitKraken alignment logic:
    // If local branch B is on the same commit as origin/B (aligned), suppress local B and display origin/B
    for (const auto& lb : localBranches) {
        bool hasMatchingRemoteAtSameCommit = false;
        for (auto& rb : remoteBranches) {
            if (rb.name == (QLatin1String("origin/") + lb.name) || rb.name.endsWith(QLatin1String("/") + lb.name)) {
                hasMatchingRemoteAtSameCommit = true;
                if (lb.isHead) {
                    rb.isHead = true; // Forward HEAD indicator to remote badge
                }
                break;
            }
        }

        if (!hasMatchingRemoteAtSameCommit) {
            // Local branch is ahead or has no remote: show local badge!
            QVariantMap badge;
            badge[QStringLiteral("name")] = lb.name;
            badge[QStringLiteral("type")] = QStringLiteral("local");
            badge[QStringLiteral("isHead")] = lb.isHead;
            badge[QStringLiteral("icon")] = lb.isHead ? QStringLiteral("● ⎇") : QStringLiteral("⎇");
            badge[QStringLiteral("bgColor")] = QStringLiteral("#064E3B"); // Emerald 900
            badge[QStringLiteral("borderColor")] = QStringLiteral("#10B981"); // Emerald 500
            badge[QStringLiteral("fgColor")] = QStringLiteral("#D1FAE5"); // Emerald 100
            result.append(badge);
        }
    }

    // Process remote branches
    for (const auto& rb : remoteBranches) {
        QVariantMap badge;
        badge[QStringLiteral("name")] = rb.name;
        badge[QStringLiteral("type")] = QStringLiteral("remote");
        badge[QStringLiteral("isHead")] = rb.isHead;
        badge[QStringLiteral("icon")] = rb.isHead ? QStringLiteral("● ☁") : QStringLiteral("☁");
        badge[QStringLiteral("bgColor")] = QStringLiteral("#312E81"); // Indigo 900
        badge[QStringLiteral("borderColor")] = QStringLiteral("#6366F1"); // Indigo 500
        badge[QStringLiteral("fgColor")] = QStringLiteral("#E0E7FF"); // Indigo 100
        result.append(badge);
    }

    // Process tags
    for (const auto& tb : tags) {
        QVariantMap badge;
        badge[QStringLiteral("name")] = tb.name;
        badge[QStringLiteral("type")] = QStringLiteral("tag");
        badge[QStringLiteral("isHead")] = false;
        badge[QStringLiteral("icon")] = QStringLiteral("🏷");
        badge[QStringLiteral("bgColor")] = QStringLiteral("#78350F"); // Amber 900
        badge[QStringLiteral("borderColor")] = QStringLiteral("#F59E0B"); // Amber 500
        badge[QStringLiteral("fgColor")] = QStringLiteral("#FEF3C7"); // Amber 100
        result.append(badge);
    }

    return result;
}

} // namespace

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
    case AuthorAvatarUrlRole:
        return computeAvatarUrl(QString::fromStdString(c.authorEmail));
    case AuthorInitialsRole:
        return computeInitials(QString::fromStdString(c.authorName));
    case AuthorColorRole:
        return computeAuthorColor(QString::fromStdString(c.authorName), QString::fromStdString(c.authorEmail));
    case StructuredDecorationsRole:
        return computeStructuredDecorations(c.refDecorations);
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
        {ParentsRole, "parents"},
        {AuthorAvatarUrlRole, "authorAvatarUrl"},
        {AuthorInitialsRole, "authorInitials"},
        {AuthorColorRole, "authorColor"},
        {StructuredDecorationsRole, "structuredDecorations"}
    };
}

void CommitHistoryListModel::setCommits(std::vector<git::bo::GitCommitLogItem> commits) {
    beginResetModel();
    m_commits = std::move(commits);
    endResetModel();
}

} // namespace gf::desktop
