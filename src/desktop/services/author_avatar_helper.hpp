#pragma once

#include <QString>
#include <QStringList>

namespace gf::desktop
{
    /// Shared helpers for deterministic author palette colors, initials, and GitHub/Gravatar URLs.
    class AuthorAvatarHelper final
    {
    public:
        [[nodiscard]] static const QStringList& palette();

        [[nodiscard]] static QString cachedAvatarForEmail(const QString& rawEmail);
        static void cacheAvatarForEmail(const QString& rawEmail, const QString& avatarUrl);

        [[nodiscard]] static QString computeAvatarUrl(const QString& name, const QString& rawEmail);
        [[nodiscard]] static QString computeCommitAvatarUrl(const QString& authorName,
                                                            const QString& authorEmail,
                                                            const QString& committerName,
                                                            const QString& committerEmail);

        [[nodiscard]] static QString computeInitials(const QString& name);
        [[nodiscard]] static QString computeColorForKey(const QString& key);
        [[nodiscard]] static QString computeAuthorColor(const QString& name, const QString& email);
    };
} // namespace gf::desktop
