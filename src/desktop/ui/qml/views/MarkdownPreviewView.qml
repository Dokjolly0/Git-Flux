import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../themes"

Rectangle {
    id: root
    color: theme.canvas

    readonly property FluxTheme theme: FluxTheme {}

    property var diffViewer: null
    property string monoFont: (typeof gfMonoFont !== "undefined" && gfMonoFont) ? gfMonoFont : "Consolas"
    property bool showToc: false
    property bool fullWidth: false
    property real zoomScale: 1.0
    property bool copiedWholeFile: false
    property int highlightedBlockIndex: -1
    property int lastChangeSearchIdx: -1

    function handleLink(url) {
        if (!url || url.length === 0) return;
        if (url.startsWith("#")) {
            const targetAnchor = url.substring(1);
            const toc = root.diffViewer ? root.diffViewer.markdownToc : [];
            for (let i = 0; i < toc.length; ++i) {
                if (toc[i].anchor === targetAnchor) {
                    jumpToBlock(toc[i].blockIndex);
                    return;
                }
            }
            return;
        }
        if (url.startsWith("http://") || url.startsWith("https://")) {
            Qt.openUrlExternally(url);
        }
    }

    function jumpToBlock(blockIdx) {
        if (blockIdx < 0 || blockIdx >= mdListView.count) return;
        root.highlightedBlockIndex = blockIdx;
        mdListView.positionViewAtIndex(blockIdx, ListView.Beginning);
        highlightResetTimer.restart();
    }

    function jumpToNextChange() {
        const blocks = root.diffViewer ? root.diffViewer.markdownBlocks : [];
        if (!blocks || blocks.length === 0) return;

        const startIdx = root.lastChangeSearchIdx + 1;
        for (let pass = 0; pass < 2; ++pass) {
            const from = (pass === 0) ? startIdx : 0;
            const to = (pass === 0) ? blocks.length : Math.min(startIdx, blocks.length);
            for (let i = from; i < to; ++i) {
                if (blocks[i].changeStatus && blocks[i].changeStatus !== "unchanged") {
                    root.lastChangeSearchIdx = i;
                    root.highlightedBlockIndex = i;
                    mdListView.positionViewAtIndex(i, ListView.Center);
                    highlightResetTimer.restart();
                    return;
                }
            }
        }
    }

    readonly property int changedBlocksCount: {
        const blocks = root.diffViewer ? root.diffViewer.markdownBlocks : [];
        if (!blocks) return 0;
        let count = 0;
        for (let i = 0; i < blocks.length; ++i) {
            if (blocks[i].changeStatus && blocks[i].changeStatus !== "unchanged") {
                ++count;
            }
        }
        return count;
    }

    Timer {
        id: highlightResetTimer
        interval: 1400
        onTriggered: root.highlightedBlockIndex = -1
    }

    Timer {
        id: copyWholeTimer
        interval: 1500
        onTriggered: root.copiedWholeFile = false
    }

    Connections {
        target: root.diffViewer

        function onMarkdownDocumentChanged() {
            root.lastChangeSearchIdx = -1;
            root.highlightedBlockIndex = -1;
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // ==========================================
        // MARKDOWN PREVIEW SUB-TOOLBAR
        // ==========================================
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 36
            color: theme.surface
            border.color: theme.border
            border.width: 1

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 12
                anchors.rightMargin: 12
                spacing: 8

                // TOC Drawer Toggle
                Button {
                    id: tocBtn
                    Layout.preferredHeight: 26
                    leftPadding: 9
                    rightPadding: 9
                    hoverEnabled: true
                    enabled: root.diffViewer && root.diffViewer.markdownToc && root.diffViewer.markdownToc.length > 0
                    opacity: enabled ? 1.0 : 0.45
                    background: Rectangle {
                        color: root.showToc ? theme.infoBg : (tocBtn.hovered ? theme.cardHover : theme.surface)
                        radius: 7
                        border.color: root.showToc ? theme.accentCyan : theme.border
                        border.width: 1
                        Behavior on color {
                            ColorAnimation {
                                duration: theme.animFast
                            }
                        }
                    }
                    contentItem: RowLayout {
                        spacing: 5
                        Text {
                            text: "📑"; font.pixelSize: 10
                        }
                        Text {
                            text: {
                                const tocCount = (root.diffViewer && root.diffViewer.markdownToc) ? root.diffViewer.markdownToc.length : 0;
                                return "Indice" + (tocCount > 0 ? (" (" + tocCount + ")") : "");
                            }
                            color: root.showToc ? theme.accentCyan : theme.textSecondary
                            font.pixelSize: 11
                            font.bold: root.showToc
                        }
                    }
                    onClicked: root.showToc = !root.showToc
                }

                // Rich Diff Toggle & Jump to Next Change
                Button {
                    id: richDiffBtn
                    Layout.preferredHeight: 26
                    leftPadding: 9
                    rightPadding: 9
                    hoverEnabled: true
                    readonly property bool isActive: root.diffViewer ? root.diffViewer.showMarkdownRichDiff : true
                    background: Rectangle {
                        color: richDiffBtn.isActive ? theme.successBg : (richDiffBtn.hovered ? theme.cardHover : theme.surface)
                        radius: 7
                        border.color: richDiffBtn.isActive ? theme.accentTeal : theme.border
                        border.width: 1
                        Behavior on color {
                            ColorAnimation {
                                duration: theme.animFast
                            }
                        }
                    }
                    contentItem: RowLayout {
                        spacing: 5
                        Rectangle {
                            Layout.preferredWidth: 7
                            Layout.preferredHeight: 7
                            radius: 3.5
                            color: richDiffBtn.isActive ? theme.accentTeal : theme.textMuted
                        }
                        Text {
                            text: "Rich Diff" + (root.changedBlocksCount > 0 ? (" (" + root.changedBlocksCount + ")") : "")
                            color: richDiffBtn.isActive ? theme.successText : theme.textMuted
                            font.pixelSize: 11
                            font.bold: richDiffBtn.isActive
                        }
                    }
                    onClicked: {
                        if (root.diffViewer) {
                            root.diffViewer.setShowMarkdownRichDiff(!root.diffViewer.showMarkdownRichDiff);
                        }
                    }
                }

                Button {
                    id: nextChangeBtn
                    visible: root.changedBlocksCount > 0
                    Layout.preferredHeight: 26
                    leftPadding: 9
                    rightPadding: 9
                    hoverEnabled: true
                    background: Rectangle {
                        color: nextChangeBtn.hovered ? theme.cardHover : theme.surface
                        radius: 7
                        border.color: theme.border
                        border.width: 1
                        Behavior on color {
                            ColorAnimation {
                                duration: theme.animFast
                            }
                        }
                    }
                    contentItem: Text {
                        text: "↓ Prossima modifica"
                        color: theme.accentCyan
                        font.pixelSize: 11
                        font.bold: true
                    }
                    onClicked: {
                        if (root.diffViewer && !root.diffViewer.showMarkdownRichDiff) {
                            root.diffViewer.setShowMarkdownRichDiff(true);
                        }
                        root.jumpToNextChange();
                    }
                }

                Item {
                    Layout.fillWidth: true
                }

                // Width Toggle (GitHub Centered vs Full Width)
                Button {
                    id: widthToggleBtn
                    Layout.preferredHeight: 26
                    leftPadding: 9
                    rightPadding: 9
                    hoverEnabled: true
                    background: Rectangle {
                        color: widthToggleBtn.hovered ? theme.cardHover : theme.surface
                        radius: 7
                        border.color: theme.border
                        border.width: 1
                        Behavior on color {
                            ColorAnimation {
                                duration: theme.animFast
                            }
                        }
                    }
                    contentItem: Text {
                        text: root.fullWidth ? "↔ Larghezza lettore" : "↔ Espandi pagina"
                        color: theme.textSecondary
                        font.pixelSize: 11
                    }
                    onClicked: root.fullWidth = !root.fullWidth
                }

                // Zoom Controls
                Rectangle {
                    Layout.preferredHeight: 26
                    implicitWidth: zoomRow.implicitWidth + 8
                    color: theme.inputBg
                    radius: 7
                    border.color: theme.border
                    border.width: 1

                    RowLayout {
                        id: zoomRow
                        anchors.centerIn: parent
                        spacing: 2

                        Button {
                            id: zoomOutBtn
                            Layout.preferredWidth: 22
                            Layout.preferredHeight: 20
                            background: Rectangle {
                                color: zoomOutBtn.hovered ? theme.cardHover : "transparent"
                                radius: 3
                            }
                            contentItem: Text {
                                text: "A-"
                                color: theme.textSecondary
                                font.pixelSize: 10
                                font.bold: true
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                            }
                            onClicked: root.zoomScale = Math.max(0.8, Math.round((root.zoomScale - 0.1) * 10) / 10)
                        }

                        Text {
                            text: Math.round(root.zoomScale * 100) + "%"
                            color: theme.textMuted
                            font.pixelSize: 10
                            Layout.preferredWidth: 34
                            horizontalAlignment: Text.AlignHCenter
                        }

                        Button {
                            id: zoomInBtn
                            Layout.preferredWidth: 22
                            Layout.preferredHeight: 20
                            background: Rectangle {
                                color: zoomInBtn.hovered ? theme.cardHover : "transparent"
                                radius: 3
                            }
                            contentItem: Text {
                                text: "A+"
                                color: theme.textSecondary
                                font.pixelSize: 10
                                font.bold: true
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                            }
                            onClicked: root.zoomScale = Math.min(1.5, Math.round((root.zoomScale + 0.1) * 10) / 10)
                        }
                    }
                }

                // Copy Full Markdown Source
                Button {
                    id: copyMdBtn
                    Layout.preferredHeight: 24
                    enabled: root.diffViewer && root.diffViewer.markdownSource && root.diffViewer.markdownSource.length > 0
                    background: Rectangle {
                        color: root.copiedWholeFile ? theme.successBg : (copyMdBtn.hovered ? theme.cardHover : theme.surfaceElevated)
                        radius: 4
                        border.color: root.copiedWholeFile ? theme.accentTeal : theme.borderHighlight
                        border.width: 1
                    }
                    contentItem: Text {
                        text: root.copiedWholeFile ? "✓ Copiato!" : "📋 Copia .md"
                        color: root.copiedWholeFile ? theme.successText : theme.textSecondary
                        font.pixelSize: 11
                        font.bold: root.copiedWholeFile
                    }
                    onClicked: {
                        if (root.diffViewer && root.diffViewer.markdownSource) {
                            root.diffViewer.copyToClipboard(root.diffViewer.markdownSource);
                            root.copiedWholeFile = true;
                            copyWholeTimer.restart();
                        }
                    }
                }
            }
        }

        // ==========================================
        // CONTEXTUAL NOTICE BANNER (Deleted / Partial)
        // ==========================================
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 32
            visible: root.diffViewer && root.diffViewer.markdownNotice && root.diffViewer.markdownNotice.length > 0
            color: theme.warningBg
            border.color: theme.warning
            border.width: 1

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 14
                anchors.rightMargin: 14
                spacing: 8

                Text {
                    text: "ℹ"; color: theme.warning; font.bold: true; font.pixelSize: 12
                }
                Text {
                    text: root.diffViewer ? root.diffViewer.markdownNotice : ""
                    color: theme.warningText
                    font.pixelSize: 11
                    elide: Text.ElideRight
                    Layout.fillWidth: true
                }
            }
        }

        // ==========================================
        // MAIN BODY: OPTIONAL TOC SIDEBAR + DOCUMENT
        // ==========================================
        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 0

            // TOC Sidebar
            Rectangle {
                visible: root.showToc
                Layout.preferredWidth: 235
                Layout.fillHeight: true
                color: theme.canvas
                border.color: theme.border
                border.width: 1

                ColumnLayout {
                    anchors.fill: parent
                    spacing: 0

                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 32
                        color: theme.surface
                        border.color: theme.border
                        border.width: 1

                        Text {
                            anchors.left: parent.left
                            anchors.leftMargin: 12
                            anchors.verticalCenter: parent.verticalCenter
                            text: "SOMMARIO DOCUMENTO"
                            color: theme.textMuted
                            font.pixelSize: 10
                            font.bold: true
                        }
                    }

                    ListView {
                        SmoothWheelScroller {}
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        clip: true
                        model: root.diffViewer ? root.diffViewer.markdownToc : null
                        boundsBehavior: Flickable.StopAtBounds
                        ScrollBar.vertical: FluxScrollBar {
                        }

                        delegate: Rectangle {
                            width: ListView.view.width
                            height: 28
                            color: tocItemMouse.containsMouse ? theme.surfaceElevated : "transparent"

                            Text {
                                anchors.left: parent.left
                                anchors.leftMargin: 10 + Math.max(0, (modelData.level - 1)) * 12
                                anchors.right: parent.right
                                anchors.rightMargin: 10
                                anchors.verticalCenter: parent.verticalCenter
                                text: modelData.title || ""
                                color: modelData.level <= 2 ? theme.textPrimary : theme.textMuted
                                font.pixelSize: modelData.level === 1 ? 12 : 11
                                font.bold: modelData.level <= 2
                                elide: Text.ElideRight
                            }

                            MouseArea {
                                id: tocItemMouse
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: root.jumpToBlock(modelData.blockIndex)
                            }
                        }
                    }
                }
            }

            // Rendered Markdown Document Area
            Item {
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true

                Text {
                    anchors.centerIn: parent
                    visible: !root.diffViewer || !root.diffViewer.markdownBlocks || root.diffViewer.markdownBlocks.length === 0
                    text: "Nessun contenuto Markdown da visualizzare"
                    color: theme.textMuted
                    font.pixelSize: 13
                }

                ListView {
                    id: mdListView
                    SmoothWheelScroller {}
                    anchors.fill: parent
                    topMargin: 20
                    bottomMargin: 32
                    spacing: 10
                    clip: true
                    model: root.diffViewer ? root.diffViewer.markdownBlocks : null
                    boundsBehavior: Flickable.StopAtBounds
                    maximumFlickVelocity: 6000
                    flickDeceleration: 1000
                    ScrollBar.vertical: FluxScrollBar {
                    }

                    delegate: Item {
                        id: blockDelegate
                        width: mdListView.width
                        height: blockContainer.implicitHeight

                        readonly property bool showRichDiff: root.diffViewer ? root.diffViewer.showMarkdownRichDiff : true
                        readonly property string changeStatus: modelData.changeStatus || "unchanged"
                        readonly property bool hasChange: showRichDiff && changeStatus !== "unchanged"
                        readonly property color changeColor: {
                            if (changeStatus === "added") return theme.success;
                            if (changeStatus === "modified") return theme.warning;
                            if (changeStatus === "deleted") return theme.danger;
                            return "transparent";
                        }

                        Item {
                            id: blockContainer
                            anchors.horizontalCenter: parent.horizontalCenter
                            width: root.fullWidth ? Math.max(240, mdListView.width - 48) : Math.max(240, Math.min(880, mdListView.width - 48))
                            implicitHeight: Math.max(innerBlockItem.implicitHeight, diffMetaRow.visible ? 22 : 0)

                            // Flash highlight rectangle for TOC / Next-Change jumps
                            Rectangle {
                                anchors.fill: parent
                                anchors.margins: -4
                                radius: 6
                                color: theme.infoBg
                                border.color: theme.accentCyan
                                border.width: 1
                                visible: root.highlightedBlockIndex === index
                                opacity: 0.45
                            }

                            // Rich Diff left gutter bar
                            Rectangle {
                                visible: blockDelegate.hasChange
                                anchors.left: parent.left
                                anchors.leftMargin: -12
                                anchors.top: parent.top
                                anchors.bottom: parent.bottom
                                width: 4
                                radius: 2
                                color: blockDelegate.changeColor
                            }

                            // Rich Diff pill (+N / -M) at top-right of changed block
                            Row {
                                id: diffMetaRow
                                visible: blockDelegate.hasChange && modelData.kind !== "thematicBreak" && modelData.kind !== "deletedSnippet"
                                anchors.top: parent.top
                                anchors.right: parent.right
                                spacing: 4
                                z: 5

                                Rectangle {
                                    visible: (modelData.addedLineCount || 0) > 0
                                    height: 16
                                    width: addCountText.implicitWidth + 8
                                    radius: 8
                                    color: theme.successBg
                                    border.color: theme.success
                                    border.width: 1
                                    Text {
                                        id: addCountText
                                        anchors.centerIn: parent
                                        text: "+" + modelData.addedLineCount
                                        color: theme.successText
                                        font.pixelSize: 9
                                        font.bold: true
                                    }
                                }

                                Rectangle {
                                    visible: (modelData.deletedLineCount || 0) > 0
                                    height: 16
                                    width: delCountText.implicitWidth + 8
                                    radius: 8
                                    color: theme.dangerBg
                                    border.color: theme.danger
                                    border.width: 1
                                    Text {
                                        id: delCountText
                                        anchors.centerIn: parent
                                        text: "-" + modelData.deletedLineCount
                                        color: theme.dangerText
                                        font.pixelSize: 9
                                        font.bold: true
                                    }
                                }
                            }

                            ColumnLayout {
                                id: innerBlockItem
                                width: parent.width
                                spacing: 6

                                // 1. HEADING
                                ColumnLayout {
                                    visible: modelData.kind === "heading"
                                    Layout.fillWidth: true
                                    Layout.topMargin: modelData.level === 1 ? 8 : (modelData.level === 2 ? 6 : 2)
                                    spacing: 6

                                    Text {
                                        Layout.fillWidth: true
                                        text: modelData.html || ""
                                        textFormat: Text.RichText
                                        wrapMode: Text.Wrap
                                        color: theme.textPrimary
                                        font.bold: true
                                        font.pixelSize: {
                                            const lvl = modelData.level || 1;
                                            let base = 13;
                                            if (lvl === 1) base = 26;
                                            else if (lvl === 2) base = 20;
                                            else if (lvl === 3) base = 16;
                                            else if (lvl === 4) base = 14;
                                            return Math.round(base * root.zoomScale);
                                        }
                                        horizontalAlignment: modelData.centered ? Text.AlignHCenter : Text.AlignLeft
                                        onLinkActivated: (link) => root.handleLink(link)
                                    }

                                    Rectangle {
                                        visible: (modelData.level || 1) <= 2
                                        Layout.fillWidth: true
                                        Layout.preferredHeight: 1
                                        color: theme.border
                                    }
                                }

                                // 2. PARAGRAPH
                                Text {
                                    visible: modelData.kind === "paragraph"
                                    Layout.fillWidth: true
                                    text: modelData.html || ""
                                    textFormat: Text.RichText
                                    wrapMode: Text.Wrap
                                    color: theme.textPrimary
                                    font.pixelSize: Math.round(14 * root.zoomScale)
                                    lineHeight: 1.45
                                    horizontalAlignment: modelData.centered ? Text.AlignHCenter : Text.AlignLeft
                                    onLinkActivated: (link) => root.handleLink(link)
                                }

                                // 3. BADGE ROW
                                Flow {
                                    visible: modelData.kind === "badgeRow"
                                    Layout.fillWidth: true
                                    spacing: 6
                                    layoutDirection: Qt.LeftToRight

                                    Repeater {
                                        model: modelData.badges || []

                                        delegate: Rectangle {
                                            height: 22
                                            width: badgeLabelRow.implicitWidth + 14
                                            radius: 4
                                            color: modelData.color || theme.accentBlue
                                            border.color: theme.textPrimary
                                            border.width: badgeMouse.containsMouse ? 1 : 0

                                            Row {
                                                id: badgeLabelRow
                                                anchors.centerIn: parent
                                                spacing: 4
                                                Text {
                                                    text: modelData.label || ""
                                                    color: theme.textPrimary
                                                    font.pixelSize: Math.round(11 * root.zoomScale)
                                                    font.bold: true
                                                }
                                            }

                                            MouseArea {
                                                id: badgeMouse
                                                anchors.fill: parent
                                                hoverEnabled: true
                                                cursorShape: (modelData.href && modelData.href.length > 0) ? Qt.PointingHandCursor : Qt.ArrowCursor
                                                onClicked: {
                                                    if (modelData.href && modelData.href.length > 0) {
                                                        root.handleLink(modelData.href);
                                                    }
                                                }
                                            }
                                        }
                                    }
                                }

                                // 4. CODE BLOCK & TREE / MERMAID CARD
                                Rectangle {
                                    id: codeCard
                                    visible: modelData.kind === "code"
                                    Layout.fillWidth: true
                                    implicitHeight: codeCardCol.implicitHeight
                                    color: theme.surfaceElevated
                                    radius: 6
                                    border.color: modelData.isDiagram ? theme.info : theme.borderHighlight
                                    border.width: 1

                                    property bool copiedBlock: false

                                    Timer {
                                        id: copyBlockTimer
                                        interval: 1500
                                        onTriggered: codeCard.copiedBlock = false
                                    }

                                    ColumnLayout {
                                        id: codeCardCol
                                        width: parent.width
                                        spacing: 0

                                        // Code Block Header
                                        Rectangle {
                                            Layout.fillWidth: true
                                            Layout.preferredHeight: 30
                                            color: theme.headerBg
                                            radius: 6

                                            Rectangle {
                                                anchors.left: parent.left
                                                anchors.right: parent.right
                                                anchors.bottom: parent.bottom
                                                height: 6
                                                color: theme.headerBg
                                            }

                                            Rectangle {
                                                anchors.left: parent.left
                                                anchors.right: parent.right
                                                anchors.bottom: parent.bottom
                                                height: 1
                                                color: theme.borderHighlight
                                            }

                                            RowLayout {
                                                anchors.fill: parent
                                                anchors.leftMargin: 12
                                                anchors.rightMargin: 8
                                                spacing: 6

                                                Text {
                                                    text: modelData.isDiagram ? "🔀" : "📄"
                                                    font.pixelSize: 10
                                                }

                                                Text {
                                                    text: (modelData.languageTag && modelData.languageTag.length > 0)
                                                        ? modelData.languageTag.toUpperCase()
                                                        : "TEXT"
                                                    color: modelData.isDiagram ? theme.infoText : theme.textMuted
                                                    font.family: root.monoFont
                                                    font.pixelSize: 10
                                                    font.bold: true
                                                }

                                                Text {
                                                    visible: modelData.isDiagram === true
                                                    text: "Diagramma sorgente"
                                                    color: theme.textPlaceholder
                                                    font.pixelSize: 10
                                                }

                                                Item {
                                                    Layout.fillWidth: true
                                                }

                                                Button {
                                                    id: copyCodeBtn
                                                    Layout.preferredHeight: 20
                                                    background: Rectangle {
                                                        color: codeCard.copiedBlock ? theme.successBg : (copyCodeBtn.hovered ? theme.borderHighlight : theme.border)
                                                        radius: 4
                                                        border.color: codeCard.copiedBlock ? theme.accentTeal : theme.borderHighlight
                                                        border.width: 1
                                                    }
                                                    contentItem: Text {
                                                        text: codeCard.copiedBlock ? "✓ Copiato" : "Copia"
                                                        color: codeCard.copiedBlock ? theme.successText : theme.textSecondary
                                                        font.pixelSize: 10
                                                        font.bold: true
                                                    }
                                                    onClicked: {
                                                        if (root.diffViewer) {
                                                            root.diffViewer.copyToClipboard(modelData.rawText || "");
                                                            codeCard.copiedBlock = true;
                                                            copyBlockTimer.restart();
                                                        }
                                                    }
                                                }
                                            }
                                        }

                                        // Horizontal Scrollable Code Area (preserves ASCII/UTF-8 box trees!)
                                        Flickable {
                                            SmoothWheelScroller {}
                                            Layout.fillWidth: true
                                            Layout.preferredHeight: codePreText.implicitHeight + 22
                                            contentWidth: Math.max(width, codePreText.implicitWidth + 24)
                                            contentHeight: codePreText.implicitHeight + 22
                                            clip: true
                                            flickableDirection: Flickable.HorizontalFlick
                                            interactive: contentWidth > width
                                            boundsBehavior: Flickable.StopAtBounds
                                            ScrollBar.horizontal: FluxScrollBar {
                                            }

                                            Text {
                                                id: codePreText
                                                x: 12
                                                y: 10
                                                text: modelData.html || ""
                                                textFormat: Text.RichText
                                                color: theme.textPrimary
                                                font.family: root.monoFont
                                                font.pixelSize: Math.round(12.5 * root.zoomScale)
                                                wrapMode: Text.NoWrap
                                            }
                                        }
                                    }
                                }

                                // 5. BLOCKQUOTE & GITHUB ALERTS
                                Rectangle {
                                    id: quoteRect
                                    visible: modelData.kind === "blockquote"
                                    Layout.fillWidth: true
                                    implicitHeight: quoteCol.implicitHeight + 14
                                    radius: 4
                                    readonly property string alertKind: modelData.alertType || "none"
                                    readonly property color accentColor: {
                                        if (alertKind === "note") return theme.info;
                                        if (alertKind === "tip") return theme.success;
                                        if (alertKind === "important") return theme.accentPurple;
                                        if (alertKind === "warning") return theme.warning;
                                        if (alertKind === "caution") return theme.danger;
                                        return theme.borderHighlight;
                                    }
                                    color: alertKind === "none" ? theme.surface : theme.surfaceElevated
                                    border.color: alertKind === "none" ? theme.border : accentColor
                                    border.width: alertKind === "none" ? 0 : 1

                                    Rectangle {
                                        anchors.left: parent.left
                                        anchors.top: parent.top
                                        anchors.bottom: parent.bottom
                                        width: 4
                                        color: parent.accentColor
                                        radius: 2
                                    }

                                    ColumnLayout {
                                        id: quoteCol
                                        anchors.left: parent.left
                                        anchors.leftMargin: 14
                                        anchors.right: parent.right
                                        anchors.rightMargin: 12
                                        anchors.top: parent.top
                                        anchors.topMargin: 7
                                        spacing: 4

                                        Text {
                                            visible: (modelData.alertType || "none") !== "none"
                                            text: {
                                                const a = modelData.alertType;
                                                if (a === "note") return "ℹ Nota";
                                                if (a === "tip") return "💡 Suggerimento";
                                                if (a === "important") return "❗ Importante";
                                                if (a === "warning") return "⚠ Avviso";
                                                if (a === "caution") return "🔥 Attenzione";
                                                return "";
                                            }
                                            color: quoteRect.accentColor
                                            font.bold: true
                                            font.pixelSize: Math.round(12 * root.zoomScale)
                                        }

                                        Text {
                                            Layout.fillWidth: true
                                            text: modelData.html || ""
                                            textFormat: Text.RichText
                                            wrapMode: Text.Wrap
                                            color: (modelData.alertType || "none") === "none" ? theme.textMuted : theme.textPrimary
                                            font.pixelSize: Math.round(13.5 * root.zoomScale)
                                            lineHeight: 1.4
                                            onLinkActivated: (link) => root.handleLink(link)
                                        }
                                    }
                                }

                                // 6. LIST ITEM & TASK CHECKBOX
                                RowLayout {
                                    visible: modelData.kind === "listItem"
                                    Layout.fillWidth: true
                                    Layout.leftMargin: Math.min(modelData.indentLevel || 0, 6) * 20
                                    spacing: 8

                                    Item {
                                        Layout.preferredWidth: modelData.isTask ? 18 : 22
                                        Layout.preferredHeight: Math.round(20 * root.zoomScale)
                                        Layout.alignment: Qt.AlignTop

                                        // Checkbox for Task List
                                        Rectangle {
                                            visible: modelData.isTask === true
                                            anchors.centerIn: parent
                                            width: 14
                                            height: 14
                                            radius: 3
                                            color: modelData.taskChecked ? theme.success : theme.surfaceElevated
                                            border.color: modelData.taskChecked ? theme.success : theme.textPlaceholder
                                            border.width: 1

                                            Text {
                                                visible: modelData.taskChecked === true
                                                anchors.centerIn: parent
                                                text: "✓"
                                                color: theme.textPrimary
                                                font.pixelSize: 10
                                                font.bold: true
                                            }
                                        }

                                        // Bullet / Number Marker
                                        Text {
                                            visible: modelData.isTask !== true
                                            anchors.right: parent.right
                                            anchors.rightMargin: 2
                                            anchors.verticalCenter: parent.verticalCenter
                                            text: modelData.listMarker || "•"
                                            color: modelData.ordered ? theme.textMuted : theme.info
                                            font.pixelSize: Math.round((modelData.ordered ? 12.5 : 15) * root.zoomScale)
                                            font.bold: true
                                        }
                                    }

                                    Text {
                                        Layout.fillWidth: true
                                        text: modelData.html || ""
                                        textFormat: Text.RichText
                                        wrapMode: Text.Wrap
                                        color: theme.textPrimary
                                        font.pixelSize: Math.round(13.5 * root.zoomScale)
                                        lineHeight: 1.4
                                        onLinkActivated: (link) => root.handleLink(link)
                                    }
                                }

                                // 7. GFM TABLE
                                Rectangle {
                                    id: tableCard
                                    visible: modelData.kind === "table"
                                    Layout.fillWidth: true
                                    implicitHeight: tableFlick.height
                                    color: theme.canvas
                                    radius: 6
                                    border.color: theme.borderHighlight
                                    border.width: 1
                                    clip: true

                                    readonly property var headers: modelData.tableHeaders || []
                                    readonly property var aligns: modelData.tableAlignments || []
                                    readonly property var rows: modelData.tableRows || []
                                    readonly property int colCount: Math.max(1, headers.length)

                                    readonly property var flatCells: {
                                        const out = [];
                                        for (let h = 0; h < headers.length; ++h) {
                                            out.push({
                                                text: headers[h],
                                                align: (h < aligns.length) ? aligns[h] : "left",
                                                isHeader: true,
                                                rowIdx: -1
                                            });
                                        }
                                        for (let r = 0; r < rows.length; ++r) {
                                            const row = rows[r];
                                            for (let c = 0; c < headers.length; ++c) {
                                                out.push({
                                                    text: (c < row.length) ? row[c] : "",
                                                    align: (c < aligns.length) ? aligns[c] : "left",
                                                    isHeader: false,
                                                    rowIdx: r
                                                });
                                            }
                                        }
                                        return out;
                                    }

                                    Flickable {
                                        id: tableFlick
                                        SmoothWheelScroller {}
                                        width: parent.width
                                        height: tableGrid.implicitHeight
                                        readonly property real baseColWidth: Math.max(130, Math.floor((width - 2) / tableCard.colCount))
                                        contentWidth: Math.max(width, tableGrid.implicitWidth)
                                        contentHeight: tableGrid.implicitHeight
                                        clip: true
                                        flickableDirection: Flickable.HorizontalFlick
                                        interactive: contentWidth > width
                                        boundsBehavior: Flickable.StopAtBounds
                                        ScrollBar.horizontal: FluxScrollBar {
                                        }

                                        GridLayout {
                                            id: tableGrid
                                            columns: tableCard.colCount
                                            columnSpacing: 0
                                            rowSpacing: 0

                                            Repeater {
                                                model: tableCard.flatCells

                                                delegate: Rectangle {
                                                    Layout.preferredWidth: tableFlick.baseColWidth
                                                    Layout.fillWidth: true
                                                    Layout.fillHeight: true
                                                    Layout.preferredHeight: Math.max(32, cellText.implicitHeight + 14)
                                                    color: modelData.isHeader
                                                        ? theme.surfaceElevated
                                                        : (modelData.rowIdx % 2 === 0 ? theme.canvas : theme.surface)
                                                    border.color: theme.borderHighlight
                                                    border.width: 0.5

                                                    Text {
                                                        id: cellText
                                                        anchors.centerIn: parent
                                                        width: Math.max(40, parent.width - 20)
                                                        text: modelData.text || ""
                                                        textFormat: Text.RichText
                                                        wrapMode: Text.Wrap
                                                        color: theme.textPrimary
                                                        font.bold: modelData.isHeader === true
                                                        font.pixelSize: Math.round(12.5 * root.zoomScale)
                                                        horizontalAlignment: {
                                                            if (modelData.align === "center") return Text.AlignHCenter;
                                                            if (modelData.align === "right") return Text.AlignRight;
                                                            return Text.AlignLeft;
                                                        }
                                                        onLinkActivated: (link) => root.handleLink(link)
                                                    }
                                                }
                                            }
                                        }
                                    }
                                }

                                // 8. THEMATIC BREAK (<hr/>)
                                Rectangle {
                                    visible: modelData.kind === "thematicBreak"
                                    Layout.fillWidth: true
                                    Layout.topMargin: 6
                                    Layout.bottomMargin: 6
                                    Layout.preferredHeight: 2
                                    color: theme.border
                                }

                                // 9. DELETED SNIPPET OR INLINE REMOVED LINES (Rich Diff)
                                Rectangle {
                                    visible: blockDelegate.showRichDiff && (
                                        modelData.kind === "deletedSnippet" ||
                                        (modelData.changeStatus === "modified" && modelData.deletedLinesText && modelData.deletedLinesText.length > 0)
                                    )
                                    Layout.fillWidth: true
                                    implicitHeight: deletedBoxCol.implicitHeight + 16
                                    color: theme.dangerBg
                                    radius: 5
                                    border.color: theme.danger
                                    border.width: 1

                                    ColumnLayout {
                                        id: deletedBoxCol
                                        anchors.left: parent.left
                                        anchors.right: parent.right
                                        anchors.top: parent.top
                                        anchors.margins: 8
                                        spacing: 4

                                        Text {
                                            text: "− Rimosso (" + (modelData.deletedLineCount || 1) + (modelData.deletedLineCount === 1 ? " riga)" : " righe)")
                                            color: theme.danger
                                            font.pixelSize: 10
                                            font.bold: true
                                        }

                                        Text {
                                            Layout.fillWidth: true
                                            text: modelData.deletedLinesText || ""
                                            color: theme.dangerText
                                            font.family: root.monoFont
                                            font.pixelSize: Math.round(11.5 * root.zoomScale)
                                            wrapMode: Text.WrapAtWordBoundaryOrAnywhere
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}
