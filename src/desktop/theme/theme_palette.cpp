#include "desktop/theme/theme_palette.hpp"

#include "core/shared/accent_palette.hpp"

#include <array>

namespace gf::desktop
{
    namespace
    {
        struct ColorTokenEntry
        {
            QLatin1StringView key;
            QColor ThemeDefinition::*member;
        };

        constexpr std::array kColorTokens = {
            ColorTokenEntry{QLatin1StringView("canvas"), &ThemeDefinition::canvas},
            ColorTokenEntry{QLatin1StringView("surface"), &ThemeDefinition::surface},
            ColorTokenEntry{QLatin1StringView("surfaceElevated"), &ThemeDefinition::surfaceElevated},
            ColorTokenEntry{QLatin1StringView("surfaceGlass"), &ThemeDefinition::surfaceGlass},
            ColorTokenEntry{QLatin1StringView("card"), &ThemeDefinition::card},
            ColorTokenEntry{QLatin1StringView("cardHover"), &ThemeDefinition::cardHover},
            ColorTokenEntry{QLatin1StringView("inputBg"), &ThemeDefinition::inputBg},
            ColorTokenEntry{QLatin1StringView("headerBg"), &ThemeDefinition::headerBg},
            ColorTokenEntry{QLatin1StringView("footerBg"), &ThemeDefinition::footerBg},
            ColorTokenEntry{QLatin1StringView("border"), &ThemeDefinition::border},
            ColorTokenEntry{QLatin1StringView("borderSubtle"), &ThemeDefinition::borderSubtle},
            ColorTokenEntry{QLatin1StringView("borderHighlight"), &ThemeDefinition::borderHighlight},
            ColorTokenEntry{QLatin1StringView("borderFocus"), &ThemeDefinition::borderFocus},
            ColorTokenEntry{QLatin1StringView("accentCyan"), &ThemeDefinition::accentCyan},
            ColorTokenEntry{QLatin1StringView("accentBlue"), &ThemeDefinition::accentBlue},
            ColorTokenEntry{QLatin1StringView("accentBlueHover"), &ThemeDefinition::accentBlueHover},
            ColorTokenEntry{QLatin1StringView("accentPurple"), &ThemeDefinition::accentPurple},
            ColorTokenEntry{QLatin1StringView("accentTeal"), &ThemeDefinition::accentTeal},
            ColorTokenEntry{QLatin1StringView("defaultTabAccent"), &ThemeDefinition::defaultTabAccent},
            ColorTokenEntry{QLatin1StringView("textPrimary"), &ThemeDefinition::textPrimary},
            ColorTokenEntry{QLatin1StringView("textSecondary"), &ThemeDefinition::textSecondary},
            ColorTokenEntry{QLatin1StringView("textMuted"), &ThemeDefinition::textMuted},
            ColorTokenEntry{QLatin1StringView("textPlaceholder"), &ThemeDefinition::textPlaceholder},
            ColorTokenEntry{QLatin1StringView("danger"), &ThemeDefinition::danger},
            ColorTokenEntry{QLatin1StringView("dangerBg"), &ThemeDefinition::dangerBg},
            ColorTokenEntry{QLatin1StringView("dangerText"), &ThemeDefinition::dangerText},
            ColorTokenEntry{QLatin1StringView("success"), &ThemeDefinition::success},
            ColorTokenEntry{QLatin1StringView("successBg"), &ThemeDefinition::successBg},
            ColorTokenEntry{QLatin1StringView("successText"), &ThemeDefinition::successText},
            ColorTokenEntry{QLatin1StringView("warning"), &ThemeDefinition::warning},
            ColorTokenEntry{QLatin1StringView("warningBg"), &ThemeDefinition::warningBg},
            ColorTokenEntry{QLatin1StringView("warningText"), &ThemeDefinition::warningText},
            ColorTokenEntry{QLatin1StringView("info"), &ThemeDefinition::info},
            ColorTokenEntry{QLatin1StringView("infoBg"), &ThemeDefinition::infoBg},
            ColorTokenEntry{QLatin1StringView("infoText"), &ThemeDefinition::infoText},
            ColorTokenEntry{QLatin1StringView("selectionBg"), &ThemeDefinition::selectionBg},
            ColorTokenEntry{QLatin1StringView("selectionText"), &ThemeDefinition::selectionText},
        };

        [[nodiscard]] QColor defaultAccentQColor()
        {
            return QColor(QString::fromUtf8(gf::core::kDefaultAccentColor.data(),
                                            static_cast<qsizetype>(gf::core::kDefaultAccentColor.size())));
        }
    } // namespace

    QPalette ThemeDefinition::toQPalette() const
    {
        QPalette pal;
        pal.setColor(QPalette::Window, surface);
        pal.setColor(QPalette::WindowText, textPrimary);
        pal.setColor(QPalette::Base, inputBg);
        pal.setColor(QPalette::AlternateBase, surfaceElevated);
        pal.setColor(QPalette::ToolTipBase, headerBg);
        pal.setColor(QPalette::ToolTipText, textPrimary);
        pal.setColor(QPalette::Text, textPrimary);
        pal.setColor(QPalette::Button, card);
        pal.setColor(QPalette::ButtonText, textPrimary);
        pal.setColor(QPalette::BrightText, accentCyan);
        pal.setColor(QPalette::Highlight, accentBlue);
        pal.setColor(QPalette::HighlightedText, selectionText);
        pal.setColor(QPalette::Dark, borderHighlight);
        pal.setColor(QPalette::Mid, border);
        pal.setColor(QPalette::Midlight, cardHover);
        pal.setColor(QPalette::Light, surfaceGlass);
        pal.setColor(QPalette::Shadow, QColor(0x00, 0x00, 0x00));
        return pal;
    }

    ThemeDefinition ThemeManager::darkPreset()
    {
        ThemeDefinition t;
        t.id = QStringLiteral("dark");
        t.displayName = QStringLiteral("Flux Dark");
        t.isDark = true;
        t.isCustom = false;

        t.canvas = QColor(QStringLiteral("#080B10"));
        t.surface = QColor(QStringLiteral("#0F141C"));
        t.surfaceElevated = QColor(QStringLiteral("#141C28"));
        t.surfaceGlass = QColor(QStringLiteral("#182232"));
        t.card = QColor(QStringLiteral("#161F2E"));
        t.cardHover = QColor(QStringLiteral("#1E2B3E"));
        t.inputBg = QColor(QStringLiteral("#0A0E15"));
        t.headerBg = QColor(QStringLiteral("#0A0E15"));
        t.footerBg = QColor(QStringLiteral("#0C121C"));

        t.border = QColor(QStringLiteral("#222F42"));
        t.borderSubtle = QColor(QStringLiteral("#1A2434"));
        t.borderHighlight = QColor(QStringLiteral("#2D3F59"));
        t.borderFocus = QColor(QStringLiteral("#38BDF8"));

        t.accentCyan = QColor(QStringLiteral("#00E5FF"));
        t.accentBlue = QColor(QStringLiteral("#0284C7"));
        t.accentBlueHover = QColor(QStringLiteral("#0369A1"));
        t.accentPurple = QColor(QStringLiteral("#7C4DFF"));
        t.accentTeal = QColor(QStringLiteral("#00B4D8"));
        t.defaultTabAccent = defaultAccentQColor();

        t.textPrimary = QColor(QStringLiteral("#F8FAFC"));
        t.textSecondary = QColor(QStringLiteral("#94A3B8"));
        t.textMuted = QColor(QStringLiteral("#64748B"));
        t.textPlaceholder = QColor(QStringLiteral("#475569"));

        t.danger = QColor(QStringLiteral("#EF4444"));
        t.dangerBg = QColor(QStringLiteral("#3B1219"));
        t.dangerText = QColor(QStringLiteral("#FCA5A5"));
        t.success = QColor(QStringLiteral("#10B981"));
        t.successBg = QColor(QStringLiteral("#082F25"));
        t.successText = QColor(QStringLiteral("#34D399"));
        t.warning = QColor(QStringLiteral("#F59E0B"));
        t.warningBg = QColor(QStringLiteral("#2A1B07"));
        t.warningText = QColor(QStringLiteral("#FBBF24"));
        t.info = QColor(QStringLiteral("#38BDF8"));
        t.infoBg = QColor(QStringLiteral("#0F2238"));
        t.infoText = QColor(QStringLiteral("#7DD3FC"));

        t.selectionBg = QColor(QStringLiteral("#1D4ED8"));
        t.selectionText = QColor(QStringLiteral("#FFFFFF"));
        return t;
    }

    ThemeDefinition ThemeManager::lightPreset()
    {
        ThemeDefinition t;
        t.id = QStringLiteral("light");
        t.displayName = QStringLiteral("Flux Light");
        t.isDark = false;
        t.isCustom = false;

        t.canvas = QColor(QStringLiteral("#F8FAFC"));
        t.surface = QColor(QStringLiteral("#FFFFFF"));
        t.surfaceElevated = QColor(QStringLiteral("#F1F5F9"));
        t.surfaceGlass = QColor(QStringLiteral("#E2E8F0"));
        t.card = QColor(QStringLiteral("#FFFFFF"));
        t.cardHover = QColor(QStringLiteral("#F1F5F9"));
        t.inputBg = QColor(QStringLiteral("#FFFFFF"));
        t.headerBg = QColor(QStringLiteral("#F1F5F9"));
        t.footerBg = QColor(QStringLiteral("#F8FAFC"));

        t.border = QColor(QStringLiteral("#CBD5E1"));
        t.borderSubtle = QColor(QStringLiteral("#E2E8F0"));
        t.borderHighlight = QColor(QStringLiteral("#94A3B8"));
        t.borderFocus = QColor(QStringLiteral("#0284C7"));

        t.accentCyan = QColor(QStringLiteral("#0284C7"));
        t.accentBlue = QColor(QStringLiteral("#0284C7"));
        t.accentBlueHover = QColor(QStringLiteral("#0369A1"));
        t.accentPurple = QColor(QStringLiteral("#6D28D9"));
        t.accentTeal = QColor(QStringLiteral("#0D9488"));
        t.defaultTabAccent = QColor(QStringLiteral("#0284C7"));

        t.textPrimary = QColor(QStringLiteral("#0F172A"));
        t.textSecondary = QColor(QStringLiteral("#475569"));
        t.textMuted = QColor(QStringLiteral("#64748B"));
        t.textPlaceholder = QColor(QStringLiteral("#94A3B8"));

        t.danger = QColor(QStringLiteral("#DC2626"));
        t.dangerBg = QColor(QStringLiteral("#FEE2E2"));
        t.dangerText = QColor(QStringLiteral("#991B1B"));
        t.success = QColor(QStringLiteral("#059669"));
        t.successBg = QColor(QStringLiteral("#D1FAE5"));
        t.successText = QColor(QStringLiteral("#065F46"));
        t.warning = QColor(QStringLiteral("#D97706"));
        t.warningBg = QColor(QStringLiteral("#FEF3C7"));
        t.warningText = QColor(QStringLiteral("#92400E"));
        t.info = QColor(QStringLiteral("#0284C7"));
        t.infoBg = QColor(QStringLiteral("#E0F2FE"));
        t.infoText = QColor(QStringLiteral("#075985"));

        t.selectionBg = QColor(QStringLiteral("#2563EB"));
        t.selectionText = QColor(QStringLiteral("#FFFFFF"));
        return t;
    }

    ThemeManager::ThemeManager(QObject* parent)
        : QObject(parent)
    {
        m_themes.push_back(darkPreset());
        m_themes.push_back(lightPreset());
        m_activeIndex = 0;
    }

    const ThemeDefinition& ThemeManager::currentTheme() const noexcept
    {
        return m_themes[static_cast<std::size_t>(m_activeIndex)];
    }

    QPalette ThemeManager::currentQPalette() const
    {
        return currentTheme().toQPalette();
    }

    QString ThemeManager::activeThemeId() const
    {
        return currentTheme().id;
    }

    void ThemeManager::setActiveThemeId(const QString& themeId)
    {
        (void)setActiveTheme(themeId);
    }

    QString ThemeManager::activeThemeName() const
    {
        return currentTheme().displayName;
    }

    bool ThemeManager::isDark() const noexcept
    {
        return currentTheme().isDark;
    }

    QVariantList ThemeManager::availableThemes() const
    {
        QVariantList out;
        out.reserve(static_cast<qsizetype>(m_themes.size()));
        for (const auto& t : m_themes)
        {
            QVariantMap m;
            m[QStringLiteral("id")] = t.id;
            m[QStringLiteral("displayName")] = t.displayName;
            m[QStringLiteral("isDark")] = t.isDark;
            m[QStringLiteral("isCustom")] = t.isCustom;
            out.append(m);
        }
        return out;
    }

    QStringList ThemeManager::accentPalette() const
    {
        QStringList out;
        out.reserve(static_cast<qsizetype>(gf::core::kAccentPalette.size()));
        for (const std::string_view hex : gf::core::kAccentPalette)
        {
            out.append(QString::fromUtf8(hex.data(), static_cast<qsizetype>(hex.size())));
        }
        return out;
    }

    int ThemeManager::findThemeIndex(const QString& themeId) const noexcept
    {
        const QString trimmed = themeId.trimmed();
        for (int i = 0; i < static_cast<int>(m_themes.size()); ++i)
        {
            if (m_themes[static_cast<std::size_t>(i)].id.compare(trimmed, Qt::CaseInsensitive) == 0)
            {
                return i;
            }
        }
        return -1;
    }

    bool ThemeManager::setActiveTheme(const QString& themeId)
    {
        const int idx = findThemeIndex(themeId);
        if (idx < 0)
        {
            return false;
        }
        if (idx == m_activeIndex)
        {
            return true;
        }
        m_activeIndex = idx;
        emit themeChanged();
        return true;
    }

    bool ThemeManager::registerCustomTheme(const QVariantMap& definition)
    {
        const QString id = definition.value(QStringLiteral("id")).toString().trimmed();
        if (id.isEmpty() ||
            id.compare(QStringLiteral("dark"), Qt::CaseInsensitive) == 0 ||
            id.compare(QStringLiteral("light"), Qt::CaseInsensitive) == 0)
        {
            return false;
        }

        const QString baseId = definition.value(QStringLiteral("baseTheme")).toString().trimmed();
        const int baseIdx = baseId.isEmpty() ? 0 : findThemeIndex(baseId);
        ThemeDefinition custom = (baseIdx >= 0) ? m_themes[static_cast<std::size_t>(baseIdx)] : darkPreset();

        custom.id = id;
        const QString name = definition.value(QStringLiteral("displayName")).toString().trimmed();
        custom.displayName = name.isEmpty() ? id : name;
        if (definition.contains(QStringLiteral("isDark")))
        {
            custom.isDark = definition.value(QStringLiteral("isDark")).toBool();
        }
        custom.isCustom = true;

        for (const auto& entry : kColorTokens)
        {
            const QString keyStr(entry.key);
            if (!definition.contains(keyStr))
            {
                continue;
            }
            const QVariant rawVal = definition.value(keyStr);
            const QColor parsed = rawVal.canConvert<QColor>() ? rawVal.value<QColor>() : QColor(rawVal.toString().trimmed());
            if (parsed.isValid())
            {
                custom.*(entry.member) = parsed;
            }
        }

        const int existingIdx = findThemeIndex(id);
        if (existingIdx >= 0)
        {
            m_themes[static_cast<std::size_t>(existingIdx)] = std::move(custom);
            emit availableThemesChanged();
            if (existingIdx == m_activeIndex)
            {
                emit themeChanged();
            }
            return true;
        }

        m_themes.push_back(std::move(custom));
        emit availableThemesChanged();
        return true;
    }

    bool ThemeManager::removeCustomTheme(const QString& themeId)
    {
        const int idx = findThemeIndex(themeId);
        if (idx < 0 || !m_themes[static_cast<std::size_t>(idx)].isCustom)
        {
            return false;
        }

        const bool removingActive = (idx == m_activeIndex);
        m_themes.erase(m_themes.begin() + idx);
        if (removingActive)
        {
            m_activeIndex = 0; // Fall back to built-in dark theme
        }
        else if (idx < m_activeIndex)
        {
            --m_activeIndex;
        }
        emit availableThemesChanged();
        if (removingActive)
        {
            emit themeChanged();
        }
        return true;
    }

    QVariantMap ThemeManager::exportTheme(const QString& themeId) const
    {
        const int idx = themeId.trimmed().isEmpty() ? m_activeIndex : findThemeIndex(themeId);
        if (idx < 0)
        {
            return {};
        }

        const auto& t = m_themes[static_cast<std::size_t>(idx)];
        QVariantMap out;
        out[QStringLiteral("id")] = t.id;
        out[QStringLiteral("displayName")] = t.displayName;
        out[QStringLiteral("isDark")] = t.isDark;
        out[QStringLiteral("isCustom")] = t.isCustom;
        for (const auto& entry : kColorTokens)
        {
            out[QString(entry.key)] = (t.*(entry.member)).name(QColor::HexRgb);
        }
        return out;
    }
} // namespace gf::desktop
