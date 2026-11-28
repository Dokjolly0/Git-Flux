import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root
    color: "#0D1117"

    property var diffViewer: null
    property string monoFont: (typeof gfMonoFont !== "undefined" && gfMonoFont) ? gfMonoFont : "monospace"

    // Sub-tab: "preview" (visual / archive / font / info) vs "hex"
    property string activeTab: "preview"
    // Image comparison mode: "side", "swipe", "onion", "diff"
    property string imageMode: "side"
    // Image canvas backdrop: "checker-dark", "checker-light", "solid-dark", "solid-light"
    property string backdropMode: "checker-dark"
    // Image zoom: 0 means "Fit to view", > 0 means explicit scale factor (0.25 to 8.0)
    property real zoomFactor: 0
    // Swipe divider ratio (0.0 to 1.0) and Onion skin opacity (0.0 to 1.0)
    property real swipeRatio: 0.5
    property real onionOpacity: 0.5
    // Font preview text
    property string fontSampleText: "The quick brown fox jumps over the lazy dog — 0123456789"
    // Archive & Hex filters
    property string archiveFilterText: ""
    property bool archiveOnlyChanged: false
    property bool hexOnlyChanged: false

    readonly property bool hasViewer: root.diffViewer !== null
    readonly property string fileKind: hasViewer ? (root.diffViewer.fileKind || "binary") : "binary"
    readonly property bool hasOldImg: hasViewer && root.diffViewer.oldImageUrl && root.diffViewer.oldImageUrl.length > 0
    readonly property bool hasNewImg: hasViewer && root.diffViewer.newImageUrl && root.diffViewer.newImageUrl.length > 0
    readonly property bool hasDiffImg: hasViewer && root.diffViewer.diffImageUrl && root.diffViewer.diffImageUrl.length > 0
    readonly property bool hasBothImgs: hasOldImg && hasNewImg
    readonly property bool hasVisual: hasOldImg || hasNewImg
    readonly property bool hasArchive: hasViewer && root.diffViewer.archiveEntries && root.diffViewer.archiveEntries.length > 0
    readonly property bool hasFont: hasViewer && ((root.diffViewer.oldFontFamily && root.diffViewer.oldFontFamily.length > 0) ||
                                                  (root.diffViewer.newFontFamily && root.diffViewer.newFontFamily.length > 0))
    readonly property bool hasHex: hasViewer && root.diffViewer.hexRows && root.diffViewer.hexRows.length > 0

    // Reset transient interactive state when switching to a different file
    Connections {
        target: root.diffViewer
        function onDiffLoaded() {
            root.activeTab = "preview";
            root.zoomFactor = 0;
            root.swipeRatio = 0.5;
            root.onionOpacity = 0.5;
            root.archiveFilterText = "";
            root.archiveOnlyChanged = false;
            root.hexOnlyChanged = false;
            if (!root.hasBothImgs) {
                root.imageMode = "side";
            }
        }
    }

    function kindIcon(kind) {
        switch (kind) {
        case "image": return "🖼";
        case "pdf": return "📄";
        case "archive": return "📦";
        case "font": return "🔤";
        case "audio": return "🎵";
        case "video": return "🎬";
        case "database": return "🗄️";
        case "executable": return "⚙️";
        case "document": return "📑";
        default: return "📦";
        }
    }

    function stepZoom(multiplier) {
        const cur = root.zoomFactor <= 0 ? 1.0 : root.zoomFactor;
        const next = Math.max(0.25, Math.min(8.0, cur * multiplier));
        root.zoomFactor = Math.round(next * 100) / 100;
    }

    // Reusable checkerboard / solid canvas for transparent images
    component ImageBackdrop: Canvas {
        property string mode: root.backdropMode
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

    // Reusable zoomable, scrollable image viewport with checkerboard backdrop
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
                readonly property real effScale: root.zoomFactor <= 0
                    ? ((natW > 0 && natH > 0)
                        ? Math.min(1.0, maxFitW / natW, maxFitH / natH)
                        : 1.0)
                    : root.zoomFactor
                width: natW > 0 ? Math.max(1, Math.round(natW * effScale)) : 0
                height: natH > 0 ? Math.max(1, Math.round(natH * effScale)) : 0
                x: Math.max(16, Math.round((imgFlick.contentWidth - width) / 2))
                y: Math.max(16, Math.round((imgFlick.contentHeight - height) / 2))
                color: "transparent"
                border.color: "#334155"
                border.width: width > 0 ? 1 : 0

                Image {
                    id: imgItem
                    anchors.fill: parent
                    source: imgCanvas.imageSource
                    cache: false
                    smooth: root.zoomFactor < 2.0
                    mipmap: true
                    fillMode: Image.PreserveAspectFit
                }
            }
        }
    }

    // Reusable framed comparison stage computing shared scale for Before/After images
    component DualImageStageFrame: Rectangle {
        property real beforeImplicitW: 0
        property real beforeImplicitH: 0
        property real afterImplicitW: 0
        property real afterImplicitH: 0

        radius: 10
        color: "#0A0E15"
        border.color: "#28374E"
        border.width: 1
        clip: true

        ImageBackdrop { anchors.fill: parent }

        readonly property real refW: Math.max(
            root.hasViewer ? root.diffViewer.oldImageWidth : 0,
            root.hasViewer ? root.diffViewer.newImageWidth : 0,
            afterImplicitW, beforeImplicitW, 1)
        readonly property real refH: Math.max(
            root.hasViewer ? root.diffViewer.oldImageHeight : 0,
            root.hasViewer ? root.diffViewer.newImageHeight : 0,
            afterImplicitH, beforeImplicitH, 1)
        readonly property real fitScale: Math.min(1.0, Math.max(32, width - 48) / refW, Math.max(32, height - 48) / refH)
        readonly property real effScale: root.zoomFactor <= 0 ? fitScale : root.zoomFactor
        readonly property real stageW: Math.max(1, Math.round(refW * effScale))
        readonly property real stageH: Math.max(1, Math.round(refH * effScale))
    }

    // Reusable font specimen preview card
    component FontSpecimenPane: Rectangle {
        id: specimenPane
        property string headerText: ""
        property color headerColor: "#FCA5A5"
        property string fontFamily: ""

        Layout.fillWidth: true
        Layout.fillHeight: true
        radius: 10
        color: "#0A0E15"
        border.width: 1
        clip: true

        Flickable {
            anchors.fill: parent
            anchors.margins: 16
            contentHeight: specimenCol.implicitHeight
            ScrollBar.vertical: FluxScrollBar {}

            ColumnLayout {
                id: specimenCol
                width: parent.width
                spacing: 14
                Text {
                    text: specimenPane.headerText
                    color: specimenPane.headerColor
                    font.pixelSize: 11
                    font.bold: true
                }
                Text {
                    Layout.fillWidth: true; wrapMode: Text.Wrap; color: "#F8FAFC"
                    font.family: specimenPane.fontFamily
                    font.pixelSize: 32; text: root.fontSampleText
                }
                Text {
                    Layout.fillWidth: true; wrapMode: Text.Wrap; color: "#CBD5E1"
                    font.family: specimenPane.fontFamily
                    font.pixelSize: 20; text: root.fontSampleText
                }
                Text {
                    Layout.fillWidth: true; wrapMode: Text.Wrap; color: "#94A3B8"
                    font.family: specimenPane.fontFamily
                    font.pixelSize: 14; text: "ABCDEFGHIJKLMNOPQRSTUVWXYZ\nabcdefghijklmnopqrstuvwxyz\n0123456789 !@#$%^&*()[]{}"
                }
            }
        }
    }

    // Reusable small pill button
    component PillButton: Button {
        property bool active: false
        property color activeBg: "#0C2D48"
        property color activeBorder: "#38BDF8"
        property color activeText: "#7DD3FC"

        hoverEnabled: true
        implicitHeight: 24
        leftPadding: 9
        rightPadding: 9
        topPadding: 0
        bottomPadding: 0

        background: Rectangle {
            radius: 5
            color: parent.active ? parent.activeBg : (parent.hovered ? "#1E293B" : "transparent")
            border.color: parent.active ? parent.activeBorder : (parent.hovered ? "#334155" : "transparent")
            border.width: 1
        }
        contentItem: Text {
            text: parent.text
            color: parent.active ? parent.activeText : (parent.hovered ? "#F8FAFC" : "#94A3B8")
            font.pixelSize: 11
            font.bold: parent.active
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // ==========================================================
        // 1. RICH PREVIEW TOOLBAR
        // ==========================================================
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 40
            color: "#11161F"

            Rectangle {
                anchors.bottom: parent.bottom
                width: parent.width
                height: 1
                color: "#1E293B"
            }

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 12
                anchors.rightMargin: 12
                spacing: 8

                // Format pill
                Rectangle {
                    implicitHeight: 22
                    implicitWidth: fmtLabel.implicitWidth + 14
                    radius: 5
                    color: "#172030"
                    border.color: "#28374E"
                    border.width: 1
                    Text {
                        id: fmtLabel
                        anchors.centerIn: parent
                        text: root.kindIcon(root.fileKind) + "  " + (root.hasViewer ? (root.diffViewer.formatDescription || "File binario") : "")
                        color: "#E2E8F0"
                        font.pixelSize: 11
                        font.bold: true
                    }
                }

                // Size comparison summary
                Text {
                    visible: root.hasViewer && (root.diffViewer.hasOldBlob || root.diffViewer.hasNewBlob)
                    text: {
                        if (!root.diffViewer) return "";
                        const o = root.diffViewer.oldFileSizeText || "";
                        const n = root.diffViewer.newFileSizeText || "";
                        const d = root.diffViewer.fileSizeDeltaText || "";
                        if (o.length > 0 && n.length > 0) return o + " → " + n + (d.length > 0 ? ("  (" + d + ")") : "");
                        if (n.length > 0) return n;
                        return o;
                    }
                    color: "#94A3B8"
                    font.pixelSize: 11
                    elide: Text.ElideRight
                }

                // Pixel diff stat chip (for modified images)
                Rectangle {
                    visible: root.hasBothImgs && root.hasViewer
                    implicitHeight: 20
                    implicitWidth: pxDiffText.implicitWidth + 12
                    radius: 10
                    color: "#2D1220"
                    border.color: "#9F1239"
                    border.width: 1
                    Text {
                        id: pxDiffText
                        anchors.centerIn: parent
                        text: root.hasViewer
                            ? (root.diffViewer.imageDiffPercent.toFixed(root.diffViewer.imageDiffPercent < 1.0 ? 2 : 1) + "% pixel modificati")
                            : ""
                        color: "#FDA4AF"
                        font.pixelSize: 10
                        font.bold: true
                    }
                }

                Item { Layout.fillWidth: true }

                // Image Comparison Mode Switcher (only when both Before & After images exist & on Preview tab)
                Rectangle {
                    visible: root.activeTab === "preview" && root.hasBothImgs
                    implicitHeight: 28
                    implicitWidth: imgModeRow.implicitWidth + 6
                    radius: 6
                    color: "#0A0E15"
                    border.color: "#1E293B"
                    border.width: 1

                    RowLayout {
                        id: imgModeRow
                        anchors.centerIn: parent
                        spacing: 2

                        PillButton {
                            text: "Affiancata"
                            active: root.imageMode === "side"
                            onClicked: root.imageMode = "side"
                        }
                        PillButton {
                            text: "Scorrimento"
                            active: root.imageMode === "swipe"
                            onClicked: root.imageMode = "swipe"
                        }
                        PillButton {
                            text: "Sovrapposizione"
                            active: root.imageMode === "onion"
                            onClicked: root.imageMode = "onion"
                        }
                        PillButton {
                            visible: root.hasDiffImg
                            text: "Differenza"
                            active: root.imageMode === "diff"
                            activeBg: "#3B1224"
                            activeBorder: "#F43F5E"
                            activeText: "#FDA4AF"
                            onClicked: root.imageMode = "diff"
                        }
                    }
                }

                // Backdrop + Zoom Controls (whenever visual preview is shown)
                RowLayout {
                    visible: root.activeTab === "preview" && root.hasVisual
                    spacing: 3

                    PillButton {
                        text: root.backdropMode === "checker-dark" ? "▦ Scuro"
                            : (root.backdropMode === "checker-light" ? "▦ Chiaro"
                               : (root.backdropMode === "solid-dark" ? "■ Nero" : "□ Bianco"))
                        ToolTip.visible: hovered
                        ToolTip.text: "Cambia sfondo trasparenza"
                        onClicked: {
                            if (root.backdropMode === "checker-dark") root.backdropMode = "checker-light";
                            else if (root.backdropMode === "checker-light") root.backdropMode = "solid-dark";
                            else if (root.backdropMode === "solid-dark") root.backdropMode = "solid-light";
                            else root.backdropMode = "checker-dark";
                        }
                    }

                    PillButton {
                        text: "−"
                        onClicked: root.stepZoom(0.8)
                    }
                    PillButton {
                        text: root.zoomFactor <= 0 ? "Adatta" : (Math.round(root.zoomFactor * 100) + "%")
                        active: root.zoomFactor > 0
                        onClicked: root.zoomFactor = (root.zoomFactor <= 0 ? 1.0 : 0)
                    }
                    PillButton {
                        text: "+"
                        onClicked: root.stepZoom(1.25)
                    }
                }

                // Preview vs Hex switch (for all non-SVG binary files that have hex rows)
                Rectangle {
                    visible: root.hasHex
                    implicitHeight: 28
                    implicitWidth: tabSwitchRow.implicitWidth + 6
                    radius: 6
                    color: "#0A0E15"
                    border.color: "#1E293B"
                    border.width: 1

                    RowLayout {
                        id: tabSwitchRow
                        anchors.centerIn: parent
                        spacing: 2

                        PillButton {
                            text: root.hasVisual ? "Anteprima" : (root.hasArchive ? "Contenuto" : (root.hasFont ? "Font" : "Dettagli"))
                            active: root.activeTab === "preview"
                            onClicked: root.activeTab = "preview"
                        }
                        PillButton {
                            text: "Hex"
                            active: root.activeTab === "hex"
                            onClicked: root.activeTab = "hex"
                        }
                    }
                }

                // Open externally & reveal in folder
                PillButton {
                    visible: root.hasViewer && (root.diffViewer.hasNewBlob || root.diffViewer.hasOldBlob)
                    text: "↗ Apri"
                    ToolTip.visible: hovered
                    ToolTip.text: "Apri con l'applicazione predefinita di sistema"
                    onClicked: {
                        if (!root.diffViewer) return;
                        root.diffViewer.openPreviewExternally(root.diffViewer.hasNewBlob ? "new" : "old");
                    }
                }

                PillButton {
                    visible: root.hasViewer && root.diffViewer.canRevealInFileManager
                    text: "📂"
                    ToolTip.visible: hovered
                    ToolTip.text: "Apri cartella contenitore"
                    onClicked: if (root.diffViewer) root.diffViewer.revealInFileManager()
                }
            }
        }

        // Optional truncation / info notice banner
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: noticeText.implicitHeight + 12
            visible: root.hasViewer && root.diffViewer.previewNotice && root.diffViewer.previewNotice.length > 0
            color: "#1C1917"
            border.color: "#78350F"
            border.width: 1

            Text {
                id: noticeText
                anchors.fill: parent
                anchors.leftMargin: 12
                anchors.rightMargin: 12
                verticalAlignment: Text.AlignVCenter
                text: "ℹ️ " + (root.hasViewer ? root.diffViewer.previewNotice : "")
                color: "#FDE68A"
                font.pixelSize: 11
                wrapMode: Text.Wrap
            }
        }

        // ==========================================================
        // 2. MAIN CONTENT STACK
        // ==========================================================
        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true

            // ------------------------------------------------------
            // A. VISUAL IMAGE / SVG / PDF-PAGE PREVIEW & DIFF
            // ------------------------------------------------------
            Item {
                anchors.fill: parent
                visible: root.activeTab === "preview" && root.hasVisual

                // Ctrl+Wheel zoom anywhere on the image canvas
                WheelHandler {
                    acceptedModifiers: Qt.ControlModifier
                    onWheel: function(event) {
                        if (event.angleDelta.y > 0) root.stepZoom(1.15);
                        else if (event.angleDelta.y < 0) root.stepZoom(0.87);
                    }
                }

                // A1. SIDE-BY-SIDE MODE (or Single Image when added/deleted)
                RowLayout {
                    anchors.fill: parent
                    anchors.margins: 16
                    spacing: 14
                    visible: root.imageMode === "side" || !root.hasBothImgs

                    // BEFORE pane
                    Rectangle {
                        visible: root.hasOldImg
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        radius: 10
                        color: "#0A0E15"
                        border.color: "#4C1D24"
                        border.width: 1
                        clip: true

                        ColumnLayout {
                            anchors.fill: parent
                            spacing: 0

                            Rectangle {
                                Layout.fillWidth: true
                                Layout.preferredHeight: 32
                                color: "#1F1216"
                                RowLayout {
                                    anchors.fill: parent
                                    anchors.leftMargin: 12
                                    anchors.rightMargin: 12
                                    spacing: 8
                                    Rectangle {
                                        implicitHeight: 18
                                        implicitWidth: beforeBadge.implicitWidth + 10
                                        radius: 4
                                        color: "#7F1D1D"
                                        Text {
                                            id: beforeBadge
                                            anchors.centerIn: parent
                                            text: root.hasNewImg ? "PRIMA" : "ELIMINATA"
                                            color: "#FECACA"
                                            font.pixelSize: 10
                                            font.bold: true
                                        }
                                    }
                                    Text {
                                        text: root.hasViewer && root.diffViewer.oldImageWidth > 0
                                            ? (root.diffViewer.oldImageWidth + " × " + root.diffViewer.oldImageHeight + " px")
                                            : ""
                                        color: "#E2E8F0"
                                        font.pixelSize: 11
                                        font.bold: true
                                    }
                                    Text {
                                        text: root.hasViewer ? root.diffViewer.oldFileSizeText : ""
                                        color: "#94A3B8"
                                        font.pixelSize: 11
                                    }
                                    Item { Layout.fillWidth: true }
                                    PillButton {
                                        text: "↗"
                                        ToolTip.visible: hovered
                                        ToolTip.text: "Apri versione precedente"
                                        onClicked: if (root.diffViewer) root.diffViewer.openPreviewExternally("old")
                                    }
                                }
                            }

                            ZoomableImageCanvas {
                                Layout.fillWidth: true
                                Layout.fillHeight: true
                                imageSource: root.hasOldImg ? root.diffViewer.oldImageUrl : ""
                                naturalWidth: (root.hasViewer && root.diffViewer.oldImageWidth > 0) ? root.diffViewer.oldImageWidth : 0
                                naturalHeight: (root.hasViewer && root.diffViewer.oldImageHeight > 0) ? root.diffViewer.oldImageHeight : 0
                            }
                        }
                    }

                    // AFTER pane
                    Rectangle {
                        visible: root.hasNewImg
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        radius: 10
                        color: "#0A0E15"
                        border.color: "#14532D"
                        border.width: 1
                        clip: true

                        ColumnLayout {
                            anchors.fill: parent
                            spacing: 0

                            Rectangle {
                                Layout.fillWidth: true
                                Layout.preferredHeight: 32
                                color: "#0E2318"
                                RowLayout {
                                    anchors.fill: parent
                                    anchors.leftMargin: 12
                                    anchors.rightMargin: 12
                                    spacing: 8
                                    Rectangle {
                                        implicitHeight: 18
                                        implicitWidth: afterBadge.implicitWidth + 10
                                        radius: 4
                                        color: "#065F46"
                                        Text {
                                            id: afterBadge
                                            anchors.centerIn: parent
                                            text: root.hasOldImg ? "DOPO" : "NUOVA"
                                            color: "#A7F3D0"
                                            font.pixelSize: 10
                                            font.bold: true
                                        }
                                    }
                                    Text {
                                        text: root.hasViewer && root.diffViewer.newImageWidth > 0
                                            ? (root.diffViewer.newImageWidth + " × " + root.diffViewer.newImageHeight + " px")
                                            : ""
                                        color: "#E2E8F0"
                                        font.pixelSize: 11
                                        font.bold: true
                                    }
                                    Text {
                                        text: root.hasViewer ? root.diffViewer.newFileSizeText : ""
                                        color: "#94A3B8"
                                        font.pixelSize: 11
                                    }
                                    Item { Layout.fillWidth: true }
                                    PillButton {
                                        text: "↗"
                                        ToolTip.visible: hovered
                                        ToolTip.text: "Apri versione attuale"
                                        onClicked: if (root.diffViewer) root.diffViewer.openPreviewExternally("new")
                                    }
                                }
                            }

                            ZoomableImageCanvas {
                                Layout.fillWidth: true
                                Layout.fillHeight: true
                                imageSource: root.hasNewImg ? root.diffViewer.newImageUrl : ""
                                naturalWidth: (root.hasViewer && root.diffViewer.newImageWidth > 0) ? root.diffViewer.newImageWidth : 0
                                naturalHeight: (root.hasViewer && root.diffViewer.newImageHeight > 0) ? root.diffViewer.newImageHeight : 0
                            }
                        }
                    }
                }

                // A2. SWIPE (CURTAIN SLIDER) MODE
                Item {
                    anchors.fill: parent
                    anchors.margins: 16
                    visible: root.hasBothImgs && root.imageMode === "swipe"

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

                            // Right side (AFTER)
                            Image {
                                id: swipeAfter
                                anchors.fill: parent
                                source: root.hasNewImg ? root.diffViewer.newImageUrl : ""
                                cache: false
                                smooth: root.zoomFactor < 2.0
                                mipmap: true
                                fillMode: Image.PreserveAspectFit
                            }

                            // Left clipped side (BEFORE)
                            Item {
                                anchors.left: parent.left
                                anchors.top: parent.top
                                anchors.bottom: parent.bottom
                                width: Math.max(0, Math.min(parent.width, parent.width * root.swipeRatio))
                                clip: true

                                Image {
                                    id: swipeBefore
                                    x: 0
                                    y: 0
                                    width: swipeStage.width
                                    height: swipeStage.height
                                    source: root.hasOldImg ? root.diffViewer.oldImageUrl : ""
                                    cache: false
                                    smooth: root.zoomFactor < 2.0
                                    mipmap: true
                                    fillMode: Image.PreserveAspectFit
                                }
                            }

                            // Divider line + handle
                            Rectangle {
                                x: Math.round(swipeStage.width * root.swipeRatio) - 1
                                y: 0
                                width: 2
                                height: swipeStage.height
                                color: "#38BDF8"

                                Rectangle {
                                    anchors.centerIn: parent
                                    width: 28
                                    height: 28
                                    radius: 14
                                    color: "#0F172A"
                                    border.color: "#38BDF8"
                                    border.width: 2
                                    Text {
                                        anchors.centerIn: parent
                                        text: "↔"
                                        color: "#38BDF8"
                                        font.pixelSize: 13
                                        font.bold: true
                                    }
                                }
                            }

                            MouseArea {
                                anchors.fill: parent
                                cursorShape: Qt.SplitHCursor
                                onPressed: function(mouse) {
                                    root.swipeRatio = Math.max(0.0, Math.min(1.0, mouse.x / Math.max(1, width)));
                                }
                                onPositionChanged: function(mouse) {
                                    if (pressed) {
                                        root.swipeRatio = Math.max(0.0, Math.min(1.0, mouse.x / Math.max(1, width)));
                                    }
                                }
                            }
                        }

                        // Corner labels
                        Rectangle {
                            anchors.left: parent.left
                            anchors.top: parent.top
                            anchors.margins: 12
                            implicitHeight: 22
                            implicitWidth: swLbl1.implicitWidth + 12
                            radius: 5
                            color: "#CC7F1D1D"
                            Text { id: swLbl1; anchors.centerIn: parent; text: "◀ PRIMA"; color: "#FECACA"; font.pixelSize: 10; font.bold: true }
                        }
                        Rectangle {
                            anchors.right: parent.right
                            anchors.top: parent.top
                            anchors.margins: 12
                            implicitHeight: 22
                            implicitWidth: swLbl2.implicitWidth + 12
                            radius: 5
                            color: "#CC065F46"
                            Text { id: swLbl2; anchors.centerIn: parent; text: "DOPO ▶"; color: "#A7F3D0"; font.pixelSize: 10; font.bold: true }
                        }
                    }
                }

                // A3. ONION SKIN (OPACITY OVERLAY) MODE
                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 16
                    spacing: 10
                    visible: root.hasBothImgs && root.imageMode === "onion"

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
                                source: root.hasOldImg ? root.diffViewer.oldImageUrl : ""
                                cache: false
                                smooth: root.zoomFactor < 2.0
                                mipmap: true
                                fillMode: Image.PreserveAspectFit
                            }
                            Image {
                                id: onionAfter
                                anchors.fill: parent
                                source: root.hasNewImg ? root.diffViewer.newImageUrl : ""
                                opacity: root.onionOpacity
                                cache: false
                                smooth: root.zoomFactor < 2.0
                                mipmap: true
                                fillMode: Image.PreserveAspectFit
                            }
                        }
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 38
                        radius: 8
                        color: "#11161F"
                        border.color: "#1E293B"
                        border.width: 1

                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 16
                            anchors.rightMargin: 16
                            spacing: 12

                            Text { text: "PRIMA"; color: "#FCA5A5"; font.pixelSize: 11; font.bold: true }
                            Slider {
                                Layout.fillWidth: true
                                from: 0.0
                                to: 1.0
                                value: root.onionOpacity
                                onMoved: root.onionOpacity = value
                            }
                            Text { text: "DOPO"; color: "#6EE7B7"; font.pixelSize: 11; font.bold: true }
                            Text {
                                text: Math.round(root.onionOpacity * 100) + "%"
                                color: "#38BDF8"
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
                    visible: root.hasBothImgs && root.hasDiffImg && root.imageMode === "diff"

                    Rectangle {
                        anchors.fill: parent
                        radius: 10
                        color: "#0A0E15"
                        border.color: "#9F1239"
                        border.width: 1
                        clip: true

                        ZoomableImageCanvas {
                            anchors.fill: parent
                            preferImplicitSize: true
                            imageSource: root.hasDiffImg ? root.diffViewer.diffImageUrl : ""
                            naturalWidth: Math.max(root.hasViewer ? root.diffViewer.oldImageWidth : 0,
                                                   root.hasViewer ? root.diffViewer.newImageWidth : 0)
                            naturalHeight: Math.max(root.hasViewer ? root.diffViewer.oldImageHeight : 0,
                                                    root.hasViewer ? root.diffViewer.newImageHeight : 0)
                        }

                        Rectangle {
                            anchors.left: parent.left
                            anchors.bottom: parent.bottom
                            anchors.margins: 12
                            implicitHeight: 24
                            implicitWidth: diffLegendRow.implicitWidth + 16
                            radius: 6
                            color: "#DD0F172A"
                            border.color: "#334155"
                            border.width: 1

                            RowLayout {
                                id: diffLegendRow
                                anchors.centerIn: parent
                                spacing: 8
                                Rectangle { width: 10; height: 10; radius: 2; color: "#F43F5E" }
                                Text {
                                    text: "Pixel modificati (" + (root.hasViewer ? root.diffViewer.imageDiffPercent.toFixed(2) : "0") + "%)"
                                    color: "#F8FAFC"
                                    font.pixelSize: 11
                                    font.bold: true
                                }
                            }
                        }
                    }
                }
            }

            // ------------------------------------------------------
            // B. ARCHIVE CONTENTS DIFF VIEW (ZIP, JAR, APK, DOCX...)
            // ------------------------------------------------------
            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 16
                spacing: 10
                visible: root.activeTab === "preview" && !root.hasVisual && root.hasArchive

                // Summary + search bar
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8

                    Rectangle {
                        visible: root.hasViewer && root.diffViewer.archiveAddedCount > 0
                        implicitHeight: 24; implicitWidth: arAddTxt.implicitWidth + 12; radius: 5; color: "#064E3B"; border.color: "#059669"
                        Text { id: arAddTxt; anchors.centerIn: parent; text: "+" + (root.hasViewer ? root.diffViewer.archiveAddedCount : 0) + " aggiunti"; color: "#A7F3D0"; font.pixelSize: 11; font.bold: true }
                    }
                    Rectangle {
                        visible: root.hasViewer && root.diffViewer.archiveModifiedCount > 0
                        implicitHeight: 24; implicitWidth: arModTxt.implicitWidth + 12; radius: 5; color: "#451A03"; border.color: "#D97706"
                        Text { id: arModTxt; anchors.centerIn: parent; text: "~" + (root.hasViewer ? root.diffViewer.archiveModifiedCount : 0) + " modificati"; color: "#FDE68A"; font.pixelSize: 11; font.bold: true }
                    }
                    Rectangle {
                        visible: root.hasViewer && root.diffViewer.archiveRemovedCount > 0
                        implicitHeight: 24; implicitWidth: arRemTxt.implicitWidth + 12; radius: 5; color: "#450A0A"; border.color: "#DC2626"
                        Text { id: arRemTxt; anchors.centerIn: parent; text: "-" + (root.hasViewer ? root.diffViewer.archiveRemovedCount : 0) + " rimossi"; color: "#FECACA"; font.pixelSize: 11; font.bold: true }
                    }
                    Rectangle {
                        visible: root.hasViewer && root.diffViewer.archiveUnchangedCount > 0
                        implicitHeight: 24; implicitWidth: arUnchTxt.implicitWidth + 12; radius: 5; color: "#1E293B"; border.color: "#334155"
                        Text { id: arUnchTxt; anchors.centerIn: parent; text: (root.hasViewer ? root.diffViewer.archiveUnchangedCount : 0) + " invariati"; color: "#94A3B8"; font.pixelSize: 11 }
                    }

                    Item { Layout.fillWidth: true }

                    PillButton {
                        text: "Solo modifiche"
                        active: root.archiveOnlyChanged
                        onClicked: root.archiveOnlyChanged = !root.archiveOnlyChanged
                    }

                    TextField {
                        Layout.preferredWidth: 220
                        Layout.preferredHeight: 26
                        placeholderText: "Filtra file nell'archivio..."
                        placeholderTextColor: "#64748B"
                        color: "#F8FAFC"
                        font.pixelSize: 11
                        text: root.archiveFilterText
                        onTextChanged: root.archiveFilterText = text
                        background: Rectangle {
                            color: "#0A0E15"
                            radius: 5
                            border.color: parent.activeFocus ? "#38BDF8" : "#334155"
                        }
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    radius: 8
                    color: "#0A0E15"
                    border.color: "#1E293B"
                    border.width: 1
                    clip: true

                    ListView {
                        id: archiveListView
                        anchors.fill: parent
                        anchors.margins: 4
                        spacing: 2
                        boundsBehavior: Flickable.StopAtBounds
                        ScrollBar.vertical: FluxScrollBar {}
                        model: {
                            if (!root.hasViewer || !root.diffViewer.archiveEntries) return [];
                            const q = root.archiveFilterText.trim().toLowerCase();
                            return root.diffViewer.archiveEntries.filter((e) => {
                                if (root.archiveOnlyChanged && e.status === "unchanged") return false;
                                return q.length === 0 || e.path.toLowerCase().indexOf(q) >= 0;
                            });
                        }

                        delegate: Rectangle {
                            width: archiveListView.width - 8
                            height: 28
                            radius: 4
                            color: modelData.status === "added" ? "#0C2518"
                                 : (modelData.status === "removed" ? "#261216"
                                    : (modelData.status === "modified" ? "#241B0D" : "transparent"))

                            RowLayout {
                                anchors.fill: parent
                                anchors.leftMargin: 10
                                anchors.rightMargin: 10
                                spacing: 8

                                Text {
                                    Layout.preferredWidth: 16
                                    horizontalAlignment: Text.AlignHCenter
                                    text: modelData.status === "added" ? "+"
                                        : (modelData.status === "removed" ? "−"
                                           : (modelData.status === "modified" ? "~" : "•"))
                                    color: modelData.status === "added" ? "#34D399"
                                         : (modelData.status === "removed" ? "#F87171"
                                            : (modelData.status === "modified" ? "#FBBF24" : "#64748B"))
                                    font.family: root.monoFont
                                    font.bold: true
                                    font.pixelSize: 12
                                }

                                Text {
                                    Layout.fillWidth: true
                                    text: modelData.path
                                    color: modelData.status === "removed" ? "#FCA5A5" : "#E2E8F0"
                                    font.family: root.monoFont
                                    font.pixelSize: 11
                                    elide: Text.ElideMiddle
                                }

                                Text {
                                    text: (modelData.oldSizeText && modelData.newSizeText && modelData.oldSizeText !== modelData.newSizeText)
                                        ? (modelData.oldSizeText + " → " + modelData.newSizeText)
                                        : (modelData.newSizeText || modelData.oldSizeText || "")
                                    color: "#94A3B8"
                                    font.family: root.monoFont
                                    font.pixelSize: 11
                                }
                            }
                        }
                    }
                }
            }

            // ------------------------------------------------------
            // C. FONT SPECIMEN PREVIEW (TTF / OTF / TTC)
            // ------------------------------------------------------
            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 16
                spacing: 12
                visible: root.activeTab === "preview" && !root.hasVisual && !root.hasArchive && root.hasFont

                TextField {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 34
                    text: root.fontSampleText
                    onTextChanged: root.fontSampleText = text
                    color: "#F8FAFC"
                    font.pixelSize: 12
                    placeholderText: "Testo di anteprima del font..."
                    placeholderTextColor: "#64748B"
                    background: Rectangle {
                        color: "#0A0E15"
                        radius: 6
                        border.color: parent.activeFocus ? "#38BDF8" : "#334155"
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    spacing: 12

                    // Old font specimen
                    FontSpecimenPane {
                        visible: root.hasViewer && root.diffViewer.oldFontFamily && root.diffViewer.oldFontFamily.length > 0
                        border.color: "#4C1D24"
                        headerText: "PRIMA — " + (root.hasViewer ? root.diffViewer.oldFontFamily : "")
                        headerColor: "#FCA5A5"
                        fontFamily: root.hasViewer ? root.diffViewer.oldFontFamily : ""
                    }

                    // New font specimen
                    FontSpecimenPane {
                        visible: root.hasViewer && root.diffViewer.newFontFamily && root.diffViewer.newFontFamily.length > 0
                        border.color: "#14532D"
                        headerText: ((root.hasViewer && root.diffViewer.oldFontFamily.length > 0) ? "DOPO — " : "FONT — ") +
                                    (root.hasViewer ? root.diffViewer.newFontFamily : "")
                        headerColor: "#6EE7B7"
                        fontFamily: root.hasViewer ? root.diffViewer.newFontFamily : ""
                    }
                }
            }

            // ------------------------------------------------------
            // D. RICH METADATA INSPECTOR CARD (PDF, Audio, Video, DB, ELF, Binary)
            // ------------------------------------------------------
            Flickable {
                anchors.fill: parent
                visible: root.activeTab === "preview" && !root.hasVisual && !root.hasArchive && !root.hasFont
                contentWidth: width
                contentHeight: metaCardCol.implicitHeight + 48
                boundsBehavior: Flickable.StopAtBounds
                ScrollBar.vertical: FluxScrollBar {}

                ColumnLayout {
                    id: metaCardCol
                    anchors.horizontalCenter: parent.horizontalCenter
                    y: 24
                    width: Math.min(parent.width - 48, 720)
                    spacing: 14

                    // Hero summary card
                    Rectangle {
                        Layout.fillWidth: true
                        implicitHeight: heroRow.implicitHeight + 28
                        radius: 10
                        color: "#11161F"
                        border.color: "#1E293B"
                        border.width: 1

                        RowLayout {
                            id: heroRow
                            anchors.fill: parent
                            anchors.margins: 14
                            spacing: 14

                            Rectangle {
                                width: 46
                                height: 46
                                radius: 10
                                color: "#172030"
                                border.color: "#28374E"
                                Text {
                                    anchors.centerIn: parent
                                    text: root.kindIcon(root.fileKind)
                                    font.pixelSize: 22
                                }
                            }

                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 3
                                Text {
                                    text: root.hasViewer ? (root.diffViewer.formatDescription || "File binario") : ""
                                    color: "#F8FAFC"
                                    font.pixelSize: 15
                                    font.bold: true
                                }
                                Text {
                                    text: root.hasViewer ? root.diffViewer.filePath : ""
                                    color: "#94A3B8"
                                    font.family: root.monoFont
                                    font.pixelSize: 11
                                    elide: Text.ElideMiddle
                                    Layout.fillWidth: true
                                }
                            }

                            RowLayout {
                                spacing: 6
                                PillButton {
                                    visible: root.hasViewer && root.diffViewer.hasOldBlob && root.diffViewer.hasNewBlob
                                    text: "↗ Prima"
                                    onClicked: if (root.diffViewer) root.diffViewer.openPreviewExternally("old")
                                }
                                PillButton {
                                    visible: root.hasViewer && (root.diffViewer.hasNewBlob || root.diffViewer.hasOldBlob)
                                    text: (root.hasViewer && root.diffViewer.hasOldBlob && root.diffViewer.hasNewBlob) ? "↗ Dopo" : "↗ Apri nel sistema"
                                    active: true
                                    onClicked: {
                                        if (!root.diffViewer) return;
                                        root.diffViewer.openPreviewExternally(root.diffViewer.hasNewBlob ? "new" : "old");
                                    }
                                }
                            }
                        }
                    }

                    // Before vs After Metadata Table
                    Rectangle {
                        Layout.fillWidth: true
                        visible: root.hasViewer && root.diffViewer.metadataRows && root.diffViewer.metadataRows.length > 0
                        implicitHeight: metaTableCol.implicitHeight + 16
                        radius: 10
                        color: "#0A0E15"
                        border.color: "#1E293B"
                        border.width: 1

                        ColumnLayout {
                            id: metaTableCol
                            anchors.fill: parent
                            anchors.margins: 8
                            spacing: 4

                            // Column header
                            RowLayout {
                                Layout.fillWidth: true
                                Layout.leftMargin: 8
                                Layout.rightMargin: 8
                                Layout.preferredHeight: 24
                                spacing: 10
                                Text { Layout.preferredWidth: 140; text: "PROPRIETÀ"; color: "#64748B"; font.pixelSize: 10; font.bold: true }
                                Text {
                                    visible: root.hasViewer && root.diffViewer.hasOldBlob
                                    Layout.fillWidth: true; text: "PRIMA"; color: "#FCA5A5"; font.pixelSize: 10; font.bold: true
                                }
                                Text {
                                    visible: root.hasViewer && root.diffViewer.hasNewBlob
                                    Layout.fillWidth: true; text: (root.hasViewer && root.diffViewer.hasOldBlob) ? "DOPO" : "VALORE"; color: "#6EE7B7"; font.pixelSize: 10; font.bold: true
                                }
                            }

                            Rectangle { Layout.fillWidth: true; height: 1; color: "#1E293B" }

                            Repeater {
                                model: root.hasViewer ? root.diffViewer.metadataRows : []
                                delegate: Rectangle {
                                    Layout.fillWidth: true
                                    implicitHeight: 30
                                    radius: 5
                                    color: modelData.changed ? "#172030" : "transparent"

                                    RowLayout {
                                        anchors.fill: parent
                                        anchors.leftMargin: 8
                                        anchors.rightMargin: 8
                                        spacing: 10

                                        Text {
                                            Layout.preferredWidth: 140
                                            text: modelData.label
                                            color: "#94A3B8"
                                            font.pixelSize: 11
                                            font.bold: true
                                            elide: Text.ElideRight
                                        }
                                        Text {
                                            visible: root.hasViewer && root.diffViewer.hasOldBlob
                                            Layout.fillWidth: true
                                            text: modelData.oldValue || "—"
                                            color: modelData.changed ? "#FDA4AF" : "#CBD5E1"
                                            font.family: modelData.label === "SHA-256" ? root.monoFont : ""
                                            font.pixelSize: 11
                                            elide: Text.ElideRight
                                        }
                                        Text {
                                            visible: root.hasViewer && root.diffViewer.hasNewBlob
                                            Layout.fillWidth: true
                                            text: modelData.newValue || "—"
                                            color: modelData.changed ? "#86EFAC" : "#E2E8F0"
                                            font.family: modelData.label === "SHA-256" ? root.monoFont : ""
                                            font.pixelSize: 11
                                            elide: Text.ElideRight
                                        }
                                    }
                                }
                            }
                        }
                    }

                    // Hint to switch to Hex Inspector
                    RowLayout {
                        Layout.fillWidth: true
                        visible: root.hasHex
                        spacing: 8
                        Item { Layout.fillWidth: true }
                        PillButton {
                            text: "🔍 Ispeziona byte in esadecimale (Hex)"
                            onClicked: root.activeTab = "hex"
                        }
                        Item { Layout.fillWidth: true }
                    }
                }
            }

            // ------------------------------------------------------
            // E. HEX & ASCII DUMP / DIFF VIEW
            // ------------------------------------------------------
            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 12
                spacing: 8
                visible: root.activeTab === "hex" && root.hasHex

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 10

                    Text {
                        text: root.hasViewer && root.diffViewer.hexTruncated
                            ? "Anteprima esadecimale (primi 4 KB)"
                            : "Dump esadecimale completo"
                        color: "#94A3B8"
                        font.pixelSize: 11
                        font.bold: true
                    }

                    Rectangle {
                        visible: root.hasViewer && root.diffViewer.hexChangedRows > 0
                        implicitHeight: 20
                        implicitWidth: hexDiffBadge.implicitWidth + 10
                        radius: 5
                        color: "#241B0D"
                        border.color: "#D97706"
                        Text {
                            id: hexDiffBadge
                            anchors.centerIn: parent
                            text: (root.hasViewer ? root.diffViewer.hexChangedRows : 0) + " righe modificate"
                            color: "#FDE68A"
                            font.pixelSize: 10
                            font.bold: true
                        }
                    }

                    Item { Layout.fillWidth: true }

                    PillButton {
                        visible: root.hasViewer && root.diffViewer.hasOldBlob && root.diffViewer.hasNewBlob
                        text: "Solo righe modificate"
                        active: root.hexOnlyChanged
                        onClicked: root.hexOnlyChanged = !root.hexOnlyChanged
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    radius: 8
                    color: "#0A0E15"
                    border.color: "#1E293B"
                    border.width: 1
                    clip: true

                    ListView {
                        id: hexListView
                        anchors.fill: parent
                        anchors.margins: 6
                        spacing: 2
                        boundsBehavior: Flickable.StopAtBounds
                        ScrollBar.vertical: FluxScrollBar {}
                        model: {
                            if (!root.hasViewer || !root.diffViewer.hexRows) return [];
                            if (!root.hexOnlyChanged) return root.diffViewer.hexRows;
                            return root.diffViewer.hexRows.filter((row) => row.hasDiff);
                        }

                        delegate: Rectangle {
                            width: hexListView.width - 8
                            height: (root.hasViewer && root.diffViewer.hasOldBlob && root.diffViewer.hasNewBlob && modelData.hasDiff) ? 44 : 22
                            radius: 3
                            color: modelData.hasDiff ? "#141A24" : "transparent"

                            ColumnLayout {
                                anchors.fill: parent
                                anchors.leftMargin: 8
                                anchors.rightMargin: 8
                                spacing: 1

                                // Single or New row (or Old if deleted)
                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: 12
                                    visible: !(root.hasViewer && root.diffViewer.hasOldBlob && root.diffViewer.hasNewBlob && modelData.hasDiff)

                                    Text {
                                        text: modelData.offset
                                        color: "#64748B"
                                        font.family: root.monoFont
                                        font.pixelSize: 11
                                    }
                                    Text {
                                        Layout.fillWidth: true
                                        text: (root.hasViewer && root.diffViewer.hasNewBlob) ? modelData.newHex : modelData.oldHex
                                        color: "#CBD5E1"
                                        font.family: root.monoFont
                                        font.pixelSize: 11
                                    }
                                    Text {
                                        Layout.preferredWidth: 140
                                        text: (root.hasViewer && root.diffViewer.hasNewBlob) ? modelData.newAscii : modelData.oldAscii
                                        color: "#94A3B8"
                                        font.family: root.monoFont
                                        font.pixelSize: 11
                                    }
                                }

                                // Changed row: Old line
                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: 12
                                    visible: root.hasViewer && root.diffViewer.hasOldBlob && root.diffViewer.hasNewBlob && modelData.hasDiff

                                    Text { text: modelData.offset + " -"; color: "#F87171"; font.family: root.monoFont; font.pixelSize: 11; font.bold: true }
                                    Text {
                                        Layout.fillWidth: true
                                        textFormat: Text.RichText
                                        text: modelData.oldRichHex
                                        color: "#94A3B8"
                                        font.family: root.monoFont
                                        font.pixelSize: 11
                                    }
                                    Text { Layout.preferredWidth: 140; text: modelData.oldAscii; color: "#FDA4AF"; font.family: root.monoFont; font.pixelSize: 11 }
                                }

                                // Changed row: New line
                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: 12
                                    visible: root.hasViewer && root.diffViewer.hasOldBlob && root.diffViewer.hasNewBlob && modelData.hasDiff

                                    Text { text: modelData.offset + " +"; color: "#34D399"; font.family: root.monoFont; font.pixelSize: 11; font.bold: true }
                                    Text {
                                        Layout.fillWidth: true
                                        textFormat: Text.RichText
                                        text: modelData.newRichHex
                                        color: "#CBD5E1"
                                        font.family: root.monoFont
                                        font.pixelSize: 11
                                    }
                                    Text { Layout.preferredWidth: 140; text: modelData.newAscii; color: "#86EFAC"; font.family: root.monoFont; font.pixelSize: 11 }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}
