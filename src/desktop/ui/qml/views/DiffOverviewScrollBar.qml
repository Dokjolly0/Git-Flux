import QtQuick
import QtQuick.Controls

Item {
    id: root

    property Flickable flickable: null
    property var markers: []
    property color oldColor: "#EF4444"
    property color newColor: "#10B981"
    property int minThumbHeight: 26
    property int minMarkerHeight: 3

    implicitWidth: 28
    implicitHeight: 100

    readonly property real maxScroll: flickable ? Math.max(0, flickable.contentHeight - flickable.height) : 0
    readonly property bool scrollable: flickable !== null && maxScroll > 0.5
    readonly property real currentScroll: flickable ? Math.max(0, Math.min(maxScroll, flickable.contentY - flickable.originY)) : 0
    readonly property real visibleRatio: (flickable && flickable.contentHeight > 0)
        ? Math.min(1.0, flickable.height / flickable.contentHeight)
        : 1.0
    readonly property real trackHeight: Math.max(1, height)
    readonly property real thumbHeight: scrollable
        ? Math.max(Math.min(trackHeight, minThumbHeight), Math.min(trackHeight, visibleRatio * trackHeight))
        : trackHeight
    readonly property real maxThumbY: Math.max(0, trackHeight - thumbHeight)
    readonly property real scrollFraction: maxScroll > 0 ? Math.max(0, Math.min(1, currentScroll / maxScroll)) : 0
    readonly property real thumbY: scrollable ? (scrollFraction * maxThumbY) : 0

    onMarkersChanged: markerCanvas.requestPaint()
    onOldColorChanged: markerCanvas.requestPaint()
    onNewColorChanged: markerCanvas.requestPaint()
    onWidthChanged: markerCanvas.requestPaint()
    onHeightChanged: markerCanvas.requestPaint()
    onVisibleChanged: if (visible) markerCanvas.requestPaint()

    function scrollToThumbY(targetThumbY) {
        if (!flickable || !scrollable || maxThumbY <= 0) {
            return;
        }
        var clamped = Math.max(0, Math.min(maxThumbY, targetThumbY));
        var frac = clamped / maxThumbY;
        if (flickable.flicking) {
            flickable.cancelFlick();
        }
        flickable.contentY = flickable.originY + frac * maxScroll;
    }

    // Track background
    Rectangle {
        anchors.fill: parent
        color: "#090D14"

        // Left border separating the overview bar from diff content
        Rectangle {
            anchors.left: parent.left
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            width: 1
            color: "#1E293B"
        }

        // Center divider between Old (before commit, left half) and New (after commit, right half)
        Rectangle {
            x: Math.floor(parent.width / 2)
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            width: 1
            color: "#1E293B"
        }
    }

    // Change markers canvas (Left half = old/deletions in red, Right half = new/additions in green)
    Canvas {
        id: markerCanvas
        anchors.fill: parent
        onAvailableChanged: if (available) requestPaint()

        onPaint: {
            var ctx = getContext("2d");
            ctx.reset();
            if (width <= 0 || height <= 0) {
                return;
            }
            var list = root.markers;
            if (!list || list.length === 0) {
                return;
            }

            var dividerX = Math.floor(width / 2);
            var leftX = 3;
            var leftW = Math.max(1, dividerX - 4);
            var rightX = dividerX + 2;
            var rightW = Math.max(1, width - rightX - 2);

            ctx.globalAlpha = 0.88;
            for (var i = 0; i < list.length; ++i) {
                var m = list[i];
                if (!m) {
                    continue;
                }
                var startRatio = Math.max(0, Math.min(1, m.start !== undefined ? m.start : 0));
                var endRatio = Math.max(startRatio, Math.min(1, m.end !== undefined ? m.end : 0));
                var rawH = (endRatio - startRatio) * height;
                var h = Math.min(height, Math.max(root.minMarkerHeight, Math.ceil(rawH)));
                var y = Math.floor(startRatio * height);
                if (y + h > height) {
                    y = Math.max(0, height - h);
                }

                if (m.side === "old") {
                    ctx.fillStyle = root.oldColor;
                    ctx.fillRect(leftX, y, leftW, h);
                } else {
                    ctx.fillStyle = root.newColor;
                    ctx.fillRect(rightX, y, rightW, h);
                }
            }
        }
    }

    // Draggable viewport indicator (translucent so underlying red/green markers stay visible)
    Rectangle {
        id: viewportThumb
        visible: root.scrollable
        x: 1
        width: Math.max(4, parent.width - 2)
        y: root.thumbY
        height: root.thumbHeight
        radius: 3
        color: barMouseArea.isDragging
            ? Qt.rgba(0.0, 0.898, 1.0, 0.24)
            : (barMouseArea.containsMouse
                ? Qt.rgba(0.22, 0.74, 0.97, 0.19)
                : Qt.rgba(0.58, 0.64, 0.72, 0.14))
        border.width: 1
        border.color: barMouseArea.isDragging
            ? "#00E5FF"
            : (barMouseArea.containsMouse ? "#38BDF8" : "#475569")

        Behavior on border.color {
            ColorAnimation {
                duration: 120
            }
        }
    }

    MouseArea {
        id: barMouseArea
        anchors.fill: parent
        hoverEnabled: true
        preventStealing: true
        cursorShape: root.scrollable
            ? (isDragging ? Qt.ClosedHandCursor : Qt.PointingHandCursor)
            : Qt.ArrowCursor

        property bool isDragging: false
        property real grabOffsetY: 0

        ToolTip.visible: containsMouse && !isDragging
        ToolTip.delay: 500
        ToolTip.text: (containsMouse && mouseX < parent.width / 2)
            ? "Prima del commit (righe rimosse)"
            : "Nuova versione (righe aggiunte)"

        onPressed: (mouse) => {
            if (!root.scrollable) {
                return;
            }
            isDragging = true;
            if (mouse.y >= viewportThumb.y && mouse.y <= viewportThumb.y + viewportThumb.height) {
                grabOffsetY = mouse.y - viewportThumb.y;
            } else {
                grabOffsetY = viewportThumb.height / 2;
                root.scrollToThumbY(mouse.y - grabOffsetY);
            }
        }

        onPositionChanged: (mouse) => {
            if (isDragging && pressed && root.scrollable) {
                root.scrollToThumbY(mouse.y - grabOffsetY);
            }
        }

        onReleased: isDragging = false
        onCanceled: isDragging = false

        onWheel: (wheel) => {
            if (!root.flickable || !root.scrollable) {
                wheel.accepted = false;
                return;
            }
            var dy = 0;
            if (wheel.pixelDelta && wheel.pixelDelta.y !== 0) {
                dy = wheel.pixelDelta.y;
            } else if (wheel.angleDelta && wheel.angleDelta.y !== 0) {
                dy = (wheel.angleDelta.y / 120.0) * 60.0;
            }
            if (dy !== 0) {
                if (root.flickable.flicking) {
                    root.flickable.cancelFlick();
                }
                var minY = root.flickable.originY;
                var maxY = minY + root.maxScroll;
                root.flickable.contentY = Math.max(minY, Math.min(maxY, root.flickable.contentY - dy));
                wheel.accepted = true;
            }
        }
    }
}
