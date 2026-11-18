import QtQuick

QtObject {
    id: theme

    // Primary Modern Dark Palette (Linear / Raycast / Arc inspired)
    readonly property color canvas: "#080B10"
    readonly property color surface: "#0F141C"
    readonly property color surfaceElevated: "#141C28"
    readonly property color surfaceGlass: "#182232"
    readonly property color card: "#161F2E"
    readonly property color cardHover: "#1E2B3E"
    readonly property color border: "#222F42"
    readonly property color borderSubtle: "#1A2434"
    readonly property color borderHighlight: "#2D3F59"
    readonly property color borderFocus: "#38BDF8"
    readonly property color inputBg: "#0A0E15"

    // Flux Accents
    readonly property color accentCyan: "#00E5FF"
    readonly property color accentBlue: "#0284C7"
    readonly property color accentBlueHover: "#0369A1"
    readonly property color accentPurple: "#7C4DFF"
    readonly property color accentTeal: "#00B4D8"

    // Typography & Text
    readonly property color textPrimary: "#F8FAFC"
    readonly property color textSecondary: "#94A3B8"
    readonly property color textMuted: "#64748B"

    // Semantic States
    readonly property color danger: "#EF4444"
    readonly property color dangerBg: "#3B1219"
    readonly property color success: "#10B981"
    readonly property color successBg: "#064E3B"
    readonly property color warning: "#F59E0B"
    readonly property color warningBg: "#451A03"

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
