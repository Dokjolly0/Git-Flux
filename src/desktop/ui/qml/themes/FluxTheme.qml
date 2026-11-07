import QtQuick

QtObject {
    id: theme

    // Primary Dark Palette (Inspired by Git Flux branding)
    readonly property color canvas: "#0E1117"
    readonly property color surface: "#161B22"
    readonly property color card: "#21262D"
    readonly property color cardHover: "#2B323C"
    readonly property color border: "#30363D"
    readonly property color borderFocus: "#58A6FF"
    readonly property color inputBg: "#0B0E14"

    // Flux Accents
    readonly property color accentCyan: "#00E5FF"
    readonly property color accentPurple: "#7C4DFF"
    readonly property color accentTeal: "#00B4D8"

    // Typography & Text
    readonly property color textPrimary: "#F0F6FC"
    readonly property color textSecondary: "#8B949E"
    readonly property color textMuted: "#6E7681"

    // Semantic States
    readonly property color danger: "#F85149"
    readonly property color success: "#3FB950"
    readonly property color warning: "#D29922"

    // Dimensions and Spacing
    readonly property int radiusSm: 4
    readonly property int radiusMd: 8
    readonly property int radiusLg: 12

    readonly property int spacingXs: 4
    readonly property int spacingSm: 8
    readonly property int spacingMd: 14
    readonly property int spacingLg: 20

    // Font Sizes
    readonly property int fontTitle: 18
    readonly property int fontSection: 15
    readonly property int fontBody: 13
    readonly property int fontCaption: 11
}
