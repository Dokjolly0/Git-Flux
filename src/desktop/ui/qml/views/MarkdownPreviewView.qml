import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root
    color: "#0D1117"

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
            var targetAnchor = url.substring(1);
            var toc = root.diffViewer ? root.diffViewer.markdownToc : [];
            for (var i = 0; i < toc.length; ++i) {
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
        var blocks = root.diffViewer ? root.diffViewer.markdownBlocks : [];
        if (!blocks || blocks.length === 0) return;

        var startIdx = root.lastChangeSearchIdx + 1;
        for (var pass = 0; pass < 2; ++pass) {
            var from = (pass === 0) ? startIdx : 0;
            var to = (pass === 0) ? blocks.length : Math.min(startIdx, blocks.length);
            for (var i = from; i < to; ++i) {
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
        var blocks = root.diffViewer ? root.diffViewer.markdownBlocks : [];
        if (!blocks) return 0;
        var count = 0;
        for (var i = 0; i < blocks.length; ++i) {
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
            color: "#11161E"
            border.color: "#21262D"
            border.width: 1

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 12
                anchors.rightMargin: 12
                spacing: 8

                // TOC Drawer Toggle
                Button {
                    id: tocBtn
                    Layout.preferredHeight: 24
                    enabled: root.diffViewer && root.diffViewer.markdownToc && root.diffViewer.markdownToc.length > 0
                    opacity: enabled ? 1.0 : 0.45
                    background: Rectangle {
                        color: root.showToc ? "#1E293B" : (tocBtn.hovered ? "#161B22" : "transparent")
                        radius: 4
                        border.color: root.showToc ? "#38BDF8" : "#30363D"
                        border.width: 1
                    }
                    contentItem: RowLayout {
                        spacing: 5
                        Text {
                            text: "📑"; font.pixelSize: 10
                        }
                        Text {
                            text: {
                                var tocCount = (root.diffViewer && root.diffViewer.markdownToc) ? root.diffViewer.markdownToc.length : 0;
                                return "Indice" + (tocCount > 0 ? (" (" + tocCount + ")") : "");
                            }
                            color: root.showToc ? "#38BDF8" : "#CBD5E1"
                            font.pixelSize: 11
                            font.bold: root.showToc
                        }
                    }
                    onClicked: root.showToc = !root.showToc
                }

                // Rich Diff Toggle & Jump to Next Change
                Button {
                    id: richDiffBtn
                    Layout.preferredHeight: 24
                    readonly property bool isActive: root.diffViewer ? root.diffViewer.showMarkdownRichDiff : true
                    background: Rectangle {
                        color: richDiffBtn.isActive ? "#172536" : (richDiffBtn.hovered ? "#161B22" : "transparent")
                        radius: 4
                        border.color: richDiffBtn.isActive ? "#3FB950" : "#30363D"
                        border.width: 1
                    }
                    contentItem: RowLayout {
                        spacing: 5
                        Rectangle {
                            Layout.preferredWidth: 7
                            Layout.preferredHeight: 7
                            radius: 3.5
                            color: richDiffBtn.isActive ? "#3FB950" : "#64748B"
                        }
                        Text {
                            text: "Rich Diff" + (root.changedBlocksCount > 0 ? (" (" + root.changedBlocksCount + ")") : "")
                            color: richDiffBtn.isActive ? "#7EE787" : "#94A3B8"
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
                    Layout.preferredHeight: 24
                    background: Rectangle {
                        color: nextChangeBtn.hovered ? "#1E293B" : "transparent"
                        radius: 4
                        border.color: "#30363D"
                        border.width: 1
                    }
                    contentItem: Text {
                        text: "↓ Prossima modifica"
                        color: "#38BDF8"
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
                    Layout.preferredHeight: 24
                    background: Rectangle {
                        color: widthToggleBtn.hovered ? "#1E293B" : "transparent"
                        radius: 4
                        border.color: "#30363D"
                        border.width: 1
                    }
                    contentItem: Text {
                        text: root.fullWidth ? "↔ Larghezza lettore" : "↔ Espandi pagina"
                        color: "#CBD5E1"
                        font.pixelSize: 11
                    }
                    onClicked: root.fullWidth = !root.fullWidth
                }

                // Zoom Controls
                Rectangle {
                    Layout.preferredHeight: 24
                    implicitWidth: zoomRow.implicitWidth + 8
                    color: "#0D1117"
                    radius: 4
                    border.color: "#30363D"
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
                                color: zoomOutBtn.hovered ? "#1E293B" : "transparent"
                                radius: 3
                            }
                            contentItem: Text {
                                text: "A-"
                                color: "#CBD5E1"
                                font.pixelSize: 10
                                font.bold: true
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                            }
                            onClicked: root.zoomScale = Math.max(0.8, Math.round((root.zoomScale - 0.1) * 10) / 10)
                        }

                        Text {
                            text: Math.round(root.zoomScale * 100) + "%"
                            color: "#8B949E"
                            font.pixelSize: 10
                            Layout.preferredWidth: 34
                            horizontalAlignment: Text.AlignHCenter
                        }

                        Button {
                            id: zoomInBtn
                            Layout.preferredWidth: 22
                            Layout.preferredHeight: 20
                            background: Rectangle {
                                color: zoomInBtn.hovered ? "#1E293B" : "transparent"
                                radius: 3
                            }
                            contentItem: Text {
                                text: "A+"
                                color: "#CBD5E1"
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
                        color: root.copiedWholeFile ? "#064E3B" : (copyMdBtn.hovered ? "#1E293B" : "#161B22")
                        radius: 4
                        border.color: root.copiedWholeFile ? "#10B981" : "#30363D"
                        border.width: 1
                    }
                    contentItem: Text {
                        text: root.copiedWholeFile ? "✓ Copiato!" : "📋 Copia .md"
                        color: root.copiedWholeFile ? "#6EE7B7" : "#E2E8F0"
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
            color: "#1C2128"
            border.color: "#D29922"
            border.width: 1

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 14
                anchors.rightMargin: 14
                spacing: 8

                Text {
                    text: "ℹ"; color: "#D29922"; font.bold: true; font.pixelSize: 12
                }
                Text {
                    text: root.diffViewer ? root.diffViewer.markdownNotice : ""
                    color: "#E3B341"
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
                color: "#0B0F17"
                border.color: "#21262D"
                border.width: 1

                ColumnLayout {
                    anchors.fill: parent
                    spacing: 0

                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 32
                        color: "#11161E"
                        border.color: "#21262D"
                        border.width: 1

                        Text {
                            anchors.left: parent.left
                            anchors.leftMargin: 12
                            anchors.verticalCenter: parent.verticalCenter
                            text: "SOMMARIO DOCUMENTO"
                            color: "#8B949E"
                            font.pixelSize: 10
                            font.bold: true
                        }
                    }

                    ListView {
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
                            color: tocItemMouse.containsMouse ? "#161B22" : "transparent"

                            Text {
                                anchors.left: parent.left
                                anchors.leftMargin: 10 + Math.max(0, (modelData.level - 1)) * 12
                                anchors.right: parent.right
                                anchors.rightMargin: 10
                                anchors.verticalCenter: parent.verticalCenter
                                text: modelData.title || ""
                                color: modelData.level <= 2 ? "#F0F6FC" : "#8B949E"
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
                    color: "#64748B"
                    font.pixelSize: 13
                }

                ListView {
                    id: mdListView
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
                            if (changeStatus === "added") return "#2EA043";
                            if (changeStatus === "modified") return "#D29922";
                            if (changeStatus === "deleted") return "#F85149";
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
                                color: "#1C2D42"
                                border.color: "#38BDF8"
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
                                    color: "#122D1B"
                                    border.color: "#2EA043"
                                    border.width: 1
                                    Text {
                                        id: addCountText
                                        anchors.centerIn: parent
                                        text: "+" + modelData.addedLineCount
                                        color: "#7EE787"
                                        font.pixelSize: 9
                                        font.bold: true
                                    }
                                }

                                Rectangle {
                                    visible: (modelData.deletedLineCount || 0) > 0
                                    height: 16
                                    width: delCountText.implicitWidth + 8
                                    radius: 8
                                    color: "#2D1215"
                                    border.color: "#F85149"
                                    border.width: 1
                                    Text {
                                        id: delCountText
                                        anchors.centerIn: parent
                                        text: "-" + modelData.deletedLineCount
                                        color: "#FFA198"
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
                                        color: "#F0F6FC"
                                        font.bold: true
                                        font.pixelSize: {
                                            var lvl = modelData.level || 1;
                                            var base = 13;
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
                                        color: "#21262D"
                                    }
                                }

                                // 2. PARAGRAPH
                                Text {
                                    visible: modelData.kind === "paragraph"
                                    Layout.fillWidth: true
                                    text: modelData.html || ""
                                    textFormat: Text.RichText
                                    wrapMode: Text.Wrap
                                    color: "#E6EDF3"
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
                                            color: modelData.color || "#1F6FEB"
                                            border.color: "#FFFFFF"
                                            border.width: badgeMouse.containsMouse ? 1 : 0

                                            Row {
                                                id: badgeLabelRow
                                                anchors.centerIn: parent
                                                spacing: 4
                                                Text {
                                                    text: modelData.label || ""
                                                    color: "#FFFFFF"
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
                                    color: "#161B22"
                                    radius: 6
                                    border.color: modelData.isDiagram ? "#2F4F6F" : "#30363D"
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
                                            color: "#1C2128"
                                            radius: 6

                                            Rectangle {
                                                anchors.left: parent.left
                                                anchors.right: parent.right
                                                anchors.bottom: parent.bottom
                                                height: 6
                                                color: "#1C2128"
                                            }

                                            Rectangle {
                                                anchors.left: parent.left
                                                anchors.right: parent.right
                                                anchors.bottom: parent.bottom
                                                height: 1
                                                color: "#30363D"
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
                                                    color: modelData.isDiagram ? "#79C0FF" : "#8B949E"
                                                    font.family: root.monoFont
                                                    font.pixelSize: 10
                                                    font.bold: true
                                                }

                                                Text {
                                                    visible: modelData.isDiagram === true
                                                    text: "Diagramma sorgente"
                                                    color: "#6E7681"
                                                    font.pixelSize: 10
                                                }

                                                Item {
                                                    Layout.fillWidth: true
                                                }

                                                Button {
                                                    id: copyCodeBtn
                                                    Layout.preferredHeight: 20
                                                    background: Rectangle {
                                                        color: codeCard.copiedBlock ? "#064E3B" : (copyCodeBtn.hovered ? "#30363D" : "#21262D")
                                                        radius: 4
                                                        border.color: codeCard.copiedBlock ? "#10B981" : "#30363D"
                                                        border.width: 1
                                                    }
                                                    contentItem: Text {
                                                        text: codeCard.copiedBlock ? "✓ Copiato" : "Copia"
                                                        color: codeCard.copiedBlock ? "#6EE7B7" : "#CBD5E1"
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
                                                color: "#E6EDF3"
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
                                        if (alertKind === "note") return "#58A6FF";
                                        if (alertKind === "tip") return "#3FB950";
                                        if (alertKind === "important") return "#A371F7";
                                        if (alertKind === "warning") return "#D29922";
                                        if (alertKind === "caution") return "#F85149";
                                        return "#3B434B";
                                    }
                                    color: alertKind === "none" ? "#11161D" : "#141A23"
                                    border.color: alertKind === "none" ? "#21262D" : accentColor
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
                                                var a = modelData.alertType;
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
                                            color: (modelData.alertType || "none") === "none" ? "#9198A1" : "#E6EDF3"
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
                                            color: modelData.taskChecked ? "#238636" : "#161B22"
                                            border.color: modelData.taskChecked ? "#3FB950" : "#484F58"
                                            border.width: 1

                                            Text {
                                                visible: modelData.taskChecked === true
                                                anchors.centerIn: parent
                                                text: "✓"
                                                color: "#FFFFFF"
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
                                            color: modelData.ordered ? "#8B949E" : "#58A6FF"
                                            font.pixelSize: Math.round((modelData.ordered ? 12.5 : 15) * root.zoomScale)
                                            font.bold: true
                                        }
                                    }

                                    Text {
                                        Layout.fillWidth: true
                                        text: modelData.html || ""
                                        textFormat: Text.RichText
                                        wrapMode: Text.Wrap
                                        color: "#E6EDF3"
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
                                    color: "#0D1117"
                                    radius: 6
                                    border.color: "#30363D"
                                    border.width: 1
                                    clip: true

                                    readonly property var headers: modelData.tableHeaders || []
                                    readonly property var aligns: modelData.tableAlignments || []
                                    readonly property var rows: modelData.tableRows || []
                                    readonly property int colCount: Math.max(1, headers.length)

                                    readonly property var flatCells: {
                                        var out = [];
                                        for (var h = 0; h < headers.length; ++h) {
                                            out.push({
                                                text: headers[h],
                                                align: (h < aligns.length) ? aligns[h] : "left",
                                                isHeader: true,
                                                rowIdx: -1
                                            });
                                        }
                                        for (var r = 0; r < rows.length; ++r) {
                                            var row = rows[r];
                                            for (var c = 0; c < headers.length; ++c) {
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
                                                        ? "#161B22"
                                                        : (modelData.rowIdx % 2 === 0 ? "#0D1117" : "#12171F")
                                                    border.color: "#30363D"
                                                    border.width: 0.5

                                                    Text {
                                                        id: cellText
                                                        anchors.centerIn: parent
                                                        width: Math.max(40, parent.width - 20)
                                                        text: modelData.text || ""
                                                        textFormat: Text.RichText
                                                        wrapMode: Text.Wrap
                                                        color: modelData.isHeader ? "#F0F6FC" : "#E6EDF3"
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
                                    color: "#21262D"
                                }

                                // 9. DELETED SNIPPET OR INLINE REMOVED LINES (Rich Diff)
                                Rectangle {
                                    visible: blockDelegate.showRichDiff && (
                                        modelData.kind === "deletedSnippet" ||
                                        (modelData.changeStatus === "modified" && modelData.deletedLinesText && modelData.deletedLinesText.length > 0)
                                    )
                                    Layout.fillWidth: true
                                    implicitHeight: deletedBoxCol.implicitHeight + 16
                                    color: "#221013"
                                    radius: 5
                                    border.color: "#7F1D1D"
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
                                            color: "#F87171"
                                            font.pixelSize: 10
                                            font.bold: true
                                        }

                                        Text {
                                            Layout.fillWidth: true
                                            text: modelData.deletedLinesText || ""
                                            color: "#FFA198"
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
