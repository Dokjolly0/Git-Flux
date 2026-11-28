import QtQuick

QtObject {
    id: theme

    // When AppBootstrap registers `gfTheme` (ThemeManager), all properties bind reactively
    // to the active preset (Dark, Light, or custom). Standalone QML tests fall back to Dark.
    readonly property var _live: (typeof gfTheme !== "undefined" && gfTheme) ? gfTheme : null

    readonly property string activeThemeId: _live ? _live.activeThemeId : "dark"
    readonly property string activeThemeName: _live ? _live.activeThemeName : "Flux Dark"
    readonly property bool isDark: _live ? _live.isDark : true
    readonly property var accentPalette: _live
        ? _live.accentPalette
        : ["#38BDF8", "#A78BFA", "#34D399", "#FBBF24", "#FB7185", "#818CF8", "#2DD4BF", "#FB923C"]

    // Surfaces & Containers
    readonly property color canvas: _live ? _live.canvas : "#080B10"
    readonly property color surface: _live ? _live.surface : "#0F141C"
    readonly property color surfaceElevated: _live ? _live.surfaceElevated : "#141C28"
    readonly property color surfaceGlass: _live ? _live.surfaceGlass : "#182232"
    readonly property color card: _live ? _live.card : "#161F2E"
    readonly property color cardHover: _live ? _live.cardHover : "#1E2B3E"
    readonly property color inputBg: _live ? _live.inputBg : "#0A0E15"
    readonly property color headerBg: _live ? _live.headerBg : "#0A0E15"
    readonly property color footerBg: _live ? _live.footerBg : "#0C121C"

    // Borders
    readonly property color border: _live ? _live.border : "#222F42"
    readonly property color borderSubtle: _live ? _live.borderSubtle : "#1A2434"
    readonly property color borderHighlight: _live ? _live.borderHighlight : "#2D3F59"
    readonly property color borderFocus: _live ? _live.borderFocus : "#38BDF8"

    // Brand & Tab Accents
    readonly property color accentCyan: _live ? _live.accentCyan : "#00E5FF"
    readonly property color accentBlue: _live ? _live.accentBlue : "#0284C7"
    readonly property color accentBlueHover: _live ? _live.accentBlueHover : "#0369A1"
    readonly property color accentPurple: _live ? _live.accentPurple : "#7C4DFF"
    readonly property color accentTeal: _live ? _live.accentTeal : "#00B4D8"
    readonly property color defaultTabAccent: _live ? _live.defaultTabAccent : "#38BDF8"

    // Typography & Text
    readonly property color textPrimary: _live ? _live.textPrimary : "#F8FAFC"
    readonly property color textSecondary: _live ? _live.textSecondary : "#94A3B8"
    readonly property color textMuted: _live ? _live.textMuted : "#64748B"
    readonly property color textPlaceholder: _live ? _live.textPlaceholder : "#475569"

    // Semantic States
    readonly property color danger: _live ? _live.danger : "#EF4444"
    readonly property color dangerBg: _live ? _live.dangerBg : "#3B1219"
    readonly property color dangerText: _live ? _live.dangerText : "#FCA5A5"
    readonly property color success: _live ? _live.success : "#10B981"
    readonly property color successBg: _live ? _live.successBg : "#082F25"
    readonly property color successText: _live ? _live.successText : "#34D399"
    readonly property color warning: _live ? _live.warning : "#F59E0B"
    readonly property color warningBg: _live ? _live.warningBg : "#2A1B07"
    readonly property color warningText: _live ? _live.warningText : "#FBBF24"
    readonly property color info: _live ? _live.info : "#38BDF8"
    readonly property color infoBg: _live ? _live.infoBg : "#0F2238"
    readonly property color infoText: _live ? _live.infoText : "#7DD3FC"

    // Selection
    readonly property color selectionBg: _live ? _live.selectionBg : "#1D4ED8"
    readonly property color selectionText: _live ? _live.selectionText : "#FFFFFF"

    // Modern Rounded Corner Radii
    readonly property int radiusXs: 6
    readonly property int radiusSm: 8
    readonly property int radiusMd: 10
    readonly property int radiusLg: 14
    readonly property int radiusXl: 18
    readonly property int radiusPill: 999

    // Spacing
    readonly property int spacingXs: 4
    readonly property int spacingSm: 8
    readonly property int spacingMd: 14
    readonly property int spacingLg: 20

    // Micro-animation durations (ms)
    readonly property int animFast: 120
    readonly property int animNormal: 180

    // Font Sizes
    readonly property int fontTitle: 18
    readonly property int fontSection: 15
    readonly property int fontBody: 13
    readonly property int fontCaption: 11
}
