import QtQuick
import "../themes"

Rectangle {
    id: hbar
    readonly property FluxTheme theme: FluxTheme {}

    property real contentWidth: 0
    property real viewWidth: 0
    property real scrollValue: 0
    property real maxScroll: Math.max(0, contentWidth - viewWidth)

    signal scrollRequested(real newValue)

    height: 10
    color: theme.surface
    border.color: theme.borderSubtle
    border.width: 1
    radius: 4
    visible: maxScroll > 0
    z: 30

    MouseArea {
        id: trackMouseArea
        anchors.fill: parent
        hoverEnabled: true
        preventStealing: true
        onPressed: (mouse) => {
            if (mouse.x < hThumb.x) {
                hbar.scrollRequested(Math.max(0, hbar.scrollValue - hbar.viewWidth * 0.4));
            } else if (mouse.x > hThumb.x + hThumb.width) {
                hbar.scrollRequested(Math.min(hbar.maxScroll, hbar.scrollValue + hbar.viewWidth * 0.4));
            }
        }
    }

    Rectangle {
        id: hThumb
        height: 6
        anchors.verticalCenter: parent.verticalCenter
        radius: 3
        color: hThumbArea.isDragging
            ? hbar.theme.accentCyan
            : (hThumbArea.containsMouse || trackMouseArea.containsMouse ? hbar.theme.borderFocus : hbar.theme.textPlaceholder)
        opacity: (hThumbArea.isDragging || hThumbArea.containsMouse || trackMouseArea.containsMouse) ? 0.95 : 0.65

        readonly property real trackWidth: hbar.width
        readonly property real visibleRatio: Math.min(1.0, hbar.viewWidth / Math.max(1, hbar.contentWidth))
        width: Math.max(36, trackWidth * visibleRatio)

        x: hbar.maxScroll > 0 ? (hbar.scrollValue / hbar.maxScroll) * Math.max(1, trackWidth - width) : 0

        MouseArea {
            id: hThumbArea
            anchors.fill: parent
            anchors.margins: -4
            hoverEnabled: true
            preventStealing: true

            property real startThumbTrackX: 0
            property real startScroll: 0
            property bool isDragging: false

            onPressed: (mouse) => {
                const pt = mapToItem(hbar, mouse.x, mouse.y);
                startThumbTrackX = pt.x;
                startScroll = hbar.scrollValue;
                isDragging = true;
            }
            onPositionChanged: (mouse) => {
                if (isDragging && pressed) {
                    const pt = mapToItem(hbar, mouse.x, mouse.y);
                    const deltaX = pt.x - startThumbTrackX;
                    const maxThumbX = Math.max(1, hbar.width - hThumb.width);
                    const targetScroll = Math.max(0, Math.min(hbar.maxScroll, startScroll + (deltaX / maxThumbX) * hbar.maxScroll));
                    hbar.scrollRequested(targetScroll);
                }
            }
            onReleased: isDragging = false
            onCanceled: isDragging = false
        }
    }
}
