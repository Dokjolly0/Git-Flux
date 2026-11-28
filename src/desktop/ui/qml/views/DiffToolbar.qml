import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../themes"

Rectangle {
    id: toolbarRoot
    readonly property FluxTheme theme: FluxTheme {}

    property var diffViewer: null
    property bool showBackButton: false
    property bool isMarkdown: false
    property bool isSvgPreviewable: false
    property bool showingMarkdownPreview: false
    property bool showingRichPreview: false
    property bool effectiveWrap: false

    signal backRequested()
    signal toggleWrapRequested()

    implicitHeight: 46
    color: theme.headerBg
    border.color: theme.borderSubtle
    border.width: 1

    component ContextOptionItem: FluxMenuItem {
        font.pixelSize: 11
    }

    component SegmentedPreviewToggle: Rectangle {
        id: segRoot
        property string codeLabel: "<> Diff"
        property string previewLabel: "👁 Anteprima"
        property bool previewActive: false
        signal codeClicked()
        signal previewClicked()

        Layout.preferredHeight: 28
        implicitWidth: segRow.implicitWidth + 6
        color: toolbarRoot.theme.inputBg
        radius: toolbarRoot.theme.radiusSm
        border.color: toolbarRoot.theme.border
        border.width: 1

        RowLayout {
            id: segRow
            anchors.centerIn: parent
            spacing: 3

            Button {
                id: segCodeBtn
                Layout.preferredHeight: 22
                leftPadding: 9
                rightPadding: 9
                hoverEnabled: true
                readonly property bool active: !segRoot.previewActive
                background: Rectangle {
                    color: segCodeBtn.active ? toolbarRoot.theme.infoBg : (segCodeBtn.hovered ? toolbarRoot.theme.card : "transparent")
                    radius: toolbarRoot.theme.radiusXs
                    border.color: segCodeBtn.active ? toolbarRoot.theme.borderFocus : "transparent"
                    border.width: 1
                    Behavior on color { ColorAnimation { duration: 110 } }
                }
                contentItem: Text {
                    text: segRoot.codeLabel
                    color: segCodeBtn.active ? toolbarRoot.theme.borderFocus : toolbarRoot.theme.textSecondary
                    font.pixelSize: 11
                    font.bold: segCodeBtn.active
                }
                onClicked: segRoot.codeClicked()
            }

            Button {
                id: segPrevBtn
                Layout.preferredHeight: 22
                leftPadding: 9
                rightPadding: 9
                hoverEnabled: true
                readonly property bool active: segRoot.previewActive
                background: Rectangle {
                    color: segPrevBtn.active ? toolbarRoot.theme.successBg : (segPrevBtn.hovered ? toolbarRoot.theme.card : "transparent")
                    radius: toolbarRoot.theme.radiusXs
                    border.color: segPrevBtn.active ? toolbarRoot.theme.successText : "transparent"
                    border.width: 1
                    Behavior on color { ColorAnimation { duration: 110 } }
                }
                contentItem: Text {
                    text: segRoot.previewLabel
                    color: segPrevBtn.active ? toolbarRoot.theme.successText : toolbarRoot.theme.textSecondary
                    font.pixelSize: 11
                    font.bold: segPrevBtn.active
                }
                onClicked: segRoot.previewClicked()
            }
        }
    }

    component ViewModeTabButton: Button {
        id: vmBtn
        property string modeId: ""
        property string modeIcon: ""
        property string modeLabel: ""
        readonly property bool isCurrent: toolbarRoot.diffViewer
            ? (modeId === "split"
                ? (toolbarRoot.diffViewer.viewMode === "split" || toolbarRoot.diffViewer.viewMode === "")
                : toolbarRoot.diffViewer.viewMode === modeId)
            : (modeId === "split")

        Layout.preferredHeight: 22
        leftPadding: 9
        rightPadding: 9
        hoverEnabled: true
        background: Rectangle {
            color: vmBtn.isCurrent ? toolbarRoot.theme.infoBg : (vmBtn.hovered ? toolbarRoot.theme.card : "transparent")
            radius: toolbarRoot.theme.radiusXs
            border.color: vmBtn.isCurrent ? toolbarRoot.theme.borderFocus : "transparent"
            border.width: 1
            Behavior on color { ColorAnimation { duration: 110 } }
        }
        contentItem: RowLayout {
            spacing: 4
            Text { text: vmBtn.modeIcon; font.pixelSize: 10 }
            Text {
                text: vmBtn.modeLabel
                color: vmBtn.isCurrent ? toolbarRoot.theme.borderFocus : toolbarRoot.theme.textSecondary
                font.pixelSize: 11
                font.bold: vmBtn.isCurrent
            }
        }
        onClicked: {
            if (toolbarRoot.diffViewer) toolbarRoot.diffViewer.setViewMode(vmBtn.modeId);
        }
    }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 14
        anchors.rightMargin: 14
        spacing: 8

        // Back Button (shown when diff view replaces the graph in GitKraken mode)
        Button {
            visible: toolbarRoot.showBackButton
            Layout.preferredHeight: 30
            leftPadding: 11
            rightPadding: 11
            hoverEnabled: true
            background: Rectangle {
                color: parent.hovered ? toolbarRoot.theme.cardHover : toolbarRoot.theme.card
                radius: toolbarRoot.theme.radiusSm
                border.color: parent.hovered ? toolbarRoot.theme.borderFocus : toolbarRoot.theme.borderHighlight
                border.width: 1
                Behavior on color { ColorAnimation { duration: toolbarRoot.theme.animFast } }
            }
            contentItem: RowLayout {
                spacing: 6
                Text { text: "←"; color: toolbarRoot.theme.borderFocus; font.bold: true; font.pixelSize: 13 }
                Text { text: "Torna al grafo"; color: toolbarRoot.theme.textPrimary; font.bold: true; font.pixelSize: 11 }
            }
            onClicked: toolbarRoot.backRequested()
        }

        Text {
            text: {
                if (!toolbarRoot.diffViewer) return "Nessun file selezionato";
                if (toolbarRoot.diffViewer.isLoading && toolbarRoot.diffViewer.loadingFilePath && toolbarRoot.diffViewer.loadingFilePath !== "") {
                    return toolbarRoot.diffViewer.loadingFilePath;
                }
                return toolbarRoot.diffViewer.filePath !== "" ? toolbarRoot.diffViewer.filePath : "Nessun file selezionato";
            }
            color: toolbarRoot.theme.textPrimary
            font.bold: true
            font.pixelSize: 13
            elide: Text.ElideMiddle
            Layout.fillWidth: true
            Layout.minimumWidth: 80
        }

        // Loading indicator badge during async file switch
        Rectangle {
            visible: Boolean(toolbarRoot.diffViewer && toolbarRoot.diffViewer.isLoading)
            Layout.preferredHeight: 22
            implicitWidth: loadingBadgeTxt.implicitWidth + 14
            radius: toolbarRoot.theme.radiusXs
            color: toolbarRoot.theme.infoBg
            border.color: toolbarRoot.theme.borderFocus
            border.width: 1
            Text {
                id: loadingBadgeTxt
                anchors.centerIn: parent
                text: "⏳ Caricamento..."
                color: toolbarRoot.theme.infoText
                font.pixelSize: 10
                font.bold: true
            }
        }

        // Type Badge
        Rectangle {
            visible: Boolean(toolbarRoot.diffViewer && !toolbarRoot.diffViewer.isEmpty)
            Layout.preferredHeight: 22
            implicitWidth: typeText.implicitWidth + 14
            radius: toolbarRoot.theme.radiusXs
            color: {
                if (!toolbarRoot.diffViewer) return "transparent";
                if (toolbarRoot.diffViewer.isNewFile) return toolbarRoot.theme.success;
                if (toolbarRoot.diffViewer.isDeletedFile) return toolbarRoot.theme.danger;
                if (toolbarRoot.diffViewer.isRenamed) return toolbarRoot.theme.accentBlue;
                if (toolbarRoot.diffViewer.isBinary) return toolbarRoot.theme.textMuted;
                return toolbarRoot.theme.warning;
            }
            Text {
                id: typeText
                anchors.centerIn: parent
                text: {
                    if (!toolbarRoot.diffViewer) return "";
                    if (toolbarRoot.diffViewer.isNewFile) return "NEW";
                    if (toolbarRoot.diffViewer.isDeletedFile) return "DELETED";
                    if (toolbarRoot.diffViewer.isRenamed) return "RENAMED";
                    if (toolbarRoot.diffViewer.isBinary) return (toolbarRoot.diffViewer.formatBadge && toolbarRoot.diffViewer.formatBadge.length > 0) ? toolbarRoot.diffViewer.formatBadge : "BINARY";
                    return "MODIFIED";
                }
                color: toolbarRoot.theme.selectionText
                font.pixelSize: 10
                font.bold: true
            }
        }

        // Format Badge for new/deleted/renamed binary files
        Rectangle {
            visible: Boolean(toolbarRoot.diffViewer && !toolbarRoot.diffViewer.isEmpty && toolbarRoot.diffViewer.isBinary &&
                     (toolbarRoot.diffViewer.isNewFile || toolbarRoot.diffViewer.isDeletedFile || toolbarRoot.diffViewer.isRenamed) &&
                     toolbarRoot.diffViewer.formatBadge && toolbarRoot.diffViewer.formatBadge.length > 0)
            Layout.preferredHeight: 22
            implicitWidth: fmtBadgeTxt.implicitWidth + 12
            radius: toolbarRoot.theme.radiusXs
            color: toolbarRoot.theme.cardHover
            border.color: toolbarRoot.theme.borderHighlight
            border.width: 1
            Text {
                id: fmtBadgeTxt
                anchors.centerIn: parent
                text: toolbarRoot.diffViewer ? toolbarRoot.diffViewer.formatBadge : ""
                color: toolbarRoot.theme.borderFocus
                font.pixelSize: 10
                font.bold: true
            }
        }

        // Language Badge
        Rectangle {
            visible: Boolean(toolbarRoot.diffViewer && !toolbarRoot.diffViewer.isEmpty && toolbarRoot.diffViewer.languageName && toolbarRoot.diffViewer.languageName.length > 0)
            Layout.preferredHeight: 22
            implicitWidth: langRow.implicitWidth + 14
            radius: toolbarRoot.theme.radiusXs
            color: toolbarRoot.theme.surfaceElevated
            border.color: toolbarRoot.theme.borderHighlight
            border.width: 1

            RowLayout {
                id: langRow
                anchors.centerIn: parent
                spacing: 4
                Text { text: toolbarRoot.diffViewer ? toolbarRoot.diffViewer.languageIcon : "📄"; font.pixelSize: 10 }
                Text { text: toolbarRoot.diffViewer ? toolbarRoot.diffViewer.languageName : ""; color: toolbarRoot.theme.borderFocus; font.pixelSize: 10; font.bold: true }
            }
        }

        // Markdown Segmented Switch
        SegmentedPreviewToggle {
            visible: Boolean(toolbarRoot.isMarkdown && toolbarRoot.diffViewer && !toolbarRoot.diffViewer.isEmpty && !toolbarRoot.diffViewer.isBinary)
            codeLabel: "<> Diff"
            previewLabel: "👁 Anteprima"
            previewActive: toolbarRoot.showingMarkdownPreview
            onCodeClicked: if (toolbarRoot.diffViewer) toolbarRoot.diffViewer.setMarkdownRendered(false)
            onPreviewClicked: if (toolbarRoot.diffViewer) toolbarRoot.diffViewer.setMarkdownRendered(true)
        }

        // SVG Segmented Switch
        SegmentedPreviewToggle {
            visible: Boolean(toolbarRoot.isSvgPreviewable && toolbarRoot.diffViewer && !toolbarRoot.diffViewer.isEmpty && !toolbarRoot.diffViewer.isBinary)
            codeLabel: "<> Codice"
            previewLabel: "🖼 Anteprima"
            previewActive: toolbarRoot.showingRichPreview
            onCodeClicked: if (toolbarRoot.diffViewer) toolbarRoot.diffViewer.setRichPreviewRendered(false)
            onPreviewClicked: if (toolbarRoot.diffViewer) toolbarRoot.diffViewer.setRichPreviewRendered(true)
        }

        // Context Lines Selector
        Button {
            id: contextBtn
            visible: Boolean(toolbarRoot.diffViewer && !toolbarRoot.diffViewer.isEmpty && !toolbarRoot.diffViewer.isBinary &&
                !toolbarRoot.showingMarkdownPreview && !toolbarRoot.showingRichPreview && toolbarRoot.diffViewer.viewMode !== "blame")
            Layout.preferredHeight: 28
            leftPadding: 10
            rightPadding: 10
            hoverEnabled: true
            background: Rectangle {
                color: (toolbarRoot.diffViewer && toolbarRoot.diffViewer.isFullContext) ? toolbarRoot.theme.infoBg : (contextBtn.hovered ? toolbarRoot.theme.cardHover : toolbarRoot.theme.surface)
                radius: 7
                border.color: (toolbarRoot.diffViewer && toolbarRoot.diffViewer.isFullContext) ? toolbarRoot.theme.borderFocus : toolbarRoot.theme.borderHighlight
                border.width: 1
                Behavior on color { ColorAnimation { duration: 110 } }
            }
            contentItem: RowLayout {
                spacing: 4
                Text { text: "↕"; color: toolbarRoot.theme.borderFocus; font.pixelSize: 11; font.bold: true }
                Text {
                    text: {
                        if (!toolbarRoot.diffViewer) return "Contesto: 10";
                        if (toolbarRoot.diffViewer.isFullContext) return "File completo";
                        return "Contesto: " + toolbarRoot.diffViewer.contextLines;
                    }
                    color: (toolbarRoot.diffViewer && toolbarRoot.diffViewer.isFullContext) ? toolbarRoot.theme.infoText : toolbarRoot.theme.textPrimary
                    font.pixelSize: 11
                    font.bold: Boolean(toolbarRoot.diffViewer && toolbarRoot.diffViewer.isFullContext)
                }
                Text { text: "▾"; color: toolbarRoot.theme.textMuted; font.pixelSize: 9 }
            }
            onClicked: contextMenu.open()

            FluxMenu {
                id: contextMenu
                y: contextBtn.height + 4

                ContextOptionItem {
                    text: "3 righe (compatto)"
                    selected: Boolean(toolbarRoot.diffViewer && toolbarRoot.diffViewer.contextLines === 3)
                    onTriggered: if (toolbarRoot.diffViewer) toolbarRoot.diffViewer.setContextLines(3)
                }
                ContextOptionItem {
                    text: "10 righe (predefinito)"
                    selected: Boolean(toolbarRoot.diffViewer && toolbarRoot.diffViewer.contextLines === 10)
                    onTriggered: if (toolbarRoot.diffViewer) toolbarRoot.diffViewer.setContextLines(10)
                }
                ContextOptionItem {
                    text: "25 righe"
                    selected: Boolean(toolbarRoot.diffViewer && toolbarRoot.diffViewer.contextLines === 25)
                    onTriggered: if (toolbarRoot.diffViewer) toolbarRoot.diffViewer.setContextLines(25)
                }
                ContextOptionItem {
                    text: "50 righe"
                    selected: Boolean(toolbarRoot.diffViewer && toolbarRoot.diffViewer.contextLines === 50)
                    onTriggered: if (toolbarRoot.diffViewer) toolbarRoot.diffViewer.setContextLines(50)
                }
                FluxMenuSeparator {}
                ContextOptionItem {
                    text: "+15 righe extra"
                    enabled: Boolean(toolbarRoot.diffViewer && !toolbarRoot.diffViewer.isFullContext)
                    onTriggered: if (toolbarRoot.diffViewer) toolbarRoot.diffViewer.expandContext(15)
                }
                ContextOptionItem {
                    text: (toolbarRoot.diffViewer && toolbarRoot.diffViewer.isFullContext) ? "Torna al contesto ridotto" : "Mostra file completo"
                    selected: Boolean(toolbarRoot.diffViewer && toolbarRoot.diffViewer.isFullContext)
                    onTriggered: if (toolbarRoot.diffViewer) toolbarRoot.diffViewer.toggleFullContext()
                }
            }
        }

        // Word Wrap Toggle
        Button {
            id: wrapBtn
            visible: Boolean(toolbarRoot.diffViewer && !toolbarRoot.diffViewer.isEmpty && !toolbarRoot.diffViewer.isBinary &&
                !toolbarRoot.showingMarkdownPreview && !toolbarRoot.showingRichPreview && toolbarRoot.diffViewer.viewMode !== "blame")
            Layout.preferredHeight: 28
            leftPadding: 10
            rightPadding: 10
            hoverEnabled: true
            background: Rectangle {
                color: toolbarRoot.effectiveWrap ? toolbarRoot.theme.infoBg : (wrapBtn.hovered ? toolbarRoot.theme.cardHover : toolbarRoot.theme.surface)
                radius: 7
                border.color: toolbarRoot.effectiveWrap ? toolbarRoot.theme.borderFocus : toolbarRoot.theme.borderHighlight
                border.width: 1
                Behavior on color { ColorAnimation { duration: 110 } }
            }
            contentItem: Text {
                text: "↩ A capo"
                color: toolbarRoot.effectiveWrap ? toolbarRoot.theme.borderFocus : toolbarRoot.theme.textSecondary
                font.pixelSize: 11
                font.bold: toolbarRoot.effectiveWrap
            }
            onClicked: toolbarRoot.toggleWrapRequested()
        }

        // View Mode Switcher
        Rectangle {
            visible: Boolean(toolbarRoot.diffViewer && !toolbarRoot.diffViewer.isEmpty && !toolbarRoot.diffViewer.isBinary && !toolbarRoot.showingMarkdownPreview && !toolbarRoot.showingRichPreview)
            Layout.preferredHeight: 28
            implicitWidth: viewModeRow.implicitWidth + 6
            radius: toolbarRoot.theme.radiusSm
            color: toolbarRoot.theme.inputBg
            border.color: toolbarRoot.theme.border
            border.width: 1

            RowLayout {
                id: viewModeRow
                anchors.centerIn: parent
                spacing: 2

                ViewModeTabButton { modeId: "inline"; modeIcon: "📄"; modeLabel: "Inline" }
                ViewModeTabButton { modeId: "split"; modeIcon: "📑"; modeLabel: "Split" }
                ViewModeTabButton { modeId: "hunk"; modeIcon: "🔍"; modeLabel: "Hunk" }
                ViewModeTabButton { modeId: "blame"; modeIcon: "👤"; modeLabel: "Blame" }
            }
        }

        // Additions / Deletions stats
        RowLayout {
            visible: Boolean(toolbarRoot.diffViewer && !toolbarRoot.diffViewer.isEmpty && !toolbarRoot.diffViewer.isBinary)
            spacing: 6

            Text {
                text: toolbarRoot.diffViewer ? ("+" + toolbarRoot.diffViewer.additionsCount) : "+0"
                color: toolbarRoot.theme.successText
                font.bold: true
                font.pixelSize: 12
            }

            Text {
                text: toolbarRoot.diffViewer ? ("-" + toolbarRoot.diffViewer.deletionsCount) : "-0"
                color: toolbarRoot.theme.dangerText
                font.bold: true
                font.pixelSize: 12
            }
        }
    }
}
