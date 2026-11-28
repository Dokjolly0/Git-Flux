import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../themes"

Item {
    id: imgPreviewRoot
    readonly property FluxTheme theme: FluxTheme {}

    property var diffViewer: null
    property string imageMode: "side"
    property string backdropMode: "checker-dark"
    property real zoomFactor: 0
    property real swipeRatio: 0.5
    property real onionOpacity: 0.5

    readonly property bool hasViewer: diffViewer !== null
    readonly property bool hasOldImg: hasViewer && diffViewer.oldImageUrl && diffViewer.oldImageUrl.length > 0
    readonly property bool hasNewImg: hasViewer && diffViewer.newImageUrl && diffViewer.newImageUrl.length > 0
    readonly property bool hasDiffImg: hasViewer && diffViewer.diffImageUrl && diffViewer.diffImageUrl.length > 0
    readonly property bool hasBothImgs: hasOldImg && hasNewImg

    signal zoomStepRequested(real multiplier)

    component ImageBackdrop: Canvas {
        property string mode: imgPreviewRoot.backdropMode
        onModeChanged: requestPaint()
        onWidthChanged: requestPaint()
        onHeightChanged: requestPaint()

        onPaint: {
            const ctx = getContext("2d");
            ctx.reset();
            if (mode === "solid-dark") {
                ctx.fillStyle = "#080B10";
                ctx.fillRect(0, 0, width, height);
                return;
            }
            if (mode === "solid-light") {
                ctx.fillStyle = "#F1F5F9";
                ctx.fillRect(0, 0, width, height);
                return;
            }
            const c1 = (mode === "checker-light") ? "#E2E8F0" : "#151D2B";
            const c2 = (mode === "checker-light") ? "#CBD5E1" : "#0E141F";
            const tile = 12;
            ctx.fillStyle = c1;
            ctx.fillRect(0, 0, width, height);
            ctx.fillStyle = c2;
            for (let y = 0; y < height; y += tile) {
                const offset = (Math.floor(y / tile) % 2) * tile;
                for (let x = offset; x < width; x += tile * 2) {
                    ctx.fillRect(x, y, tile, tile);
                }
            }
        }
    }

    component ZoomableImageCanvas: Item {
        id: imgCanvas
        property url imageSource: ""
        property real naturalWidth: 0
        property real naturalHeight: 0
        property bool preferImplicitSize: false

        clip: true

        ImageBackdrop { anchors.fill: parent }

        Flickable {
            id: imgFlick
            anchors.fill: parent
            contentWidth: Math.max(width, imgWrap.width + 32)
            contentHeight: Math.max(height, imgWrap.height + 32)
            boundsBehavior: Flickable.StopAtBounds
            ScrollBar.vertical: FluxScrollBar {}
            ScrollBar.horizontal: FluxScrollBar {}

            Rectangle {
                id: imgWrap
                readonly property real natW: imgCanvas.preferImplicitSize
                    ? (imgItem.implicitWidth > 0 ? imgItem.implicitWidth : imgCanvas.naturalWidth)
                    : (imgCanvas.naturalWidth > 0 ? imgCanvas.naturalWidth : (imgItem.implicitWidth > 0 ? imgItem.implicitWidth : 0))
                readonly property real natH: imgCanvas.preferImplicitSize
                    ? (imgItem.implicitHeight > 0 ? imgItem.implicitHeight : imgCanvas.naturalHeight)
                    : (imgCanvas.naturalHeight > 0 ? imgCanvas.naturalHeight : (imgItem.implicitHeight > 0 ? imgItem.implicitHeight : 0))
                readonly property real maxFitW: Math.max(32, imgFlick.width - 32)
                readonly property real maxFitH: Math.max(32, imgFlick.height - 32)
                readonly property real effScale: imgPreviewRoot.zoomFactor <= 0
                    ? ((natW > 0 && natH > 0)
                        ? Math.min(1.0, maxFitW / natW, maxFitH / natH)
                        : 1.0)
                    : imgPreviewRoot.zoomFactor
                width: natW > 0 ? Math.max(1, Math.round(natW * effScale)) : 0
                height: natH > 0 ? Math.max(1, Math.round(natH * effScale)) : 0
                x: Math.max(16, Math.round((imgFlick.contentWidth - width) / 2))
                y: Math.max(16, Math.round((imgFlick.contentHeight - height) / 2))
                color: "transparent"
                border.color: imgPreviewRoot.theme.borderHighlight
                border.width: width > 0 ? 1 : 0

                Image {
                    id: imgItem
                    anchors.fill: parent
                    source: imgCanvas.imageSource
                    cache: false
                    smooth: imgPreviewRoot.zoomFactor < 2.0
                    mipmap: true
                    fillMode: Image.PreserveAspectFit
                }
            }
        }
    }

    component DualImageStageFrame: Rectangle {
        property real beforeImplicitW: 0
        property real beforeImplicitH: 0
        property real afterImplicitW: 0
        property real afterImplicitH: 0

        radius: 10
        color: imgPreviewRoot.theme.inputBg
        border.color: imgPreviewRoot.theme.border
        border.width: 1
        clip: true

        ImageBackdrop { anchors.fill: parent }

        readonly property real refW: Math.max(
            imgPreviewRoot.hasViewer ? imgPreviewRoot.diffViewer.oldImageWidth : 0,
            imgPreviewRoot.hasViewer ? imgPreviewRoot.diffViewer.newImageWidth : 0,
            afterImplicitW, beforeImplicitW, 1)
        readonly property real refH: Math.max(
            imgPreviewRoot.hasViewer ? imgPreviewRoot.diffViewer.oldImageHeight : 0,
            imgPreviewRoot.hasViewer ? imgPreviewRoot.diffViewer.newImageHeight : 0,
            afterImplicitH, beforeImplicitH, 1)
        readonly property real fitScale: Math.min(1.0, Math.max(32, width - 48) / refW, Math.max(32, height - 48) / refH)
        readonly property real effScale: imgPreviewRoot.zoomFactor <= 0 ? fitScale : imgPreviewRoot.zoomFactor
        readonly property real stageW: Math.max(1, Math.round(refW * effScale))
        readonly property real stageH: Math.max(1, Math.round(refH * effScale))
    }

    // Ctrl+Wheel zoom anywhere on the image canvas
    WheelHandler {
        acceptedModifiers: Qt.ControlModifier
        onWheel: function(event) {
            if (event.angleDelta.y > 0) imgPreviewRoot.zoomStepRequested(1.15);
            else if (event.angleDelta.y < 0) imgPreviewRoot.zoomStepRequested(0.87);
        }
    }

    // A1. SIDE-BY-SIDE MODE
    RowLayout {
        anchors.fill: parent
        anchors.margins: 16
        spacing: 14
        visible: imgPreviewRoot.imageMode === "side" || !imgPreviewRoot.hasBothImgs

        Rectangle {
            visible: imgPreviewRoot.hasOldImg
            Layout.fillWidth: true
            Layout.fillHeight: true
            radius: 10
            color: imgPreviewRoot.theme.inputBg
            border.color: imgPreviewRoot.theme.danger
            border.width: 1
            clip: true

            ColumnLayout {
                anchors.fill: parent
                spacing: 0

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 32
                    color: imgPreviewRoot.theme.dangerBg
                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 12
                        anchors.rightMargin: 12
                        spacing: 8
                        Rectangle {
                            implicitHeight: 18
                            implicitWidth: beforeBadge.implicitWidth + 10
                            radius: 4
                            color: imgPreviewRoot.theme.danger
                            Text {
                                id: beforeBadge
                                anchors.centerIn: parent
                                text: imgPreviewRoot.hasNewImg ? "PRIMA" : "ELIMINATA"
                                color: imgPreviewRoot.theme.dangerText
                                font.pixelSize: 10
                                font.bold: true
                            }
                        }
                        Text {
                            text: imgPreviewRoot.hasViewer && imgPreviewRoot.diffViewer.oldImageWidth > 0
                                ? (imgPreviewRoot.diffViewer.oldImageWidth + " × " + imgPreviewRoot.diffViewer.oldImageHeight + " px")
                                : ""
                            color: imgPreviewRoot.theme.textSecondary
                            font.pixelSize: 11
                            font.bold: true
                        }
                        Text {
                            text: imgPreviewRoot.hasViewer ? imgPreviewRoot.diffViewer.oldFileSizeText : ""
                            color: imgPreviewRoot.theme.textMuted
                            font.pixelSize: 11
                        }
                        Item { Layout.fillWidth: true }
                        RichPreviewPillButton {
                            text: "↗"
                            ToolTip.visible: hovered
                            ToolTip.text: "Apri versione precedente"
                            onClicked: if (imgPreviewRoot.diffViewer) imgPreviewRoot.diffViewer.openPreviewExternally("old")
                        }
                    }
                }

                ZoomableImageCanvas {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    imageSource: imgPreviewRoot.hasOldImg ? imgPreviewRoot.diffViewer.oldImageUrl : ""
                    naturalWidth: (imgPreviewRoot.hasViewer && imgPreviewRoot.diffViewer.oldImageWidth > 0) ? imgPreviewRoot.diffViewer.oldImageWidth : 0
                    naturalHeight: (imgPreviewRoot.hasViewer && imgPreviewRoot.diffViewer.oldImageHeight > 0) ? imgPreviewRoot.diffViewer.oldImageHeight : 0
                }
            }
        }

        Rectangle {
            visible: imgPreviewRoot.hasNewImg
            Layout.fillWidth: true
            Layout.fillHeight: true
            radius: 10
            color: imgPreviewRoot.theme.inputBg
            border.color: imgPreviewRoot.theme.success
            border.width: 1
            clip: true

            ColumnLayout {
                anchors.fill: parent
                spacing: 0

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 32
                    color: imgPreviewRoot.theme.successBg
                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 12
                        anchors.rightMargin: 12
                        spacing: 8
                        Rectangle {
                            implicitHeight: 18
                            implicitWidth: afterBadge.implicitWidth + 10
                            radius: 4
                            color: imgPreviewRoot.theme.success
                            Text {
                                id: afterBadge
                                anchors.centerIn: parent
                                text: imgPreviewRoot.hasOldImg ? "DOPO" : "NUOVA"
                                color: imgPreviewRoot.theme.successText
                                font.pixelSize: 10
                                font.bold: true
                            }
                        }
                        Text {
                            text: imgPreviewRoot.hasViewer && imgPreviewRoot.diffViewer.newImageWidth > 0
                                ? (imgPreviewRoot.diffViewer.newImageWidth + " × " + imgPreviewRoot.diffViewer.newImageHeight + " px")
                                : ""
                            color: imgPreviewRoot.theme.textSecondary
                            font.pixelSize: 11
                            font.bold: true
                        }
                        Text {
                            text: imgPreviewRoot.hasViewer ? imgPreviewRoot.diffViewer.newFileSizeText : ""
                            color: imgPreviewRoot.theme.textMuted
                            font.pixelSize: 11
                        }
                        Item { Layout.fillWidth: true }
                        RichPreviewPillButton {
                            text: "↗"
                            ToolTip.visible: hovered
                            ToolTip.text: "Apri versione attuale"
                            onClicked: if (imgPreviewRoot.diffViewer) imgPreviewRoot.diffViewer.openPreviewExternally("new")
                        }
                    }
                }

                ZoomableImageCanvas {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    imageSource: imgPreviewRoot.hasNewImg ? imgPreviewRoot.diffViewer.newImageUrl : ""
                    naturalWidth: (imgPreviewRoot.hasViewer && imgPreviewRoot.diffViewer.newImageWidth > 0) ? imgPreviewRoot.diffViewer.newImageWidth : 0
                    naturalHeight: (imgPreviewRoot.hasViewer && imgPreviewRoot.diffViewer.newImageHeight > 0) ? imgPreviewRoot.diffViewer.newImageHeight : 0
                }
            }
        }
    }

    // A2. SWIPE MODE
    Item {
        anchors.fill: parent
        anchors.margins: 16
        visible: imgPreviewRoot.hasBothImgs && imgPreviewRoot.imageMode === "swipe"

        DualImageStageFrame {
            id: swipeFrame
            anchors.fill: parent
            beforeImplicitW: swipeBefore.implicitWidth
            beforeImplicitH: swipeBefore.implicitHeight
            afterImplicitW: swipeAfter.implicitWidth
            afterImplicitH: swipeAfter.implicitHeight

            Item {
                id: swipeStage
                anchors.centerIn: parent
                width: swipeFrame.stageW
                height: swipeFrame.stageH

                Image {
                    id: swipeAfter
                    anchors.fill: parent
                    source: imgPreviewRoot.hasNewImg ? imgPreviewRoot.diffViewer.newImageUrl : ""
                    cache: false
                    smooth: imgPreviewRoot.zoomFactor < 2.0
                    mipmap: true
                    fillMode: Image.PreserveAspectFit
                }

                Item {
                    anchors.left: parent.left
                    anchors.top: parent.top
                    anchors.bottom: parent.bottom
                    width: Math.max(0, Math.min(parent.width, parent.width * imgPreviewRoot.swipeRatio))
                    clip: true

                    Image {
                        id: swipeBefore
                        x: 0
                        y: 0
                        width: swipeStage.width
                        height: swipeStage.height
                        source: imgPreviewRoot.hasOldImg ? imgPreviewRoot.diffViewer.oldImageUrl : ""
                        cache: false
                        smooth: imgPreviewRoot.zoomFactor < 2.0
                        mipmap: true
                        fillMode: Image.PreserveAspectFit
                    }
                }

                Rectangle {
                    x: Math.round(swipeStage.width * imgPreviewRoot.swipeRatio) - 1
                    y: 0
                    width: 2
                    height: swipeStage.height
                    color: imgPreviewRoot.theme.accentCyan

                    Rectangle {
                        anchors.centerIn: parent
                        width: 28
                        height: 28
                        radius: 14
                        color: imgPreviewRoot.theme.surfaceElevated
                        border.color: imgPreviewRoot.theme.accentCyan
                        border.width: 2
                        Text {
                            anchors.centerIn: parent
                            text: "↔"
                            color: imgPreviewRoot.theme.accentCyan
                            font.pixelSize: 13
                            font.bold: true
                        }
                    }
                }

                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.SplitHCursor
                    onPressed: function(mouse) {
                        imgPreviewRoot.swipeRatio = Math.max(0.0, Math.min(1.0, mouse.x / Math.max(1, width)));
                    }
                    onPositionChanged: function(mouse) {
                        if (pressed) {
                            imgPreviewRoot.swipeRatio = Math.max(0.0, Math.min(1.0, mouse.x / Math.max(1, width)));
                        }
                    }
                }
            }

            Rectangle {
                anchors.left: parent.left
                anchors.top: parent.top
                anchors.margins: 12
                implicitHeight: 22
                implicitWidth: swLbl1.implicitWidth + 12
                radius: 5
                color: Qt.alpha(imgPreviewRoot.theme.dangerBg, 0.85)
                Text { id: swLbl1; anchors.centerIn: parent; text: "◀ PRIMA"; color: imgPreviewRoot.theme.dangerText; font.pixelSize: 10; font.bold: true }
            }
            Rectangle {
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.margins: 12
                implicitHeight: 22
                implicitWidth: swLbl2.implicitWidth + 12
                radius: 5
                color: Qt.alpha(imgPreviewRoot.theme.successBg, 0.85)
                Text { id: swLbl2; anchors.centerIn: parent; text: "DOPO ▶"; color: imgPreviewRoot.theme.successText; font.pixelSize: 10; font.bold: true }
            }
        }
    }

    // A3. ONION SKIN MODE
    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 16
        spacing: 10
        visible: imgPreviewRoot.hasBothImgs && imgPreviewRoot.imageMode === "onion"

        DualImageStageFrame {
            id: onionFrame
            Layout.fillWidth: true
            Layout.fillHeight: true
            beforeImplicitW: onionBefore.implicitWidth
            beforeImplicitH: onionBefore.implicitHeight
            afterImplicitW: onionAfter.implicitWidth
            afterImplicitH: onionAfter.implicitHeight

            Item {
                anchors.centerIn: parent
                width: onionFrame.stageW
                height: onionFrame.stageH

                Image {
                    id: onionBefore
                    anchors.fill: parent
                    source: imgPreviewRoot.hasOldImg ? imgPreviewRoot.diffViewer.oldImageUrl : ""
                    cache: false
                    smooth: imgPreviewRoot.zoomFactor < 2.0
                    mipmap: true
                    fillMode: Image.PreserveAspectFit
                }
                Image {
                    id: onionAfter
                    anchors.fill: parent
                    source: imgPreviewRoot.hasNewImg ? imgPreviewRoot.diffViewer.newImageUrl : ""
                    opacity: imgPreviewRoot.onionOpacity
                    cache: false
                    smooth: imgPreviewRoot.zoomFactor < 2.0
                    mipmap: true
                    fillMode: Image.PreserveAspectFit
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 38
            radius: 8
            color: imgPreviewRoot.theme.surface
            border.color: imgPreviewRoot.theme.borderSubtle
            border.width: 1

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 16
                anchors.rightMargin: 16
                spacing: 12

                Text { text: "PRIMA"; color: imgPreviewRoot.theme.dangerText; font.pixelSize: 11; font.bold: true }
                Slider {
                    Layout.fillWidth: true
                    from: 0.0
                    to: 1.0
                    value: imgPreviewRoot.onionOpacity
                    onMoved: imgPreviewRoot.onionOpacity = value
                }
                Text { text: "DOPO"; color: imgPreviewRoot.theme.successText; font.pixelSize: 11; font.bold: true }
                Text {
                    text: Math.round(imgPreviewRoot.onionOpacity * 100) + "%"
                    color: imgPreviewRoot.theme.accentCyan
                    font.pixelSize: 11
                    font.bold: true
                    Layout.preferredWidth: 36
                    horizontalAlignment: Text.AlignRight
                }
            }
        }
    }

    // A4. PIXEL DIFF HIGHLIGHT MODE
    Item {
        anchors.fill: parent
        anchors.margins: 16
        visible: imgPreviewRoot.hasBothImgs && imgPreviewRoot.hasDiffImg && imgPreviewRoot.imageMode === "diff"

        Rectangle {
            anchors.fill: parent
            radius: 10
            color: imgPreviewRoot.theme.inputBg
            border.color: imgPreviewRoot.theme.danger
            border.width: 1
            clip: true

            ZoomableImageCanvas {
                anchors.fill: parent
                preferImplicitSize: true
                imageSource: imgPreviewRoot.hasDiffImg ? imgPreviewRoot.diffViewer.diffImageUrl : ""
                naturalWidth: Math.max(imgPreviewRoot.hasViewer ? imgPreviewRoot.diffViewer.oldImageWidth : 0,
                                       imgPreviewRoot.hasViewer ? imgPreviewRoot.diffViewer.newImageWidth : 0)
                naturalHeight: Math.max(imgPreviewRoot.hasViewer ? imgPreviewRoot.diffViewer.oldImageHeight : 0,
                                        imgPreviewRoot.hasViewer ? imgPreviewRoot.diffViewer.newImageHeight : 0)
            }

            Rectangle {
                anchors.left: parent.left
                anchors.bottom: parent.bottom
                anchors.margins: 12
                implicitHeight: 24
                implicitWidth: diffLegendRow.implicitWidth + 16
                radius: 6
                color: Qt.alpha(imgPreviewRoot.theme.surfaceElevated, 0.9)
                border.color: imgPreviewRoot.theme.borderHighlight
                border.width: 1

                RowLayout {
                    id: diffLegendRow
                    anchors.centerIn: parent
                    spacing: 8
                    Rectangle { width: 10; height: 10; radius: 2; color: imgPreviewRoot.theme.danger }
                    Text {
                        text: "Pixel modificati (" + (imgPreviewRoot.hasViewer ? imgPreviewRoot.diffViewer.imageDiffPercent.toFixed(2) : "0") + "%)"
                        color: imgPreviewRoot.theme.textPrimary
                        font.pixelSize: 11
                        font.bold: true
                    }
                }
            }
        }
    }
}
