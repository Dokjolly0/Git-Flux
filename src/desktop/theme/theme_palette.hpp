#pragma once

#include <QColor>
#include <QObject>
#include <QPalette>
#include <QString>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>

#include <vector>

namespace gf::desktop
{
    /// Complete color palette definition for a GitFlux UI theme (dark, light, or custom).
    struct ThemeDefinition
    {
        QString id;
        QString displayName;
        bool isDark = true;
        bool isCustom = false;

        // Surfaces & Containers
        QColor canvas;
        QColor surface;
        QColor surfaceElevated;
        QColor surfaceGlass;
        QColor card;
        QColor cardHover;
        QColor inputBg;
        QColor headerBg;
        QColor footerBg;

        // Borders
        QColor border;
        QColor borderSubtle;
        QColor borderHighlight;
        QColor borderFocus;

        // Brand & Tab Accents
        QColor accentCyan;
        QColor accentBlue;
        QColor accentBlueHover;
        QColor accentPurple;
        QColor accentTeal;
        QColor defaultTabAccent;

        // Typography
        QColor textPrimary;
        QColor textSecondary;
        QColor textMuted;
        QColor textPlaceholder;

        // Semantic States
        QColor danger;
        QColor dangerBg;
        QColor dangerText;
        QColor success;
        QColor successBg;
        QColor successText;
        QColor warning;
        QColor warningBg;
        QColor warningText;
        QColor info;
        QColor infoBg;
        QColor infoText;

        // Selection & Scrollbars
        QColor selectionBg;
        QColor selectionText;

        /// Builds a Qt application QPalette synchronized with this theme definition.
        [[nodiscard]] QPalette toQPalette() const;
    };

    /// Shared Qt theme & color palette manager exposed to C++ and QML (`gfTheme`).
    /// Supports built-in Dark and Light presets plus runtime-registered custom themes.
    class ThemeManager final : public QObject
    {
        Q_OBJECT

        Q_PROPERTY(QString activeThemeId READ activeThemeId WRITE setActiveThemeId NOTIFY themeChanged)
        Q_PROPERTY(QString activeThemeName READ activeThemeName NOTIFY themeChanged)
        Q_PROPERTY(bool isDark READ isDark NOTIFY themeChanged)
        Q_PROPERTY(QVariantList availableThemes READ availableThemes NOTIFY availableThemesChanged)
        Q_PROPERTY(QStringList accentPalette READ accentPalette CONSTANT)

        // Surfaces
        Q_PROPERTY(QColor canvas READ canvas NOTIFY themeChanged)
        Q_PROPERTY(QColor surface READ surface NOTIFY themeChanged)
        Q_PROPERTY(QColor surfaceElevated READ surfaceElevated NOTIFY themeChanged)
        Q_PROPERTY(QColor surfaceGlass READ surfaceGlass NOTIFY themeChanged)
        Q_PROPERTY(QColor card READ card NOTIFY themeChanged)
        Q_PROPERTY(QColor cardHover READ cardHover NOTIFY themeChanged)
        Q_PROPERTY(QColor inputBg READ inputBg NOTIFY themeChanged)
        Q_PROPERTY(QColor headerBg READ headerBg NOTIFY themeChanged)
        Q_PROPERTY(QColor footerBg READ footerBg NOTIFY themeChanged)

        // Borders
        Q_PROPERTY(QColor border READ border NOTIFY themeChanged)
        Q_PROPERTY(QColor borderSubtle READ borderSubtle NOTIFY themeChanged)
        Q_PROPERTY(QColor borderHighlight READ borderHighlight NOTIFY themeChanged)
        Q_PROPERTY(QColor borderFocus READ borderFocus NOTIFY themeChanged)

        // Accents
        Q_PROPERTY(QColor accentCyan READ accentCyan NOTIFY themeChanged)
        Q_PROPERTY(QColor accentBlue READ accentBlue NOTIFY themeChanged)
        Q_PROPERTY(QColor accentBlueHover READ accentBlueHover NOTIFY themeChanged)
        Q_PROPERTY(QColor accentPurple READ accentPurple NOTIFY themeChanged)
        Q_PROPERTY(QColor accentTeal READ accentTeal NOTIFY themeChanged)
        Q_PROPERTY(QColor defaultTabAccent READ defaultTabAccent NOTIFY themeChanged)

        // Typography
        Q_PROPERTY(QColor textPrimary READ textPrimary NOTIFY themeChanged)
        Q_PROPERTY(QColor textSecondary READ textSecondary NOTIFY themeChanged)
        Q_PROPERTY(QColor textMuted READ textMuted NOTIFY themeChanged)
        Q_PROPERTY(QColor textPlaceholder READ textPlaceholder NOTIFY themeChanged)

        // Semantic States
        Q_PROPERTY(QColor danger READ danger NOTIFY themeChanged)
        Q_PROPERTY(QColor dangerBg READ dangerBg NOTIFY themeChanged)
        Q_PROPERTY(QColor dangerText READ dangerText NOTIFY themeChanged)
        Q_PROPERTY(QColor success READ success NOTIFY themeChanged)
        Q_PROPERTY(QColor successBg READ successBg NOTIFY themeChanged)
        Q_PROPERTY(QColor successText READ successText NOTIFY themeChanged)
        Q_PROPERTY(QColor warning READ warning NOTIFY themeChanged)
        Q_PROPERTY(QColor warningBg READ warningBg NOTIFY themeChanged)
        Q_PROPERTY(QColor warningText READ warningText NOTIFY themeChanged)
        Q_PROPERTY(QColor info READ info NOTIFY themeChanged)
        Q_PROPERTY(QColor infoBg READ infoBg NOTIFY themeChanged)
        Q_PROPERTY(QColor infoText READ infoText NOTIFY themeChanged)

        // Selection
        Q_PROPERTY(QColor selectionBg READ selectionBg NOTIFY themeChanged)
        Q_PROPERTY(QColor selectionText READ selectionText NOTIFY themeChanged)

        // Design-system metrics matching FluxTheme
        Q_PROPERTY(int radiusXs READ radiusXs CONSTANT)
        Q_PROPERTY(int radiusSm READ radiusSm CONSTANT)
        Q_PROPERTY(int radiusMd READ radiusMd CONSTANT)
        Q_PROPERTY(int radiusLg READ radiusLg CONSTANT)
        Q_PROPERTY(int radiusXl READ radiusXl CONSTANT)
        Q_PROPERTY(int radiusPill READ radiusPill CONSTANT)
        Q_PROPERTY(int spacingXs READ spacingXs CONSTANT)
        Q_PROPERTY(int spacingSm READ spacingSm CONSTANT)
        Q_PROPERTY(int spacingMd READ spacingMd CONSTANT)
        Q_PROPERTY(int spacingLg READ spacingLg CONSTANT)
        Q_PROPERTY(int animFast READ animFast CONSTANT)
        Q_PROPERTY(int animNormal READ animNormal CONSTANT)
        Q_PROPERTY(int fontTitle READ fontTitle CONSTANT)
        Q_PROPERTY(int fontSection READ fontSection CONSTANT)
        Q_PROPERTY(int fontBody READ fontBody CONSTANT)
        Q_PROPERTY(int fontCaption READ fontCaption CONSTANT)

    public:
        explicit ThemeManager(QObject* parent = nullptr);
        ~ThemeManager() override = default;

        [[nodiscard]] static ThemeDefinition darkPreset();
        [[nodiscard]] static ThemeDefinition lightPreset();

        [[nodiscard]] const ThemeDefinition& currentTheme() const noexcept;
        [[nodiscard]] QPalette currentQPalette() const;

        [[nodiscard]] QString activeThemeId() const;
        void setActiveThemeId(const QString& themeId);
        [[nodiscard]] QString activeThemeName() const;
        [[nodiscard]] bool isDark() const noexcept;
        [[nodiscard]] QVariantList availableThemes() const;
        [[nodiscard]] QStringList accentPalette() const;

        [[nodiscard]] QColor canvas() const noexcept { return currentTheme().canvas; }
        [[nodiscard]] QColor surface() const noexcept { return currentTheme().surface; }
        [[nodiscard]] QColor surfaceElevated() const noexcept { return currentTheme().surfaceElevated; }
        [[nodiscard]] QColor surfaceGlass() const noexcept { return currentTheme().surfaceGlass; }
        [[nodiscard]] QColor card() const noexcept { return currentTheme().card; }
        [[nodiscard]] QColor cardHover() const noexcept { return currentTheme().cardHover; }
        [[nodiscard]] QColor inputBg() const noexcept { return currentTheme().inputBg; }
        [[nodiscard]] QColor headerBg() const noexcept { return currentTheme().headerBg; }
        [[nodiscard]] QColor footerBg() const noexcept { return currentTheme().footerBg; }

        [[nodiscard]] QColor border() const noexcept { return currentTheme().border; }
        [[nodiscard]] QColor borderSubtle() const noexcept { return currentTheme().borderSubtle; }
        [[nodiscard]] QColor borderHighlight() const noexcept { return currentTheme().borderHighlight; }
        [[nodiscard]] QColor borderFocus() const noexcept { return currentTheme().borderFocus; }

        [[nodiscard]] QColor accentCyan() const noexcept { return currentTheme().accentCyan; }
        [[nodiscard]] QColor accentBlue() const noexcept { return currentTheme().accentBlue; }
        [[nodiscard]] QColor accentBlueHover() const noexcept { return currentTheme().accentBlueHover; }
        [[nodiscard]] QColor accentPurple() const noexcept { return currentTheme().accentPurple; }
        [[nodiscard]] QColor accentTeal() const noexcept { return currentTheme().accentTeal; }
        [[nodiscard]] QColor defaultTabAccent() const noexcept { return currentTheme().defaultTabAccent; }

        [[nodiscard]] QColor textPrimary() const noexcept { return currentTheme().textPrimary; }
        [[nodiscard]] QColor textSecondary() const noexcept { return currentTheme().textSecondary; }
        [[nodiscard]] QColor textMuted() const noexcept { return currentTheme().textMuted; }
        [[nodiscard]] QColor textPlaceholder() const noexcept { return currentTheme().textPlaceholder; }

        [[nodiscard]] QColor danger() const noexcept { return currentTheme().danger; }
        [[nodiscard]] QColor dangerBg() const noexcept { return currentTheme().dangerBg; }
        [[nodiscard]] QColor dangerText() const noexcept { return currentTheme().dangerText; }
        [[nodiscard]] QColor success() const noexcept { return currentTheme().success; }
        [[nodiscard]] QColor successBg() const noexcept { return currentTheme().successBg; }
        [[nodiscard]] QColor successText() const noexcept { return currentTheme().successText; }
        [[nodiscard]] QColor warning() const noexcept { return currentTheme().warning; }
        [[nodiscard]] QColor warningBg() const noexcept { return currentTheme().warningBg; }
        [[nodiscard]] QColor warningText() const noexcept { return currentTheme().warningText; }
        [[nodiscard]] QColor info() const noexcept { return currentTheme().info; }
        [[nodiscard]] QColor infoBg() const noexcept { return currentTheme().infoBg; }
        [[nodiscard]] QColor infoText() const noexcept { return currentTheme().infoText; }

        [[nodiscard]] QColor selectionBg() const noexcept { return currentTheme().selectionBg; }
        [[nodiscard]] QColor selectionText() const noexcept { return currentTheme().selectionText; }

        [[nodiscard]] int radiusXs() const noexcept { return 6; }
        [[nodiscard]] int radiusSm() const noexcept { return 8; }
        [[nodiscard]] int radiusMd() const noexcept { return 10; }
        [[nodiscard]] int radiusLg() const noexcept { return 14; }
        [[nodiscard]] int radiusXl() const noexcept { return 18; }
        [[nodiscard]] int radiusPill() const noexcept { return 999; }
        [[nodiscard]] int spacingXs() const noexcept { return 4; }
        [[nodiscard]] int spacingSm() const noexcept { return 8; }
        [[nodiscard]] int spacingMd() const noexcept { return 14; }
        [[nodiscard]] int spacingLg() const noexcept { return 20; }
        [[nodiscard]] int animFast() const noexcept { return 120; }
        [[nodiscard]] int animNormal() const noexcept { return 180; }
        [[nodiscard]] int fontTitle() const noexcept { return 18; }
        [[nodiscard]] int fontSection() const noexcept { return 15; }
        [[nodiscard]] int fontBody() const noexcept { return 13; }
        [[nodiscard]] int fontCaption() const noexcept { return 11; }

        Q_INVOKABLE bool setActiveTheme(const QString& themeId);
        Q_INVOKABLE bool registerCustomTheme(const QVariantMap& definition);
        Q_INVOKABLE bool removeCustomTheme(const QString& themeId);
        Q_INVOKABLE [[nodiscard]] QVariantMap exportTheme(const QString& themeId) const;

    signals:
        void themeChanged();
        void availableThemesChanged();

    private:
        [[nodiscard]] int findThemeIndex(const QString& themeId) const noexcept;

        std::vector<ThemeDefinition> m_themes;
        int m_activeIndex = 0;
    };
} // namespace gf::desktop
