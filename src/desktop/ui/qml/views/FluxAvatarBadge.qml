import QtQuick
import "../themes"

Rectangle {
    id: root

    FluxTheme { id: theme }

    property string avatarUrl: ""
    property string initials: "??"
    property color avatarColor: theme.accentBlue
    property color maskColor: theme.canvas
    property color ringColor: "transparent"
    property real ringWidth: 0
    property int initialsFontSize: 10
    property color initialsColor: theme.selectionText

    radius: width / 2
    color: avatarImg.status === Image.Ready ? "transparent" : avatarColor
    border.width: 0

    Image {
        id: avatarImg
        anchors.fill: parent
        source: root.avatarUrl
        fillMode: Image.PreserveAspectCrop
        smooth: true
        mipmap: true
    }

    Canvas {
        id: maskCanvas
        anchors.fill: parent
        visible: avatarImg.status === Image.Ready

        onVisibleChanged: if (visible) requestPaint()
        onAvailableChanged: if (available) requestPaint()
        onWidthChanged: if (visible) requestPaint()
        onHeightChanged: if (visible) requestPaint()

        Connections {
            target: root
            function onMaskColorChanged() { maskCanvas.requestPaint(); }
            function onRingColorChanged() { maskCanvas.requestPaint(); }
            function onRingWidthChanged() { maskCanvas.requestPaint(); }
        }

        onPaint: {
            const ctx = getContext("2d");
            ctx.reset();
            const cx = width / 2;
            const cy = height / 2;
            const r = Math.min(width, height) / 2 - 0.5;
            if (r <= 0) return;

            ctx.globalCompositeOperation = "source-over";
            ctx.fillStyle = root.maskColor.toString();
            ctx.fillRect(0, 0, width, height);

            ctx.globalCompositeOperation = "destination-out";
            ctx.beginPath();
            ctx.arc(cx, cy, r, 0, Math.PI * 2, false);
            ctx.closePath();
            ctx.fillStyle = "#000000";
            ctx.fill();

            if (root.ringWidth > 0 && root.ringColor.a > 0) {
                ctx.globalCompositeOperation = "source-over";
                ctx.beginPath();
                ctx.arc(cx, cy, r - root.ringWidth / 2 + 0.5, 0, Math.PI * 2, false);
                ctx.strokeStyle = root.ringColor.toString();
                ctx.lineWidth = root.ringWidth;
                ctx.stroke();
            }
        }
    }

    Text {
        anchors.centerIn: parent
        text: root.initials || "??"
        color: root.initialsColor
        font.pixelSize: root.initialsFontSize
        font.bold: true
        visible: avatarImg.status !== Image.Ready
    }
}
