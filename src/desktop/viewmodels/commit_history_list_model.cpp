#include "desktop/viewmodels/commit_history_list_model.hpp"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMutex>
#include <QMutexLocker>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSettings>
#include <QUrl>

#include <QCryptographicHash>
#include <QDateTime>
#include <QHash>
#include <QRegularExpression>
#include <QVariantMap>
#include <algorithm>
#include <cstdint>
#include <string_view>
#include <unordered_map>
#include <unordered_set>

namespace gf::desktop
{
    namespace
    {
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
            QStringLiteral("#84CC16") // Lime
        };

        const QStringList kGraphPalette = {
            QStringLiteral("#06B6D4"), // Cyan (main)
            QStringLiteral("#A855F7"), // Purple
            QStringLiteral("#F97316"), // Orange
            QStringLiteral("#10B981"), // Emerald
            QStringLiteral("#EC4899"), // Pink
            QStringLiteral("#EAB308"), // Yellow
            QStringLiteral("#38BDF8"), // Sky Blue
            QStringLiteral("#8B5CF6") // Violet
        };

        QString computeAvatarUrl(const QString& name, const QString& rawEmail)
        {
            QString email = rawEmail.trimmed().toLower();
            QString cleanName = name.trimmed();

            // 1. If email is a GitHub noreply address
            if (email.endsWith(QLatin1String("users.noreply.github.com")))
            {
                int atIdx = email.indexOf(QLatin1Char('@'));
                if (atIdx > 0)
                {
                    QString userPart = email.left(atIdx);
                    int plusIdx = userPart.indexOf(QLatin1Char('+'));
                    if (plusIdx >= 0) userPart = userPart.mid(plusIdx + 1);
                    if (!userPart.isEmpty())
                    {
                        return QStringLiteral("https://github.com/%1.png?size=64").arg(userPart);
                    }
                }
            }

            // 2. If name is a single username token without spaces (e.g. "Dokjolly0")
            if (!cleanName.isEmpty() && !cleanName.contains(QLatin1Char(' ')))
            {
                return QStringLiteral("https://github.com/%1.png?size=64").arg(cleanName);
            }

            // 2.5 Check email-to-GitHub-avatar cache
            if (!email.isEmpty())
            {
                QString cached = CommitHistoryListModel::cachedAvatarForEmail(email);
                if (!cached.isEmpty())
                {
                    return cached;
                }
            }

            // 3. Fallback to Gravatar MD5
            if (!email.isEmpty())
            {
                QByteArray hash = QCryptographicHash::hash(email.toUtf8(), QCryptographicHash::Md5).toHex();
                return QStringLiteral("https://www.gravatar.com/avatar/%1?s=64&d=identicon").arg(
                    QString::fromLatin1(hash));
            }

            return QStringLiteral("https://www.gravatar.com/avatar/?d=mp");
        }

        QString computeCommitAvatarUrl(const git::bo::GitCommitLogItem& c)
        {
            QString authorName = QString::fromStdString(c.authorName).trimmed();
            QString authorEmail = QString::fromStdString(c.authorEmail).trimmed().toLower();
            QString committerName = QString::fromStdString(c.committerName).trimmed();
            QString committerEmail = QString::fromStdString(c.committerEmail).trimmed().toLower();

            // 1. Check if authorName is a single-word GitHub username (e.g. Dokjolly0)
            if (!authorName.isEmpty() && !authorName.contains(QLatin1Char(' ')))
            {
                return QStringLiteral("https://github.com/%1.png?size=64").arg(authorName);
            }

            // 2. Check if authorEmail is GitHub noreply
            if (authorEmail.endsWith(QLatin1String("users.noreply.github.com")))
            {
                int atIdx = authorEmail.indexOf(QLatin1Char('@'));
                if (atIdx > 0)
                {
                    QString userPart = authorEmail.left(atIdx);
                    int plusIdx = userPart.indexOf(QLatin1Char('+'));
                    if (plusIdx >= 0) userPart = userPart.mid(plusIdx + 1);
                    if (!userPart.isEmpty())
                    {
                        return QStringLiteral("https://github.com/%1.png?size=64").arg(userPart);
                    }
                }
            }

            // 3. Check if committerName is a single-word GitHub username (e.g. Dokjolly0 when author has full real name)
            if (!authorEmail.isEmpty())
            {
                QString cached = CommitHistoryListModel::cachedAvatarForEmail(authorEmail);
                if (!cached.isEmpty())
                {
                    return cached;
                }
            }
            if (!committerName.isEmpty() && !committerName.contains(QLatin1Char(' ')) &&
                committerName.compare(QLatin1String("GitHub"), Qt::CaseInsensitive) != 0)
            {
                return QStringLiteral("https://github.com/%1.png?size=64").arg(committerName);
            }

            // 4. Check if committerEmail is GitHub noreply
            if (committerEmail.endsWith(QLatin1String("users.noreply.github.com")))
            {
                int atIdx = committerEmail.indexOf(QLatin1Char('@'));
                if (atIdx > 0)
                {
                    QString userPart = committerEmail.left(atIdx);
                    int plusIdx = userPart.indexOf(QLatin1Char('+'));
                    if (plusIdx >= 0) userPart = userPart.mid(plusIdx + 1);
                    if (!userPart.isEmpty())
                    {
                        return QStringLiteral("https://github.com/%1.png?size=64").arg(userPart);
                    }
                }
            }

            // 4.5 Check email-to-GitHub-avatar cache for committerEmail
            if (!committerEmail.isEmpty())
            {
                QString cached = CommitHistoryListModel::cachedAvatarForEmail(committerEmail);
                if (!cached.isEmpty())
                {
                    return cached;
                }
            }

            // 5. Fallback to Gravatar MD5
            QString emailForGravatar = !authorEmail.isEmpty() ? authorEmail : committerEmail;
            if (!emailForGravatar.isEmpty())
            {
                QByteArray hash = QCryptographicHash::hash(emailForGravatar.toUtf8(), QCryptographicHash::Md5).toHex();
                return QStringLiteral("https://www.gravatar.com/avatar/%1?s=64&d=identicon").arg(
                    QString::fromLatin1(hash));
            }

            return QStringLiteral("https://www.gravatar.com/avatar/?d=mp");
        }

        QString formatCommitDate(const QString& isoStr)
        {
            QDateTime dt = QDateTime::fromString(isoStr, Qt::ISODate);
            if (!dt.isValid())
            {
                return isoStr;
            }
            return dt.toString(QStringLiteral("dd/MM/yyyy @ HH:mm"));
        }

        QString computeInitials(const QString& name)
        {
            QString clean = name.trimmed();
            if (clean.isEmpty()) return QStringLiteral("??");

            QStringList parts = clean.split(QLatin1Char(' '), Qt::SkipEmptyParts);
            if (parts.size() >= 2)
            {
                return QString(parts[0].front().toUpper()) + QString(parts[1].front().toUpper());
            }
            if (clean.length() >= 2)
            {
                return clean.left(2).toUpper();
            }
            return clean.toUpper();
        }

        QString computeAuthorColor(const QString& name, const QString& email)
        {
            QString key = email.isEmpty() ? name : email;
            const size_t hash = qHash(key);
            const int idx = static_cast<int>(hash % kAuthorPalette.size());
            return kAuthorPalette[idx];
        }

        // `git log %D` marks the checked-out commit as "HEAD -> <branch>", or plain "HEAD" when detached.
        QString findHeadSha(const std::vector<gf::git::bo::GitCommitLogItem>& commits)
        {
            for (const auto& c : commits)
            {
                for (const auto& decStd : c.refDecorations)
                {
                    const QString dec = QString::fromStdString(decStd).trimmed();
                    if (dec == QLatin1String("HEAD") || dec.startsWith(QLatin1String("HEAD -> ")))
                    {
                        return QString::fromStdString(c.sha);
                    }
                }
            }
            return {};
        }

        bool isStashDecoration(const std::string& dec)
        {
            return dec == "refs/stash" || dec == "stash" || dec.starts_with("stash@{");
        }

        QString extractStashSelector(const std::vector<std::string>& decs)
        {
            for (const auto& dec : decs)
            {
                if (dec.starts_with("stash@{"))
                {
                    return QString::fromStdString(dec);
                }
            }
            for (const auto& dec : decs)
            {
                if (dec == "refs/stash" || dec == "stash")
                {
                    return QStringLiteral("stash@{0}");
                }
            }
            return {};
        }

        bool isStashCommit(const git::bo::GitCommitLogItem& c)
        {
            return std::any_of(c.refDecorations.begin(), c.refDecorations.end(),
                               [](const std::string& d) { return isStashDecoration(d); });
        }

        bool hasBranchDecoration(const std::vector<std::string>& rawDecorations)
        {
            for (const auto& decStd : rawDecorations)
            {
                const QString dec = QString::fromStdString(decStd).trimmed();
                if (dec.isEmpty() ||
                    dec == QLatin1String("origin/HEAD") ||
                    dec.endsWith(QLatin1String("/HEAD")) ||
                    dec == QLatin1String("HEAD") ||
                    isStashDecoration(decStd) ||
                    dec.startsWith(QLatin1String("tag: ")))
                {
                    continue;
                }
                return true;
            }
            return false;
        }

        QVariantMap makeGraphRoute(int fromLane, int toLane, const QString& color, int type, bool dashed = false)
        {
            QVariantMap r;
            r[QStringLiteral("fromLane")] = fromLane;
            r[QStringLiteral("toLane")] = toLane;
            r[QStringLiteral("color")] = color;
            r[QStringLiteral("type")] = type;
            r[QStringLiteral("dashed")] = dashed;
            return r;
        }

        QVariantList computeStructuredDecorations(const std::vector<std::string>& rawDecorations)
        {
            struct RawRef
            {
                QString name;
                bool isTag = false;
                bool isRemote = false;
                bool isHead = false;
            };

            QList<RawRef> tags;
            QList<RawRef> localBranches;
            QList<RawRef> remoteBranches;
            QStringList stashes;

            for (const auto& decStd : rawDecorations)
            {
                QString dec = QString::fromStdString(decStd).trimmed();
                if (dec.isEmpty()) continue;

                // Skip origin/HEAD, remote HEAD, or plain HEAD
                if (dec == QLatin1String("origin/HEAD") || dec.endsWith(QLatin1String("/HEAD")) || dec ==
                    QLatin1String("HEAD"))
                {
                    continue;
                }

                if (dec == QLatin1String("refs/stash") || dec == QLatin1String("stash") ||
                    dec.startsWith(QLatin1String("stash@{")))
                {
                    const QString selector = (dec.startsWith(QLatin1String("stash@{")))
                                                 ? dec
                                                 : QStringLiteral("stash@{0}");
                    if (!stashes.contains(selector))
                    {
                        stashes.append(selector);
                    }
                    continue;
                }

                if (dec.startsWith(QLatin1String("tag: ")))
                {
                    tags.append(RawRef{
                        .name = dec.mid(5).trimmed(),
                        .isTag = true,
                        .isRemote = false,
                        .isHead = false
                    });
                    continue;
                }

                bool isHead = false;
                if (dec.startsWith(QLatin1String("HEAD -> ")))
                {
                    isHead = true;
                    dec = dec.mid(8).trimmed();
                }

                if (dec.contains(QLatin1Char('/')))
                {
                    remoteBranches.append(RawRef{
                        .name = dec,
                        .isTag = false,
                        .isRemote = true,
                        .isHead = isHead
                    });
                }
                else
                {
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
            for (const auto& lb : localBranches)
            {
                bool hasMatchingRemoteAtSameCommit = false;
                for (auto& rb : remoteBranches)
                {
                    if (rb.name == (QLatin1String("origin/") + lb.name) || rb.name.endsWith(
                        QLatin1String("/") + lb.name))
                    {
                        hasMatchingRemoteAtSameCommit = true;
                        if (lb.isHead)
                        {
                            rb.isHead = true; // Forward HEAD indicator to remote badge
                        }
                        break;
                    }
                }

                if (!hasMatchingRemoteAtSameCommit)
                {
                    // Local branch is ahead or has no remote: show local badge!
                    bool isPages = (lb.name == QLatin1String("gh-pages") || lb.name.
                                                                               contains(QLatin1String("gh-pages")));
                    QVariantMap badge;
                    badge[QStringLiteral("name")] = lb.name;
                    badge[QStringLiteral("type")] = QStringLiteral("local");
                    badge[QStringLiteral("isHead")] = lb.isHead;
                    badge[QStringLiteral("isOrphan")] = isPages;
                    if (isPages)
                    {
                        badge[QStringLiteral("label")] = lb.isHead
                                                             ? QStringLiteral("✓ 📄 %1 💻").arg(lb.name)
                                                             : QStringLiteral("📄 %1 💻").arg(lb.name);
                        badge[QStringLiteral("bgColor")] = QStringLiteral("#2E1065");
                        badge[QStringLiteral("borderColor")] = QStringLiteral("#A855F7");
                        badge[QStringLiteral("fgColor")] = QStringLiteral("#E9D5FF");
                    }
                    else
                    {
                        badge[QStringLiteral("label")] = lb.isHead
                                                             ? QStringLiteral("✓ %1 💻").arg(lb.name)
                                                             : QStringLiteral("%1 💻").arg(lb.name);
                        badge[QStringLiteral("bgColor")] = lb.isHead
                                                               ? QStringLiteral("#0E7490")
                                                               : QStringLiteral("#064E3B");
                        badge[QStringLiteral("borderColor")] = lb.isHead
                                                                   ? QStringLiteral("#06B6D4")
                                                                   : QStringLiteral("#10B981");
                        badge[QStringLiteral("fgColor")] = QStringLiteral("#FFFFFF");
                    }
                    result.append(badge);
                }
            }

            // Process remote branches
            for (const auto& rb : remoteBranches)
            {
                QVariantMap badge;
                QString cleanName = rb.name;
                if (cleanName.startsWith(QLatin1String("origin/")))
                {
                    cleanName = cleanName.mid(7);
                }
                bool isPages = (cleanName == QLatin1String("gh-pages") || rb.name.contains(QLatin1String("gh-pages")));
                badge[QStringLiteral("name")] = rb.name;
                badge[QStringLiteral("type")] = QStringLiteral("remote");
                badge[QStringLiteral("isHead")] = rb.isHead;
                badge[QStringLiteral("isOrphan")] = isPages;
                if (isPages)
                {
                    badge[QStringLiteral("label")] = QStringLiteral("📄 %1 ☁").arg(cleanName);
                    badge[QStringLiteral("bgColor")] = QStringLiteral("#2E1065"); // Dark violet for orphan/pages
                    badge[QStringLiteral("borderColor")] = QStringLiteral("#A855F7"); // Purple border
                    badge[QStringLiteral("fgColor")] = QStringLiteral("#E9D5FF"); // Soft violet text
                }
                else
                {
                    badge[QStringLiteral("label")] = QStringLiteral("%1 ☁").arg(cleanName);
                    badge[QStringLiteral("bgColor")] = QStringLiteral("#1E293B"); // GitKraken dark slate blue
                    badge[QStringLiteral("borderColor")] = QStringLiteral("#3B82F6"); // Slate border
                    badge[QStringLiteral("fgColor")] = QStringLiteral("#93C5FD"); // Light blue text
                }
                result.append(badge);
            }

            // Process stashes (GitKraken-style amber stash pill)
            for (const auto& selector : stashes)
            {
                QVariantMap badge;
                badge[QStringLiteral("name")] = selector;
                badge[QStringLiteral("type")] = QStringLiteral("stash");
                badge[QStringLiteral("isHead")] = false;
                badge[QStringLiteral("isOrphan")] = false;
                badge[QStringLiteral("label")] = QStringLiteral("📦 %1").arg(selector);
                badge[QStringLiteral("bgColor")] = QStringLiteral("#451A03");
                badge[QStringLiteral("borderColor")] = QStringLiteral("#F59E0B");
                badge[QStringLiteral("fgColor")] = QStringLiteral("#FDE68A");
                result.append(badge);
            }

            // Process tags
            for (const auto& tb : tags)
            {
                QVariantMap badge;
                badge[QStringLiteral("name")] = tb.name;
                badge[QStringLiteral("type")] = QStringLiteral("tag");
                badge[QStringLiteral("isHead")] = false;
                badge[QStringLiteral("label")] = QStringLiteral("🏷 %1").arg(tb.name);
                badge[QStringLiteral("bgColor")] = QStringLiteral("#78350F"); // Amber 900
                badge[QStringLiteral("borderColor")] = QStringLiteral("#F59E0B"); // Amber 500
                badge[QStringLiteral("fgColor")] = QStringLiteral("#FEF3C7"); // Amber 100
                result.append(badge);
            }

            return result;
        }

        /// Extracts `Co-authored-by: Name <email>` trailers from a commit body, skipping the
        /// main author and duplicates (matched by e-mail, case-insensitive).
        QVariantList computeCoAuthors(const git::bo::GitCommitLogItem& c)
        {
            static const QRegularExpression kCoAuthorRe(
                QStringLiteral("^\\s*co-authored-by\\s*:\\s*(.*?)\\s*<([^>]*)>\\s*$"),
                QRegularExpression::CaseInsensitiveOption | QRegularExpression::MultilineOption);

            QVariantList result;
            QSet<QString> seenEmails;
            seenEmails.insert(QString::fromStdString(c.authorEmail).trimmed().toLower());

            auto it = kCoAuthorRe.globalMatch(QString::fromStdString(c.body));
            while (it.hasNext())
            {
                const auto match = it.next();
                const QString name = match.captured(1).trimmed();
                const QString email = match.captured(2).trimmed();
                const QString key = email.toLower();
                if ((name.isEmpty() && email.isEmpty()) || (!key.isEmpty() && seenEmails.contains(key)))
                {
                    continue;
                }
                seenEmails.insert(key);

                const QString displayName = name.isEmpty() ? email : name;
                QVariantMap entry;
                entry[QStringLiteral("name")] = displayName;
                entry[QStringLiteral("email")] = email;
                entry[QStringLiteral("avatarUrl")] = computeAvatarUrl(displayName, email);
                entry[QStringLiteral("initials")] = computeInitials(displayName);
                entry[QStringLiteral("color")] = computeAuthorColor(displayName, email);
                result.append(entry);
            }
            return result;
        }
    } // namespace

    CommitHistoryListModel::CommitHistoryListModel(QObject* parent)
        : QAbstractListModel(parent)
    {
    }

    int CommitHistoryListModel::rowCount(const QModelIndex& parent) const
    {
        if (parent.isValid())
        {
            return 0;
        }
        return static_cast<int>(m_commits.size());
    }

    QVariant CommitHistoryListModel::data(const QModelIndex& index, int role) const
    {
        if (!index.isValid() || index.row() < 0 ||
            index.row() >= static_cast<int>(m_commits.size()))
        {
            return {};
        }

        std::size_t idx = static_cast<std::size_t>(index.row());
        const auto& c = m_commits[idx];

        switch (role)
        {
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
        case AuthorDateFormattedRole:
            return formatCommitDate(QString::fromStdString(c.authorDateIso));
        case DecorationsRole:
            {
                QStringList list;
                for (const auto& dec : c.refDecorations)
                {
                    list.append(QString::fromStdString(dec));
                }
                return list;
            }
        case ParentsRole:
            {
                QStringList list;
                for (const auto& p : c.parents)
                {
                    list.append(QString::fromStdString(p));
                }
                return list;
            }
        case AuthorAvatarUrlRole:
            return computeCommitAvatarUrl(c);
        case AuthorInitialsRole:
            return computeInitials(QString::fromStdString(c.authorName));
        case AuthorColorRole:
            return computeAuthorColor(QString::fromStdString(c.authorName), QString::fromStdString(c.authorEmail));
        case StructuredDecorationsRole:
            return computeStructuredDecorations(c.refDecorations);
        case CommitterNameRole:
            return QString::fromStdString(c.committerName);
        case CommitterEmailRole:
            return QString::fromStdString(c.committerEmail);
        case CommitterDateFormattedRole:
            return formatCommitDate(QString::fromStdString(c.committerDateIso));
        case CommitterAvatarUrlRole:
            return computeAvatarUrl(QString::fromStdString(c.committerName), QString::fromStdString(c.committerEmail));
        case CommitterInitialsRole:
            return computeInitials(QString::fromStdString(c.committerName));
        case GraphLaneRole:
            return (idx < m_graphItems.size()) ? m_graphItems[idx].lane : 0;
        case GraphColorRole:
            return (idx < m_graphItems.size()) ? m_graphItems[idx].color : QStringLiteral("#06B6D4");
        case GraphRoutesRole:
            return (idx < m_graphItems.size()) ? m_graphItems[idx].routes : QVariantList{};
        case IsMergeRole:
            return !isStashCommit(c) && c.parents.size() > 1;
        case IsWipRole:
            return c.sha == "WIP";
        case IsOrphanRole:
            return (idx < m_graphItems.size()) ? m_graphItems[idx].isOrphan : false;
        case IsStashRole:
            return (idx < m_graphItems.size()) ? m_graphItems[idx].isStash : isStashCommit(c);
        case StashSelectorRole:
            return (idx < m_graphItems.size()) ? m_graphItems[idx].stashSelector : extractStashSelector(c.refDecorations);
        case BranchCommitCountRole:
            return branchCommitCountForCommit(c);
        default:
            return {};
        }
    }

    QHash<int, QByteArray> CommitHistoryListModel::roleNames() const
    {
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
            {StructuredDecorationsRole, "structuredDecorations"},
            {AuthorDateFormattedRole, "authorDateFormatted"},
            {CommitterNameRole, "committerName"},
            {CommitterEmailRole, "committerEmail"},
            {CommitterDateFormattedRole, "committerDateFormatted"},
            {CommitterAvatarUrlRole, "committerAvatarUrl"},
            {CommitterInitialsRole, "committerInitials"},
            {GraphLaneRole, "graphLane"},
            {GraphColorRole, "graphColor"},
            {GraphRoutesRole, "graphRoutes"},
            {IsMergeRole, "isMerge"},
            {IsWipRole, "isWip"},
            {IsOrphanRole, "isOrphan"},
            {IsStashRole, "isStash"},
            {StashSelectorRole, "stashSelector"},
            {BranchCommitCountRole, "branchCommitCount"}
        };
    }

    QVariantMap CommitHistoryListModel::getCommit(int index) const
    {
        if (index < 0 || static_cast<std::size_t>(index) >= m_commits.size())
        {
            return {};
        }
        std::size_t idx = static_cast<std::size_t>(index);
        const auto& c = m_commits[idx];

        QVariantMap map;
        map[QStringLiteral("sha")] = QString::fromStdString(c.sha);
        map[QStringLiteral("shortSha")] = QString::fromStdString(c.shortSha);
        map[QStringLiteral("subject")] = QString::fromStdString(c.subject);
        map[QStringLiteral("body")] = QString::fromStdString(c.body);
        map[QStringLiteral("authorName")] = QString::fromStdString(c.authorName);
        map[QStringLiteral("authorEmail")] = QString::fromStdString(c.authorEmail);
        map[QStringLiteral("authorDate")] = QString::fromStdString(c.authorDateIso);
        map[QStringLiteral("authorDateFormatted")] = formatCommitDate(QString::fromStdString(c.authorDateIso));
        map[QStringLiteral("authorAvatarUrl")] = computeCommitAvatarUrl(c);
        map[QStringLiteral("authorInitials")] = computeInitials(QString::fromStdString(c.authorName));
        map[QStringLiteral("authorColor")] = computeAuthorColor(QString::fromStdString(c.authorName),
                                                                QString::fromStdString(c.authorEmail));

        map[QStringLiteral("coAuthors")] = computeCoAuthors(c);

        map[QStringLiteral("committerName")] = QString::fromStdString(c.committerName);
        map[QStringLiteral("committerEmail")] = QString::fromStdString(c.committerEmail);
        map[QStringLiteral("committerDateFormatted")] = formatCommitDate(QString::fromStdString(c.committerDateIso));
        map[QStringLiteral("committerAvatarUrl")] = computeAvatarUrl(QString::fromStdString(c.committerName),
                                                                     QString::fromStdString(c.committerEmail));
        map[QStringLiteral("committerInitials")] = computeInitials(QString::fromStdString(c.committerName));

        QStringList decList;
        for (const auto& d : c.refDecorations) decList << QString::fromStdString(d);
        map[QStringLiteral("decorations")] = decList;

        QStringList pList;
        for (const auto& p : c.parents) pList << QString::fromStdString(p);
        map[QStringLiteral("parents")] = pList;

        map[QStringLiteral("structuredDecorations")] = computeStructuredDecorations(c.refDecorations);

        const bool isStash = isStashCommit(c);
        if (idx < m_graphItems.size())
        {
            map[QStringLiteral("graphLane")] = m_graphItems[idx].lane;
            map[QStringLiteral("graphColor")] = m_graphItems[idx].color;
            map[QStringLiteral("graphRoutes")] = m_graphItems[idx].routes;
            map[QStringLiteral("isOrphan")] = m_graphItems[idx].isOrphan;
            map[QStringLiteral("isStash")] = m_graphItems[idx].isStash;
            map[QStringLiteral("stashSelector")] = m_graphItems[idx].stashSelector;
        }
        else
        {
            map[QStringLiteral("isStash")] = isStash;
            map[QStringLiteral("stashSelector")] = extractStashSelector(c.refDecorations);
        }
        map[QStringLiteral("isMerge")] = (!isStash && c.parents.size() > 1);
        map[QStringLiteral("isWip")] = (c.sha == "WIP");
        map[QStringLiteral("branchCommitCount")] = branchCommitCountForCommit(c);
        if (c.sha == "WIP")
        {
            QVariantMap wipDec;
            wipDec[QStringLiteral("name")] = QStringLiteral("WIP");
            wipDec[QStringLiteral("label")] = QStringLiteral("WIP");
            wipDec[QStringLiteral("bgColor")] = QStringLiteral("#78350F");
            wipDec[QStringLiteral("borderColor")] = QStringLiteral("#F59E0B");
            wipDec[QStringLiteral("fgColor")] = QStringLiteral("#FDE68A");
            map[QStringLiteral("structuredDecorations")] = QVariantList{wipDec};
        }
        return map;
    }

    void CommitHistoryListModel::rebuildGraph()
    {
        m_graphItems.clear();
        m_graphItems.resize(m_commits.size());
        m_maxLanes = 1;

        if (m_commits.empty())
        {
            emit maxLanesChanged();
            return;
        }

        std::unordered_set<std::string> knownShas;
        knownShas.reserve(m_commits.size());
        std::unordered_map<std::string, const git::bo::GitCommitLogItem*> commitMap;
        commitMap.reserve(m_commits.size());
        for (const auto& c : m_commits)
        {
            knownShas.insert(c.sha);
            commitMap[c.sha] = &c;
        }

        // 1. Identify HEAD commit and all ancestors reachable from HEAD
        std::string headSha;
        for (const auto& c : m_commits)
        {
            if (c.sha == "WIP")
            {
                if (!c.parents.empty())
                {
                    headSha = c.parents[0];
                    break;
                }
            }
            for (const auto& dec : c.refDecorations)
            {
                if (dec.starts_with("HEAD -> ") || dec == "HEAD")
                {
                    headSha = c.sha;
                    break;
                }
            }
            if (!headSha.empty())
            {
                break;
            }
        }

        if (headSha.empty() && !m_commits.empty())
        {
            for (const auto& c : m_commits)
            {
                if (isStashCommit(c))
                {
                    continue;
                }
                bool isPages = false;
                for (const auto& dec : c.refDecorations)
                {
                    if (dec.find("gh-pages") != std::string::npos)
                    {
                        isPages = true;
                        break;
                    }
                }
                if (!isPages)
                {
                    headSha = c.sha;
                    break;
                }
            }
            if (headSha.empty())
            {
                headSha = m_commits[0].sha;
            }
        }

        std::unordered_set<std::string> headReachable;
        std::vector<std::string> queue;
        if (!headSha.empty())
        {
            queue.push_back(headSha);
            headReachable.insert(headSha);
        }

        while (!queue.empty())
        {
            std::string curr = queue.back();
            queue.pop_back();

            auto it = commitMap.find(curr);
            if (it != commitMap.end())
            {
                for (const auto& p : it->second->parents)
                {
                    if (headReachable.insert(p).second)
                    {
                        queue.push_back(p);
                    }
                }
            }
        }

        // 2. Identify gh-pages / orphan commits
        std::unordered_set<std::string> orphanShas;
        for (const auto& c : m_commits)
        {
            for (const auto& dec : c.refDecorations)
            {
                if (dec.find("gh-pages") != std::string::npos)
                {
                    orphanShas.insert(c.sha);
                    break;
                }
            }
        }

        bool expanded = true;
        while (expanded)
        {
            expanded = false;
            for (const auto& c : m_commits)
            {
                if (orphanShas.count(c.sha) > 0)
                {
                    for (const auto& p : c.parents)
                    {
                        if (headReachable.count(p) == 0 && orphanShas.insert(p).second)
                        {
                            expanded = true;
                        }
                    }
                }
            }
        }

        auto isOrphanCommit = [&](const git::bo::GitCommitLogItem& c) -> bool
        {
            if (isStashCommit(c))
            {
                return false;
            }
            if (orphanShas.count(c.sha) > 0)
            {
                return true;
            }
            if (c.sha != "WIP" && headReachable.count(c.sha) == 0 && c.parents.empty() && c.sha != headSha)
            {
                return true;
            }
            return false;
        };

        bool hasOrphans = false;
        for (const auto& c : m_commits)
        {
            if (isOrphanCommit(c))
            {
                hasOrphans = true;
                break;
            }
        }

        int mainTrunkLane = hasOrphans ? 1 : 0;

        std::vector<std::string> activeLanes;
        std::vector<QString> laneColors;
        std::vector<bool> laneIsStash;

        auto ensureLaneSize = [&](int targetLane)
        {
            while (static_cast<int>(activeLanes.size()) <= targetLane)
            {
                const int newIdx = static_cast<int>(activeLanes.size());
                activeLanes.push_back("");
                laneColors.push_back(kGraphPalette[newIdx % kGraphPalette.size()]);
                laneIsStash.push_back(false);
            }
            while (static_cast<int>(laneColors.size()) <= targetLane)
            {
                const int newIdx = static_cast<int>(laneColors.size());
                laneColors.push_back(kGraphPalette[newIdx % kGraphPalette.size()]);
            }
            while (static_cast<int>(laneIsStash.size()) <= targetLane)
            {
                laneIsStash.push_back(false);
            }
        };

        if (hasOrphans)
        {
            activeLanes.resize(2, "");
            laneColors.resize(2);
            laneColors[0] = QStringLiteral("#A855F7"); // Purple for orphan lane (left of main trunk)
            laneColors[1] = QStringLiteral("#06B6D4"); // Cyan for main trunk
            laneIsStash.resize(2, false);
        }
        else
        {
            activeLanes.resize(1, "");
            laneColors.resize(1);
            laneColors[0] = QStringLiteral("#06B6D4"); // Cyan for main trunk
            laneIsStash.resize(1, false);
        }

        for (std::size_t i = 0; i < m_commits.size(); ++i)
        {
            const auto& commit = m_commits[i];
            const bool isStash = isStashCommit(commit);
            const QString stashSelector = isStash ? extractStashSelector(commit.refDecorations) : QString{};
            const bool isOrphan = !isStash && isOrphanCommit(commit);
            int commitLane = -1;

            // 1. Locate if commit is already expected in a non-stash lane of activeLanes
            if (!isStash)
            {
                for (int l = 0; l < static_cast<int>(activeLanes.size()); ++l)
                {
                    if (!laneIsStash[l] && activeLanes[l] == commit.sha)
                    {
                        if (isOrphan && l >= mainTrunkLane)
                        {
                            continue;
                        }
                        if (!isOrphan && l < mainTrunkLane)
                        {
                            continue;
                        }
                        commitLane = l;
                        break;
                    }
                }
            }

            const bool wasExpectedFromAbove = (commitLane != -1);
            if (commitLane == -1)
            {
                if (isStash)
                {
                    // Stash commits always occupy a side lane off the main trunk (GitKraken style)
                    for (int l = mainTrunkLane + 1; l < static_cast<int>(activeLanes.size()); ++l)
                    {
                        if (activeLanes[l].empty())
                        {
                            commitLane = l;
                            break;
                        }
                    }
                    if (commitLane == -1)
                    {
                        commitLane = std::max(mainTrunkLane + 1, static_cast<int>(activeLanes.size()));
                    }
                }
                else if (isOrphan)
                {
                    // Orphan commit: locate first free lane in [0, mainTrunkLane - 1] (i.e. lane 0 on the left)
                    for (int l = 0; l < mainTrunkLane; ++l)
                    {
                        if (activeLanes[l].empty())
                        {
                            commitLane = l;
                            break;
                        }
                    }
                    if (commitLane == -1)
                    {
                        commitLane = 0;
                    }
                }
                else
                {
                    // Main trunk or regular branch:
                    if ((commit.sha == headSha || headReachable.count(commit.sha) > 0 || commit.sha == "WIP") &&
                        activeLanes[mainTrunkLane].empty())
                    {
                        commitLane = mainTrunkLane;
                    }
                    else
                    {
                        for (int l = mainTrunkLane; l < static_cast<int>(activeLanes.size()); ++l)
                        {
                            if (activeLanes[l].empty())
                            {
                                commitLane = l;
                                break;
                            }
                        }
                        if (commitLane == -1)
                        {
                            commitLane = std::max(mainTrunkLane, static_cast<int>(activeLanes.size()));
                        }
                    }
                }
            }

            ensureLaneSize(commitLane);

            QString nodeColor = (commit.sha == "WIP" || isStash) ? QStringLiteral("#F59E0B") : laneColors[commitLane];
            QVariantList routes;

            // 2. Pass-through routes and stash lanes terminating onto this base commit
            for (int l = 0; l < static_cast<int>(activeLanes.size()); ++l)
            {
                if (l == commitLane || activeLanes[l].empty())
                {
                    continue;
                }
                if (laneIsStash[l] && activeLanes[l] == commit.sha)
                {
                    // Dashed stash connector curves directly into its base commit node
                    routes.append(makeGraphRoute(l, commitLane, QStringLiteral("#F59E0B"), 2, true));
                    activeLanes[l] = "";
                    laneIsStash[l] = false;
                }
                else
                {
                    const QString routeColor = laneIsStash[l] ? QStringLiteral("#F59E0B") : laneColors[l];
                    routes.append(makeGraphRoute(l, l, routeColor, 0, laneIsStash[l]));
                }
            }

            // 3. Inbound route from top into node
            if (wasExpectedFromAbove)
            {
                routes.append(makeGraphRoute(commitLane, commitLane, nodeColor, 2, false));
            }

            // 4. Outbound routes to parents
            activeLanes[commitLane] = "";
            laneIsStash[commitLane] = false;

            const auto& parents = commit.parents;
            if (isStash)
            {
                if (!parents.empty())
                {
                    const auto& baseSha = parents[0];
                    int baseLane = -1;
                    for (int l = 0; l < static_cast<int>(activeLanes.size()); ++l)
                    {
                        if (!laneIsStash[l] && activeLanes[l] == baseSha)
                        {
                            baseLane = l;
                            break;
                        }
                    }
                    if (baseLane != -1)
                    {
                        routes.append(makeGraphRoute(commitLane, baseLane, QStringLiteral("#F59E0B"), 1, true));
                    }
                    else if (knownShas.count(baseSha) > 0)
                    {
                        activeLanes[commitLane] = baseSha;
                        laneIsStash[commitLane] = true;
                        routes.append(makeGraphRoute(commitLane, commitLane, QStringLiteral("#F59E0B"), 1, true));
                    }
                    else
                    {
                        routes.append(makeGraphRoute(commitLane, commitLane, QStringLiteral("#F59E0B"), 1, true));
                    }
                }
            }
            else if (!parents.empty())
            {
                // First parent (never merge a real branch onto a stash lane)
                const auto& p0 = parents[0];
                if (knownShas.count(p0) > 0)
                {
                    int p0Lane = -1;
                    for (int l = 0; l < static_cast<int>(activeLanes.size()); ++l)
                    {
                        if (!laneIsStash[l] && activeLanes[l] == p0)
                        {
                            p0Lane = l;
                            break;
                        }
                    }
                    if (p0Lane != -1)
                    {
                        routes.append(makeGraphRoute(commitLane, p0Lane, nodeColor, 1, false));
                    }
                    else
                    {
                        activeLanes[commitLane] = p0;
                        laneIsStash[commitLane] = false;
                        routes.append(makeGraphRoute(commitLane, commitLane, nodeColor, 1, false));
                    }
                }
                else
                {
                    routes.append(makeGraphRoute(commitLane, commitLane, nodeColor, 1, false));
                }

                // Secondary parents (merge commits)
                for (std::size_t pIdx = 1; pIdx < parents.size(); ++pIdx)
                {
                    const auto& pOther = parents[pIdx];
                    if (knownShas.count(pOther) > 0)
                    {
                        int pOtherLane = -1;
                        for (int l = 0; l < static_cast<int>(activeLanes.size()); ++l)
                        {
                            if (!laneIsStash[l] && activeLanes[l] == pOther)
                            {
                                pOtherLane = l;
                                break;
                            }
                        }
                        if (pOtherLane == -1)
                        {
                            for (int l = mainTrunkLane; l < static_cast<int>(activeLanes.size()); ++l)
                            {
                                if (activeLanes[l].empty())
                                {
                                    pOtherLane = l;
                                    break;
                                }
                            }
                            if (pOtherLane == -1)
                            {
                                pOtherLane = std::max(mainTrunkLane, static_cast<int>(activeLanes.size()));
                                ensureLaneSize(pOtherLane);
                            }
                            activeLanes[pOtherLane] = pOther;
                            laneIsStash[pOtherLane] = false;
                        }

                        routes.append(makeGraphRoute(commitLane, pOtherLane, laneColors[pOtherLane], 1, false));
                    }
                }
            }

            // 5. Trim trailing empty lanes in activeLanes beyond mainTrunkLane
            while (activeLanes.size() > static_cast<std::size_t>(mainTrunkLane + 1) && activeLanes.back().empty())
            {
                activeLanes.pop_back();
                if (laneIsStash.size() > activeLanes.size())
                {
                    laneIsStash.pop_back();
                }
            }

            m_maxLanes = std::max(m_maxLanes, std::max(1, static_cast<int>(activeLanes.size())));
            m_maxLanes = std::max(m_maxLanes, commitLane + 1);

            m_graphItems[i] = GraphItem{
                .lane = commitLane,
                .color = nodeColor,
                .routes = std::move(routes),
                .isOrphan = isOrphan,
                .isStash = isStash,
                .stashSelector = stashSelector
            };
        }

        emit maxLanesChanged();
    }

    int CommitHistoryListModel::indexOfSha(const QString& sha) const
    {
        if (sha.isEmpty())
        {
            return -1;
        }
        const std::string needle = sha.toStdString();
        for (std::size_t i = 0; i < m_commits.size(); ++i)
        {
            if (m_commits[i].sha == needle)
            {
                return static_cast<int>(i);
            }
        }
        return -1;
    }

    void CommitHistoryListModel::setSearchFilter(const QString& filter)
    {
        QString trimmed = filter.trimmed();
        if (m_searchFilter == trimmed)
        {
            return;
        }
        beginResetModel();
        m_searchFilter = trimmed;
        applyWipAndRebuild();
        endResetModel();
        emit searchFilterChanged(m_searchFilter);
    }

    void CommitHistoryListModel::applyWipAndRebuild()
    {
        indexAvatarsFromCommits();
        m_commits.clear();

        if (m_searchFilter.isEmpty())
        {
            m_commits = m_rawCommits;
        }
        else
        {
            const QString lowerFilter = m_searchFilter.toLower();
            m_commits.reserve(m_rawCommits.size());

            for (const auto& c : m_rawCommits)
            {
                QString subj = QString::fromStdString(c.subject).toLower();
                QString author = QString::fromStdString(c.authorName).toLower();
                QString email = QString::fromStdString(c.authorEmail).toLower();
                QString sha = QString::fromStdString(c.sha).toLower();
                QString shortSha = QString::fromStdString(c.shortSha).toLower();

                if (subj.contains(lowerFilter) ||
                    author.contains(lowerFilter) ||
                    email.contains(lowerFilter) ||
                    sha.startsWith(lowerFilter) ||
                    shortSha.contains(lowerFilter) ||
                    (isStashCommit(c) && extractStashSelector(c.refDecorations).toLower().contains(lowerFilter)))
                {
                    m_commits.push_back(c);
                }
            }
        }

        // Strip Git's internal stash helper commits (index / untracked) and keep only
        // the base commit parent on stash commits so stashes render cleanly like GitKraken.
        {
            std::unordered_set<std::string> stashHelperShas;
            std::unordered_map<std::string, const git::bo::GitCommitLogItem*> currentMap;
            currentMap.reserve(m_commits.size());
            for (const auto& c : m_commits)
            {
                currentMap[c.sha] = &c;
            }
            for (const auto& c : m_commits)
            {
                if (isStashCommit(c) && c.parents.size() > 1)
                {
                    for (std::size_t p = 1; p < c.parents.size(); ++p)
                    {
                        auto it = currentMap.find(c.parents[p]);
                        if (it == currentMap.end() || it->second->refDecorations.empty())
                        {
                            stashHelperShas.insert(c.parents[p]);
                        }
                    }
                }
            }
            if (!stashHelperShas.empty())
            {
                std::erase_if(m_commits, [&stashHelperShas](const git::bo::GitCommitLogItem& c)
                {
                    return stashHelperShas.count(c.sha) > 0;
                });
            }
            for (auto& c : m_commits)
            {
                if (isStashCommit(c) && c.parents.size() > 1)
                {
                    c.parents.resize(1);
                }
            }
        }

        if (m_hasWip)
        {
            bool matchWip = m_searchFilter.isEmpty() ||
                QStringLiteral("wip").contains(m_searchFilter.toLower()) ||
                QStringLiteral("modifiche locali").contains(m_searchFilter.toLower());
            if (matchWip)
            {
                git::bo::GitCommitLogItem wip;
                wip.sha = "WIP";
                wip.shortSha = "WIP";
                wip.subject = QString("// WIP - %1 %2")
                              .arg(m_wipCount)
                              .arg(m_wipCount == 1
                                       ? QStringLiteral("file modificato")
                                       : QStringLiteral("file modificati"))
                              .toStdString();
                wip.authorName = "Modifiche locali";
                wip.authorEmail = "";
                wip.authorDateIso = QDateTime::currentDateTimeUtc().toString(Qt::ISODate).toStdString();
                wip.committerName = "Modifiche locali";
                wip.committerEmail = "";
                wip.committerDateIso = wip.authorDateIso;
                if (!m_headSha.isEmpty())
                {
                    wip.parents = {m_headSha.toStdString()};
                }
                else
                {
                    for (const auto& c : m_commits)
                    {
                        if (!isStashCommit(c))
                        {
                            wip.parents = {c.sha};
                            break;
                        }
                    }
                }
                m_commits.insert(m_commits.begin(), std::move(wip));
            }
        }
        rebuildGraph();
    }

    namespace
    {
        QMutex& avatarCacheMutex()
        {
            static QMutex s_mutex;
            return s_mutex;
        }

        QHash<QString, QString>& avatarMemoryCache()
        {
            static QHash<QString, QString> s_cache;
            static bool s_loaded = false;
            if (!s_loaded)
            {
                s_loaded = true;
                QSettings settings(QStringLiteral("GitFlux"), QStringLiteral("GitFlux"));
                settings.beginGroup(QStringLiteral("AvatarEmailCache"));
                const QStringList keys = settings.childKeys();
                for (const QString& key : keys)
                {
                    const QString val = settings.value(key).toString();
                    if (!val.isEmpty())
                    {
                        s_cache.insert(key, val);
                    }
                }
                settings.endGroup();
            }
            return s_cache;
        }
    } // namespace

    QString CommitHistoryListModel::cachedAvatarForEmail(const QString& rawEmail)
    {
        const QString email = rawEmail.trimmed().toLower();
        if (email.isEmpty() || email == QLatin1String("noreply@github.com"))
        {
            return {};
        }
        QMutexLocker locker(&avatarCacheMutex());
        return avatarMemoryCache().value(email);
    }

    void CommitHistoryListModel::cacheAvatarForEmail(const QString& rawEmail, const QString& avatarUrl)
    {
        const QString email = rawEmail.trimmed().toLower();
        if (email.isEmpty() || email == QLatin1String("noreply@github.com") || avatarUrl.isEmpty())
        {
            return;
        }
        QMutexLocker locker(&avatarCacheMutex());
        auto& cache = avatarMemoryCache();
        if (cache.value(email) == avatarUrl)
        {
            return;
        }
        cache.insert(email, avatarUrl);
        QSettings settings(QStringLiteral("GitFlux"), QStringLiteral("GitFlux"));
        settings.beginGroup(QStringLiteral("AvatarEmailCache"));
        settings.setValue(email, avatarUrl);
        settings.endGroup();
    }

    void CommitHistoryListModel::indexAvatarsFromCommits()
    {
        auto tryExtract = [](const QString& rawName, const QString& rawEmail) -> QString
        {
            const QString name = rawName.trimmed();
            const QString email = rawEmail.trimmed().toLower();
            if (email.endsWith(QLatin1String("users.noreply.github.com")))
            {
                const int atIdx = email.indexOf(QLatin1Char('@'));
                if (atIdx > 0)
                {
                    QString userPart = email.left(atIdx);
                    const int plusIdx = userPart.indexOf(QLatin1Char('+'));
                    if (plusIdx >= 0) userPart = userPart.mid(plusIdx + 1);
                    if (!userPart.isEmpty())
                    {
                        return QStringLiteral("https://github.com/%1.png?size=64").arg(userPart);
                    }
                }
            }
            if (!name.isEmpty() && !name.contains(QLatin1Char(' ')) &&
                name.compare(QLatin1String("WIP"), Qt::CaseInsensitive) != 0 &&
                name.compare(QLatin1String("GitHub"), Qt::CaseInsensitive) != 0)
            {
                return QStringLiteral("https://github.com/%1.png?size=64").arg(name);
            }
            return {};
        };

        for (const auto& c : m_rawCommits)
        {
            const QString authorEmail = QString::fromStdString(c.authorEmail).trimmed().toLower();
            const QString authorName = QString::fromStdString(c.authorName).trimmed();
            if (!authorEmail.isEmpty())
            {
                const QString url = tryExtract(authorName, authorEmail);
                if (!url.isEmpty())
                {
                    cacheAvatarForEmail(authorEmail, url);
                }
            }

            const QString committerEmail = QString::fromStdString(c.committerEmail).trimmed().toLower();
            const QString committerName = QString::fromStdString(c.committerName).trimmed();
            if (!committerEmail.isEmpty() && committerEmail != QLatin1String("noreply@github.com"))
            {
                const QString url = tryExtract(committerName, committerEmail);
                if (!url.isEmpty())
                {
                    cacheAvatarForEmail(committerEmail, url);
                }
            }
        }
    }

    void CommitHistoryListModel::resolveMissingAvatarsFromGitHub()
    {
        if (qEnvironmentVariable("QT_QPA_PLATFORM") == QLatin1String("offscreen"))
        {
            return;
        }

        int queuedCount = 0;
        for (const auto& c : m_rawCommits)
        {
            if (queuedCount >= 5)
            {
                break;
            }
            const QString email = QString::fromStdString(c.authorEmail).trimmed().toLower();
            if (email.isEmpty() ||
                email == QLatin1String("noreply@github.com") ||
                email.endsWith(QLatin1String("users.noreply.github.com")) ||
                !cachedAvatarForEmail(email).isEmpty() ||
                m_queriedEmails.contains(email))
            {
                continue;
            }

            m_queriedEmails.insert(email);
            ++queuedCount;

            const QString encodedEmail = QString::fromLatin1(QUrl::toPercentEncoding(email));
            const QUrl url(
                QStringLiteral("https://api.github.com/search/commits?q=author-email:%1&per_page=1").arg(encodedEmail));
            QNetworkRequest req(url);
            req.setRawHeader("Accept", "application/vnd.github+json");
            req.setRawHeader("User-Agent", "GitFlux-Desktop");
            req.setTransferTimeout(5000);

            QNetworkReply* reply = m_networkManager.get(req);
            connect(reply, &QNetworkReply::finished, this, [this, reply, email]()
            {
                reply->deleteLater();
                if (reply->error() != QNetworkReply::NoError)
                {
                    return;
                }
                const QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
                if (!doc.isObject())
                {
                    return;
                }
                const QJsonArray items = doc.object().value(QStringLiteral("items")).toArray();
                if (items.isEmpty())
                {
                    return;
                }
                const QJsonObject firstItem = items.at(0).toObject();
                const QJsonObject authorObj = firstItem.value(QStringLiteral("author")).toObject();
                QString login = authorObj.value(QStringLiteral("login")).toString().trimmed();
                QString avatarUrl = authorObj.value(QStringLiteral("avatar_url")).toString().trimmed();
                if (!login.isEmpty())
                {
                    avatarUrl = QStringLiteral("https://github.com/%1.png?size=64").arg(login);
                }
                if (!avatarUrl.isEmpty())
                {
                    cacheAvatarForEmail(email, avatarUrl);
                    if (rowCount() > 0)
                    {
                        emit dataChanged(index(0, 0), index(rowCount() - 1, 0),
                                         {AuthorAvatarUrlRole, CommitterAvatarUrlRole});
                    }
                }
            });
        }
    }

    void CommitHistoryListModel::computeBranchCommitCounts()
    {
        m_branchCommitCounts.clear();
        if (!m_isFullDag || m_rawCommits.empty())
        {
            return;
        }

        const std::size_t n = m_rawCommits.size();
        std::unordered_map<std::string_view, int> shaToIdx;
        shaToIdx.reserve(n);
        for (std::size_t i = 0; i < n; ++i)
        {
            const auto& c = m_rawCommits[i];
            if (!c.sha.empty() && c.sha != "WIP")
            {
                shaToIdx.try_emplace(c.sha, static_cast<int>(i));
            }
        }

        std::vector<std::vector<int>> parentsIdx(n);
        for (std::size_t i = 0; i < n; ++i)
        {
            const auto& parents = m_rawCommits[i].parents;
            parentsIdx[i].reserve(parents.size());
            for (const auto& p : parents)
            {
                if (auto it = shaToIdx.find(p); it != shaToIdx.end())
                {
                    parentsIdx[i].push_back(it->second);
                }
            }
        }

        // Reuse a single stamp array + DFS stack across all branch tips to avoid per-branch allocations.
        std::vector<std::uint32_t> visitStamp(n, 0);
        std::uint32_t currentStamp = 0;
        std::vector<int> stack;
        stack.reserve(std::min<std::size_t>(n, 256));

        for (std::size_t i = 0; i < n; ++i)
        {
            const auto& c = m_rawCommits[i];
            if (c.sha.empty() || c.sha == "WIP" || isStashCommit(c) || !hasBranchDecoration(c.refDecorations))
            {
                continue;
            }
            if (m_branchCommitCounts.contains(c.sha))
            {
                continue;
            }

            ++currentStamp;
            if (currentStamp == 0)
            {
                std::fill(visitStamp.begin(), visitStamp.end(), 0);
                currentStamp = 1;
            }

            const int rootIdx = static_cast<int>(i);
            visitStamp[rootIdx] = currentStamp;
            stack.clear();
            stack.push_back(rootIdx);
            int count = 1;

            while (!stack.empty())
            {
                const int u = stack.back();
                stack.pop_back();
                for (const int pIdx : parentsIdx[u])
                {
                    if (visitStamp[pIdx] != currentStamp)
                    {
                        visitStamp[pIdx] = currentStamp;
                        ++count;
                        stack.push_back(pIdx);
                    }
                }
            }

            m_branchCommitCounts.emplace(c.sha, count);
        }
    }

    int CommitHistoryListModel::branchCommitCountForCommit(const git::bo::GitCommitLogItem& commit) const noexcept
    {
        if (!m_isFullDag || commit.sha.empty() || commit.sha == "WIP" || isStashCommit(commit))
        {
            return 0;
        }
        if (auto it = m_branchCommitCounts.find(commit.sha); it != m_branchCommitCounts.end())
        {
            return it->second;
        }
        return 0;
    }

    void CommitHistoryListModel::setCommits(std::vector<gf::git::bo::GitCommitLogItem> commits, bool isFullDag)
    {
        beginResetModel();
        m_isFullDag = isFullDag;
        m_rawCommits = std::move(commits);
        m_headSha = findHeadSha(m_rawCommits);
        computeBranchCommitCounts();
        applyWipAndRebuild();
        endResetModel();
        indexAvatarsFromCommits();
        emit totalCommitsCountChanged();
        resolveMissingAvatarsFromGitHub();
    }

    void CommitHistoryListModel::setWip(bool hasWip, int changedFilesCount)
    {
        if (m_hasWip == hasWip && m_wipCount == changedFilesCount)
        {
            return;
        }
        if (m_hasWip&& hasWip &&!m_commits.empty() && m_commits[0].sha == "WIP")
        {
            m_wipCount = changedFilesCount;
            m_commits[0].subject = QString("// WIP - %1 %2")
                                   .arg(m_wipCount)
                                   .arg(m_wipCount == 1
                                            ? QStringLiteral("file modificato")
                                            : QStringLiteral("file modificati"))
                                   .toStdString();
            emit dataChanged(index(0, 0), index(0, 0), {SubjectRole});
            return;
        }
        if (m_hasWip == hasWip && m_wipCount == changedFilesCount)
        {
            return;
        }
        beginResetModel();
        m_hasWip = hasWip;
        m_wipCount = changedFilesCount;
        applyWipAndRebuild();
        endResetModel();
    }
} // namespace gf::desktop
