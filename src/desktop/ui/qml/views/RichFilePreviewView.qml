import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../themes"

Rectangle {
    id: root
    color: theme.canvas

    readonly property FluxTheme theme: FluxTheme {}

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
    // Font preview text
    property string fontSampleText: "The quick brown fox jumps over the lazy dog — 0123456789"

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
            imagePreview.swipeRatio = 0.5;
            imagePreview.onionOpacity = 0.5;
            archivePreview.archiveFilterText = "";
            archivePreview.archiveOnlyChanged = false;
            hexPreview.hexOnlyChanged = false;
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

    // Reusable font specimen preview card
    component FontSpecimenPane: Rectangle {
        id: specimenPane
        property string headerText: ""
        property color headerColor: theme.dangerText
        property string fontFamily: ""

        Layout.fillWidth: true
        Layout.fillHeight: true
        radius: 10
        color: theme.inputBg
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
                    Layout.fillWidth: true; wrapMode: Text.Wrap; color: theme.textPrimary
                    font.family: specimenPane.fontFamily
                    font.pixelSize: 32; text: root.fontSampleText
                }
                Text {
                    Layout.fillWidth: true; wrapMode: Text.Wrap; color: theme.textSecondary
                    font.family: specimenPane.fontFamily
                    font.pixelSize: 20; text: root.fontSampleText
                }
                Text {
                    Layout.fillWidth: true; wrapMode: Text.Wrap; color: theme.textMuted
                    font.family: specimenPane.fontFamily
                    font.pixelSize: 14; text: "ABCDEFGHIJKLMNOPQRSTUVWXYZ\nabcdefghijklmnopqrstuvwxyz\n0123456789 !@#$%^&*()[]{}"
                }
            }
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
            color: theme.surface

            Rectangle {
                anchors.bottom: parent.bottom
                width: parent.width
                height: 1
                color: theme.borderSubtle
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
                    color: theme.surfaceElevated
                    border.color: theme.border
                    border.width: 1
                    Text {
                        id: fmtLabel
                        anchors.centerIn: parent
                        text: root.kindIcon(root.fileKind) + "  " + (root.hasViewer ? (root.diffViewer.formatDescription || "File binario") : "")
                        color: theme.textSecondary
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
                    color: theme.textMuted
                    font.pixelSize: 11
                    elide: Text.ElideRight
                }

                // Pixel diff stat chip (for modified images)
                Rectangle {
                    visible: root.hasBothImgs && root.hasViewer
                    implicitHeight: 20
                    implicitWidth: pxDiffText.implicitWidth + 12
                    radius: 10
                    color: theme.dangerBg
                    border.color: theme.danger
                    border.width: 1
                    Text {
                        id: pxDiffText
                        anchors.centerIn: parent
                        text: root.hasViewer
                            ? (root.diffViewer.imageDiffPercent.toFixed(root.diffViewer.imageDiffPercent < 1.0 ? 2 : 1) + "% pixel modificati")
                            : ""
                        color: theme.dangerText
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
                    color: theme.inputBg
                    border.color: theme.borderSubtle
                    border.width: 1

                    RowLayout {
                        id: imgModeRow
                        anchors.centerIn: parent
                        spacing: 2

                        RichPreviewPillButton {
                            text: "Affiancata"
                            active: root.imageMode === "side"
                            onClicked: root.imageMode = "side"
                        }
                        RichPreviewPillButton {
                            text: "Scorrimento"
                            active: root.imageMode === "swipe"
                            onClicked: root.imageMode = "swipe"
                        }
                        RichPreviewPillButton {
                            text: "Sovrapposizione"
                            active: root.imageMode === "onion"
                            onClicked: root.imageMode = "onion"
                        }
                        RichPreviewPillButton {
                            visible: root.hasDiffImg
                            text: "Differenza"
                            active: root.imageMode === "diff"
                            activeBg: theme.dangerBg
                            activeBorder: theme.danger
                            activeText: theme.dangerText
                            onClicked: root.imageMode = "diff"
                        }
                    }
                }

                // Backdrop + Zoom Controls (whenever visual preview is shown)
                RowLayout {
                    visible: root.activeTab === "preview" && root.hasVisual
                    spacing: 3

                    RichPreviewPillButton {
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

                    RichPreviewPillButton {
                        text: "−"
                        onClicked: root.stepZoom(0.8)
                    }
                    RichPreviewPillButton {
                        text: root.zoomFactor <= 0 ? "Adatta" : (Math.round(root.zoomFactor * 100) + "%")
                        active: root.zoomFactor > 0
                        onClicked: root.zoomFactor = (root.zoomFactor <= 0 ? 1.0 : 0)
                    }
                    RichPreviewPillButton {
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
                    color: theme.inputBg
                    border.color: theme.borderSubtle
                    border.width: 1

                    RowLayout {
                        id: tabSwitchRow
                        anchors.centerIn: parent
                        spacing: 2

                        RichPreviewPillButton {
                            text: root.hasVisual ? "Anteprima" : (root.hasArchive ? "Contenuto" : (root.hasFont ? "Font" : "Dettagli"))
                            active: root.activeTab === "preview"
                            onClicked: root.activeTab = "preview"
                        }
                        RichPreviewPillButton {
                            text: "Hex"
                            active: root.activeTab === "hex"
                            onClicked: root.activeTab = "hex"
                        }
                    }
                }

                // Open externally & reveal in folder
                RichPreviewPillButton {
                    visible: root.hasViewer && (root.diffViewer.hasNewBlob || root.diffViewer.hasOldBlob)
                    text: "↗ Apri"
                    ToolTip.visible: hovered
                    ToolTip.text: "Apri con l'applicazione predefinita di sistema"
                    onClicked: {
                        if (!root.diffViewer) return;
                        root.diffViewer.openPreviewExternally(root.diffViewer.hasNewBlob ? "new" : "old");
                    }
                }

                RichPreviewPillButton {
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
            color: theme.warningBg
            border.color: theme.warning
            border.width: 1

            Text {
                id: noticeText
                anchors.fill: parent
                anchors.leftMargin: 12
                anchors.rightMargin: 12
                verticalAlignment: Text.AlignVCenter
                text: "ℹ️ " + (root.hasViewer ? root.diffViewer.previewNotice : "")
                color: theme.warningText
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

            // A. VISUAL IMAGE / SVG / PDF-PAGE PREVIEW & DIFF
            ImageDiffPreview {
                id: imagePreview
                anchors.fill: parent
                visible: root.activeTab === "preview" && root.hasVisual
                diffViewer: root.diffViewer
                imageMode: root.imageMode
                backdropMode: root.backdropMode
                zoomFactor: root.zoomFactor
                onZoomStepRequested: function(mult) { root.stepZoom(mult); }
            }

            // B. ARCHIVE CONTENTS DIFF VIEW (ZIP, JAR, APK, DOCX...)
            ArchiveDiffView {
                id: archivePreview
                anchors.fill: parent
                anchors.margins: 16
                visible: root.activeTab === "preview" && !root.hasVisual && root.hasArchive
                diffViewer: root.diffViewer
                monoFont: root.monoFont
            }

            // C. FONT SPECIMEN PREVIEW (TTF / OTF / TTC)
            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 16
                spacing: 12
                visible: root.activeTab === "preview" && !root.hasVisual && !root.hasArchive && root.hasFont

                FluxTextField {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 34
                    text: root.fontSampleText
                    onTextChanged: root.fontSampleText = text
                    font.pixelSize: 12
                    placeholderText: "Testo di anteprima del font..."
                }

                RowLayout {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    spacing: 12

                    FontSpecimenPane {
                        visible: root.hasViewer && root.diffViewer.oldFontFamily && root.diffViewer.oldFontFamily.length > 0
                        border.color: theme.danger
                        headerText: "PRIMA — " + (root.hasViewer ? root.diffViewer.oldFontFamily : "")
                        headerColor: theme.dangerText
                        fontFamily: root.hasViewer ? root.diffViewer.oldFontFamily : ""
                    }

                    FontSpecimenPane {
                        visible: root.hasViewer && root.diffViewer.newFontFamily && root.diffViewer.newFontFamily.length > 0
                        border.color: theme.success
                        headerText: ((root.hasViewer && root.diffViewer.oldFontFamily.length > 0) ? "DOPO — " : "FONT — ") +
                                    (root.hasViewer ? root.diffViewer.newFontFamily : "")
                        headerColor: theme.successText
                        fontFamily: root.hasViewer ? root.diffViewer.newFontFamily : ""
                    }
                }
            }

            // D. RICH METADATA INSPECTOR CARD (PDF, Audio, Video, DB, ELF, Binary)
            BinaryMetadataView {
                anchors.fill: parent
                visible: root.activeTab === "preview" && !root.hasVisual && !root.hasArchive && !root.hasFont
                diffViewer: root.diffViewer
                monoFont: root.monoFont
                fileKindIcon: root.kindIcon(root.fileKind)
                hasHex: root.hasHex
                onSwitchToHexRequested: root.activeTab = "hex"
            }

            // E. HEX & ASCII DUMP / DIFF VIEW
            HexDiffView {
                id: hexPreview
                anchors.fill: parent
                anchors.margins: 12
                visible: root.activeTab === "hex" && root.hasHex
                diffViewer: root.diffViewer
                monoFont: root.monoFont
            }
        }
    }
}
