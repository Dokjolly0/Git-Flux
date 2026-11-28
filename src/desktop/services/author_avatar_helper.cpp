#include "desktop/services/author_avatar_helper.hpp"

#include <QCryptographicHash>
#include <QHash>
#include <QMutex>
#include <QMutexLocker>
#include <QSettings>

namespace gf::desktop
{
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
                for (const QString& key : settings.childKeys())
                {
                    if (const QString val = settings.value(key).toString(); !val.isEmpty())
                    {
                        s_cache.insert(key, val);
                    }
                }
                settings.endGroup();
            }
            return s_cache;
        }

        QString extractGitHubNoReplyUser(const QString& normalizedEmail)
        {
            if (!normalizedEmail.endsWith(QLatin1String("users.noreply.github.com")))
            {
                return {};
            }
            const int atIdx = normalizedEmail.indexOf(QLatin1Char('@'));
            if (atIdx <= 0)
            {
                return {};
            }
            QString userPart = normalizedEmail.left(atIdx);
            if (const int plusIdx = userPart.indexOf(QLatin1Char('+')); plusIdx >= 0)
            {
                userPart = userPart.mid(plusIdx + 1);
            }
            return userPart;
        }

        QString formatGitHubAvatarUrl(const QString& username)
        {
            return QStringLiteral("https://github.com/%1.png?size=64").arg(username);
        }

        QString formatGravatarUrl(const QString& normalizedEmail)
        {
            if (normalizedEmail.isEmpty())
            {
                return QStringLiteral("https://www.gravatar.com/avatar/?d=mp");
            }
            const QByteArray hash = QCryptographicHash::hash(
                normalizedEmail.toUtf8(), QCryptographicHash::Md5).toHex();
            return QStringLiteral("https://www.gravatar.com/avatar/%1?s=64&d=identicon")
                .arg(QString::fromLatin1(hash));
        }

        QString resolveSingleIdentityAvatar(const QString& cleanName, const QString& normalizedEmail)
        {
            if (!cleanName.isEmpty() && !cleanName.contains(QLatin1Char(' ')) &&
                cleanName.compare(QLatin1String("GitHub"), Qt::CaseInsensitive) != 0)
            {
                return formatGitHubAvatarUrl(cleanName);
            }
            if (const QString ghUser = extractGitHubNoReplyUser(normalizedEmail); !ghUser.isEmpty())
            {
                return formatGitHubAvatarUrl(ghUser);
            }
            if (const QString cached = AuthorAvatarHelper::cachedAvatarForEmail(normalizedEmail);
                !cached.isEmpty())
            {
                return cached;
            }
            return {};
        }
    } // namespace

    const QStringList& AuthorAvatarHelper::palette()
    {
        static const QStringList kPalette = {
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
        return kPalette;
    }

    QString AuthorAvatarHelper::cachedAvatarForEmail(const QString& rawEmail)
    {
        const QString email = rawEmail.trimmed().toLower();
        if (email.isEmpty() || email == QLatin1String("noreply@github.com"))
        {
            return {};
        }
        QMutexLocker locker(&avatarCacheMutex());
        return avatarMemoryCache().value(email);
    }

    void AuthorAvatarHelper::cacheAvatarForEmail(const QString& rawEmail, const QString& avatarUrl)
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

    QString AuthorAvatarHelper::computeAvatarUrl(const QString& name, const QString& rawEmail)
    {
        const QString email = rawEmail.trimmed().toLower();
        const QString cleanName = name.trimmed();

        if (const QString ghUser = extractGitHubNoReplyUser(email); !ghUser.isEmpty())
        {
            return formatGitHubAvatarUrl(ghUser);
        }
        if (!cleanName.isEmpty() && !cleanName.contains(QLatin1Char(' ')) &&
            cleanName.compare(QLatin1String("GitHub"), Qt::CaseInsensitive) != 0)
        {
            return formatGitHubAvatarUrl(cleanName);
        }
        if (const QString cached = cachedAvatarForEmail(email); !cached.isEmpty())
        {
            return cached;
        }
        return formatGravatarUrl(email);
    }

    QString AuthorAvatarHelper::computeCommitAvatarUrl(const QString& authorName,
                                                         const QString& authorEmail,
                                                         const QString& committerName,
                                                         const QString& committerEmail)
    {
        const QString cleanAuthor = authorName.trimmed();
        const QString normAuthorEmail = authorEmail.trimmed().toLower();
        if (const QString url = resolveSingleIdentityAvatar(cleanAuthor, normAuthorEmail); !url.isEmpty())
        {
            return url;
        }

        const QString cleanCommitter = committerName.trimmed();
        const QString normCommitterEmail = committerEmail.trimmed().toLower();
        if (const QString url = resolveSingleIdentityAvatar(cleanCommitter, normCommitterEmail); !url.isEmpty())
        {
            return url;
        }

        const QString emailForGravatar = !normAuthorEmail.isEmpty() ? normAuthorEmail : normCommitterEmail;
        return formatGravatarUrl(emailForGravatar);
    }

    QString AuthorAvatarHelper::computeInitials(const QString& name)
    {
        const QString clean = name.trimmed();
        if (clean.isEmpty())
        {
            return QStringLiteral("??");
        }
        const QStringList parts = clean.split(QLatin1Char(' '), Qt::SkipEmptyParts);
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

    QString AuthorAvatarHelper::computeColorForKey(const QString& key)
    {
        const auto& colors = palette();
        const std::size_t hash = qHash(key);
        const int idx = static_cast<int>(hash % static_cast<std::size_t>(colors.size()));
        return colors[idx];
    }

    QString AuthorAvatarHelper::computeAuthorColor(const QString& name, const QString& email)
    {
        return computeColorForKey(email.isEmpty() ? name : email);
    }
} // namespace gf::desktop
