import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root
    color: "#0D1117"

    property var sessionVm: null
    property var diffViewer: (sessionVm && sessionVm.diffViewer) ? sessionVm.diffViewer : null
    property bool showBackButton: false
    signal backRequested()

    readonly property string monoFont: (typeof gfMonoFont !== "undefined" && gfMonoFont) ? gfMonoFont : "Consolas"
    property bool wrapMarkdown: true
    property bool wrapCode: false
    readonly property bool isMarkdown: (root.diffViewer && root.diffViewer.isMarkdown === true)
    readonly property bool effectiveWrap: isMarkdown ? wrapMarkdown : wrapCode
    readonly property bool showingMarkdownPreview: (root.diffViewer && root.diffViewer.isMarkdownRendered === true && !root.diffViewer.isBinary && !root.diffViewer.isEmpty)

    property real horizontalScrollX: 0

    TextMetrics {
        id: monoMetrics
        font.family: root.monoFont
        font.pixelSize: 12
        text: "M"
    }

    readonly property real charWidth: Math.max(7.5, monoMetrics.width)
    readonly property int maxLineLength: (root.diffViewer && root.diffViewer.maxLineLength > 0) ? root.diffViewer.maxLineLength : 0
    readonly property real maxCodeWidth: maxLineLength > 0 ? (maxLineLength * charWidth + 80) : 0

    readonly property real inlineAvailWidth: Math.max(100, root.width - 108)
    readonly property real inlineMaxScroll: root.effectiveWrap ? 0 : Math.max(0, maxCodeWidth - inlineAvailWidth)

    readonly property real splitAvailWidth: Math.max(100, Math.floor(root.width / 2) - 44)
    readonly property real splitMaxScroll: root.effectiveWrap ? 0 : Math.max(0, maxCodeWidth - splitAvailWidth)

    readonly property real blameAvailWidth: Math.max(100, root.width - 320)
    readonly property real blameMaxScroll: Math.max(0, maxCodeWidth - blameAvailWidth)

    readonly property real currentMaxScroll: {
        var mode = root.diffViewer ? root.diffViewer.viewMode : "split";
        if (mode === "inline") return inlineMaxScroll;
        if (mode === "blame") return blameMaxScroll;
        return splitMaxScroll;
    }

    onCurrentMaxScrollChanged: {
        if (root.horizontalScrollX > currentMaxScroll) {
            root.horizontalScrollX = currentMaxScroll;
        }
    }

    onEffectiveWrapChanged: {
        if (root.effectiveWrap) {
            root.horizontalScrollX = 0;
        }
    }

    property string lastLoadedFileKey: ""
    property bool pendingScrollRestore: false
    property real savedScrollRatio: 0
    property string savedScrollFileKey: ""

    function currentFileKey() {
        var rev = (root.diffViewer && root.diffViewer.currentRevision) ? root.diffViewer.currentRevision : "";
        var file = (root.diffViewer && root.diffViewer.filePath) ? root.diffViewer.filePath : "";
        return rev + "::" + file;
    }

    function activeVerticalListView() {
        if (!root.diffViewer || root.showingMarkdownPreview) return null;
        var mode = root.diffViewer.viewMode || "split";
        if (mode === "inline") return inlineListView;
        if (mode === "hunk") return hunkListView;
        if (mode === "blame") return blameListView;
        return splitListView;
    }

    Connections {
        target: root.diffViewer
        function onContextLinesChanged() {
            var lv = root.activeVerticalListView();
            if (lv && lv.contentHeight > lv.height) {
                root.savedScrollRatio = Math.max(0, Math.min(1.0, lv.contentY / (lv.contentHeight - lv.height)));
                root.savedScrollFileKey = root.currentFileKey();
                root.pendingScrollRestore = true;
            }
        }
        function onDiffLoaded() {
            var key = root.currentFileKey();
            if (key !== root.lastLoadedFileKey) {
                root.lastLoadedFileKey = key;
                root.horizontalScrollX = 0;
                root.pendingScrollRestore = false;
            } else {
                // Same file reloaded (e.g. background polling or context expansion):
                if (root.horizontalScrollX > root.currentMaxScroll) {
                    root.horizontalScrollX = root.currentMaxScroll;
                }
                if (root.pendingScrollRestore && key === root.savedScrollFileKey) {
                    var ratio = root.savedScrollRatio;
                    root.pendingScrollRestore = false;
                    Qt.callLater(function() {
                        var lv = root.activeVerticalListView();
                        if (lv && lv.contentHeight > lv.height) {
                            lv.contentY = Math.max(0, Math.min(lv.contentHeight - lv.height, ratio * (lv.contentHeight - lv.height)));
                        }
                    });
                }
            }
        }
        function onViewModeChanged() {
            root.horizontalScrollX = 0;
        }
    }

    component DiffHScrollBar: Rectangle {
        id: hbar
        property real contentWidth: 0
        property real viewWidth: 0
        property real scrollValue: 0
        property real maxScroll: Math.max(0, contentWidth - viewWidth)
        signal scrollRequested(real newValue)

        height: 10
        color: "#0F141C"
        border.color: "#1E293B"
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
                   ? "#00E5FF"
                   : (hThumbArea.containsMouse || trackMouseArea.containsMouse ? "#38BDF8" : "#475569")
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
                    var pt = mapToItem(hbar, mouse.x, mouse.y);
                    startThumbTrackX = pt.x;
                    startScroll = hbar.scrollValue;
                    isDragging = true;
                }
                onPositionChanged: (mouse) => {
                    if (isDragging && pressed) {
                        var pt = mapToItem(hbar, mouse.x, mouse.y);
                        var deltaX = pt.x - startThumbTrackX;
                        var maxThumbX = Math.max(1, hbar.width - hThumb.width);
                        var targetScroll = Math.max(0, Math.min(hbar.maxScroll, startScroll + (deltaX / maxThumbX) * hbar.maxScroll));
                        hbar.scrollRequested(targetScroll);
                    }
                }
                onReleased: isDragging = false
                onCanceled: isDragging = false
            }
        }
    }

    // Inline Hunk Action Buttons component (+15 righe / File completo)
    component HunkExpandActions: Row {
        spacing: 6
        visible: root.diffViewer && !root.diffViewer.isFullContext

        Rectangle {
            height: 18
            width: plus15Text.implicitWidth + 10
            radius: 4
            color: plus15Mouse.containsMouse ? "#1E3A5F" : "#162436"
            border.color: plus15Mouse.containsMouse ? "#38BDF8" : "#334155"
            border.width: 1

            Text {
                id: plus15Text
                anchors.centerIn: parent
                text: "+15 righe"
                color: "#7DD3FC"
                font.pixelSize: 9
                font.bold: true
            }
            MouseArea {
                id: plus15Mouse
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: if (root.diffViewer) root.diffViewer.expandContext(15)
            }
        }

        Rectangle {
            height: 18
            width: fullCtxText.implicitWidth + 10
            radius: 4
            color: fullCtxMouse.containsMouse ? "#1E3A5F" : "#162436"
            border.color: fullCtxMouse.containsMouse ? "#38BDF8" : "#334155"
            border.width: 1

            Text {
                id: fullCtxText
                anchors.centerIn: parent
                text: "File completo"
                color: "#38BDF8"
                font.pixelSize: 9
                font.bold: true
            }
            MouseArea {
                id: fullCtxMouse
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: if (root.diffViewer) root.diffViewer.toggleFullContext()
            }
        }
    }

    component ContextOptionItem: MenuItem {
        id: optItem
        property bool selected: false
        implicitHeight: 28
        background: Rectangle {
            color: optItem.highlighted ? "#1E293B" : "transparent"
            radius: 4
        }
        contentItem: Text {
            text: optItem.text
            color: optItem.enabled ? (optItem.selected ? "#38BDF8" : "#E2E8F0") : "#64748B"
            font.pixelSize: 11
            font.bold: optItem.selected
            verticalAlignment: Text.AlignVCenter
            leftPadding: 8
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // ==========================================
        // DIFF VIEWER HEADER
        // ==========================================
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 44
            color: "#161B22"
            border.color: "#21262D"
            border.width: 1

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 14
                anchors.rightMargin: 14
                spacing: 8

                // Back Button (shown when diff view replaces the graph in GitKraken mode)
                Button {
                    visible: root.showBackButton
                    Layout.preferredHeight: 28
                    background: Rectangle {
                        color: parent.hovered ? "#334155" : "#1E293B"
                        radius: 4
                        border.color: "#475569"
                        border.width: 1
                    }
                    contentItem: RowLayout {
                        spacing: 6
                        Text {
                            text: "←"
                            color: "#38BDF8"
                            font.bold: true
                            font.pixelSize: 13
                        }
                        Text {
                            text: "Torna al grafo"
                            color: "#F1F5F9"
                            font.bold: true
                            font.pixelSize: 11
                        }
                    }
                    onClicked: root.backRequested()
                }

                Text {
                    text: (root.diffViewer && root.diffViewer.filePath !== "") ? root.diffViewer.filePath : "Nessun file selezionato"
                    color: "#F1F5F9"
                    font.bold: true
                    font.pixelSize: 13
                    elide: Text.ElideMiddle
                    Layout.fillWidth: true
                    Layout.minimumWidth: 80
                }

                // Type Badge (NEW, DELETED, RENAMED, BINARY, MODIFIED)
                Rectangle {
                    visible: root.diffViewer && !root.diffViewer.isEmpty
                    Layout.preferredHeight: 20
                    implicitWidth: typeText.implicitWidth + 10
                    radius: 3
                    color: {
                        if (!root.diffViewer) return "transparent";
                        if (root.diffViewer.isNewFile) return "#065F46";
                        if (root.diffViewer.isDeletedFile) return "#7F1D1D";
                        if (root.diffViewer.isRenamed) return "#1E3A8A";
                        if (root.diffViewer.isBinary) return "#475569";
                        return "#78350F";
                    }

                    Text {
                        id: typeText
                        anchors.centerIn: parent
                        text: {
                            if (!root.diffViewer) return "";
                            if (root.diffViewer.isNewFile) return "NEW";
                            if (root.diffViewer.isDeletedFile) return "DELETED";
                            if (root.diffViewer.isRenamed) return "RENAMED";
                            if (root.diffViewer.isBinary) return "BINARY";
                            return "MODIFIED";
                        }
                        color: "#FFFFFF"
                        font.pixelSize: 10
                        font.bold: true
                    }
                }

                // Language Badge
                Rectangle {
                    visible: root.diffViewer && !root.diffViewer.isEmpty && root.diffViewer.languageName && root.diffViewer.languageName.length > 0
                    Layout.preferredHeight: 20
                    implicitWidth: langRow.implicitWidth + 12
                    radius: 3
                    color: "#1E293B"
                    border.color: "#334155"
                    border.width: 1

                    RowLayout {
                        id: langRow
                        anchors.centerIn: parent
                        spacing: 4
                        Text {
                            text: root.diffViewer ? root.diffViewer.languageIcon : "📄"
                            font.pixelSize: 10
                        }
                        Text {
                            text: root.diffViewer ? root.diffViewer.languageName : ""
                            color: "#38BDF8"
                            font.pixelSize: 10
                            font.bold: true
                        }
                    }
                }

                // Markdown Segmented Switch: Diff <> vs Anteprima 👁
                Rectangle {
                    visible: root.isMarkdown && root.diffViewer && !root.diffViewer.isEmpty && !root.diffViewer.isBinary
                    Layout.preferredHeight: 26
                    implicitWidth: mdToggleRow.implicitWidth + 4
                    color: "#0D1117"
                    radius: 5
                    border.color: "#30363D"
                    border.width: 1

                    RowLayout {
                        id: mdToggleRow
                        anchors.centerIn: parent
                        spacing: 2

                        Button {
                            id: mdDiffSourceBtn
                            Layout.preferredHeight: 22
                            readonly property bool active: !root.showingMarkdownPreview
                            background: Rectangle {
                                color: mdDiffSourceBtn.active ? "#1E293B" : (mdDiffSourceBtn.hovered ? "#161B22" : "transparent")
                                radius: 4
                                border.color: mdDiffSourceBtn.active ? "#38BDF8" : "transparent"
                                border.width: 1
                            }
                            contentItem: Text {
                                text: "<> Diff"
                                color: mdDiffSourceBtn.active ? "#38BDF8" : "#94A3B8"
                                font.pixelSize: 11
                                font.bold: mdDiffSourceBtn.active
                            }
                            onClicked: if (root.diffViewer) root.diffViewer.setMarkdownRendered(false)
                        }

                        Button {
                            id: mdPreviewBtn
                            Layout.preferredHeight: 22
                            readonly property bool active: root.showingMarkdownPreview
                            background: Rectangle {
                                color: mdPreviewBtn.active ? "#1E293B" : (mdPreviewBtn.hovered ? "#161B22" : "transparent")
                                radius: 4
                                border.color: mdPreviewBtn.active ? "#3FB950" : "transparent"
                                border.width: 1
                            }
                            contentItem: Text {
                                text: "👁 Anteprima"
                                color: mdPreviewBtn.active ? "#7EE787" : "#94A3B8"
                                font.pixelSize: 11
                                font.bold: mdPreviewBtn.active
                            }
                            onClicked: if (root.diffViewer) root.diffViewer.setMarkdownRendered(true)
                        }
                    }
                }

                // Context Lines Selector (for ALL files when in source diff view)
                Button {
                    id: contextBtn
                    visible: root.diffViewer && !root.diffViewer.isEmpty && !root.diffViewer.isBinary &&
                             !root.showingMarkdownPreview && root.diffViewer.viewMode !== "blame"
                    Layout.preferredHeight: 26
                    background: Rectangle {
                        color: (root.diffViewer && root.diffViewer.isFullContext) ? "#172536" : (contextBtn.hovered ? "#1E293B" : "transparent")
                        radius: 4
                        border.color: (root.diffViewer && root.diffViewer.isFullContext) ? "#38BDF8" : "#334155"
                        border.width: 1
                    }
                    contentItem: RowLayout {
                        spacing: 4
                        Text {
                            text: "↕"
                            color: "#38BDF8"
                            font.pixelSize: 11
                            font.bold: true
                        }
                        Text {
                            text: {
                                if (!root.diffViewer) return "Contesto: 10";
                                if (root.diffViewer.isFullContext) return "File completo";
                                return "Contesto: " + root.diffViewer.contextLines;
                            }
                            color: (root.diffViewer && root.diffViewer.isFullContext) ? "#7DD3FC" : "#CBD5E1"
                            font.pixelSize: 11
                            font.bold: root.diffViewer && root.diffViewer.isFullContext
                        }
                        Text {
                            text: "▾"
                            color: "#64748B"
                            font.pixelSize: 9
                        }
                    }
                    onClicked: contextMenu.open()

                    Menu {
                        id: contextMenu
                        y: contextBtn.height + 4

                        background: Rectangle {
                            implicitWidth: 205
                            color: "#161B22"
                            border.color: "#30363D"
                            border.width: 1
                            radius: 6
                        }

                        ContextOptionItem {
                            text: "3 righe (compatto)"
                            selected: root.diffViewer && root.diffViewer.contextLines === 3
                            onTriggered: if (root.diffViewer) root.diffViewer.setContextLines(3)
                        }
                        ContextOptionItem {
                            text: "10 righe (predefinito)"
                            selected: root.diffViewer && root.diffViewer.contextLines === 10
                            onTriggered: if (root.diffViewer) root.diffViewer.setContextLines(10)
                        }
                        ContextOptionItem {
                            text: "25 righe"
                            selected: root.diffViewer && root.diffViewer.contextLines === 25
                            onTriggered: if (root.diffViewer) root.diffViewer.setContextLines(25)
                        }
                        ContextOptionItem {
                            text: "50 righe"
                            selected: root.diffViewer && root.diffViewer.contextLines === 50
                            onTriggered: if (root.diffViewer) root.diffViewer.setContextLines(50)
                        }
                        MenuSeparator {
                            contentItem: Rectangle {
                                implicitHeight: 1
                                color: "#21262D"
                            }
                        }
                        ContextOptionItem {
                            text: "+15 righe extra"
                            enabled: root.diffViewer && !root.diffViewer.isFullContext
                            onTriggered: if (root.diffViewer) root.diffViewer.expandContext(15)
                        }
                        ContextOptionItem {
                            text: (root.diffViewer && root.diffViewer.isFullContext) ? "Torna al contesto ridotto" : "Mostra file completo"
                            selected: root.diffViewer && root.diffViewer.isFullContext
                            onTriggered: if (root.diffViewer) root.diffViewer.toggleFullContext()
                        }
                    }
                }

                // Word Wrap Toggle (Inline, Split, Hunk)
                Button {
                    id: wrapBtn
                    visible: root.diffViewer && !root.diffViewer.isEmpty && !root.diffViewer.isBinary &&
                             !root.showingMarkdownPreview && root.diffViewer.viewMode !== "blame"
                    Layout.preferredHeight: 26
                    background: Rectangle {
                        color: root.effectiveWrap ? "#1E293B" : (wrapBtn.hovered ? "#161B22" : "transparent")
                        radius: 4
                        border.color: root.effectiveWrap ? "#38BDF8" : "#334155"
                        border.width: 1
                    }
                    contentItem: Text {
                        text: "↩ A capo"
                        color: root.effectiveWrap ? "#38BDF8" : "#94A3B8"
                        font.pixelSize: 11
                        font.bold: root.effectiveWrap
                    }
                    onClicked: {
                        if (root.isMarkdown) {
                            root.wrapMarkdown = !root.wrapMarkdown;
                        } else {
                            root.wrapCode = !root.wrapCode;
                        }
                    }
                }

                // View Mode Switcher Buttons (Inline, Split, Hunk, Blame)
                RowLayout {
                    visible: root.diffViewer && !root.diffViewer.isEmpty && !root.diffViewer.isBinary && !root.showingMarkdownPreview
                    spacing: 4

                    // 1. Inline View
                    Button {
                        id: inlineBtn
                        Layout.preferredHeight: 26
                        readonly property bool isCurrent: root.diffViewer ? root.diffViewer.viewMode === "inline" : false
                        background: Rectangle {
                            color: inlineBtn.isCurrent ? "#1E293B" : (inlineBtn.hovered ? "#161B22" : "transparent")
                            radius: 4
                            border.color: inlineBtn.isCurrent ? "#38BDF8" : "#334155"
                            border.width: 1
                        }
                        contentItem: RowLayout {
                            spacing: 4
                            Text { text: "📄"; font.pixelSize: 10 }
                            Text {
                                text: "Inline"
                                color: inlineBtn.isCurrent ? "#38BDF8" : "#94A3B8"
                                font.pixelSize: 11
                                font.bold: inlineBtn.isCurrent
                            }
                        }
                        onClicked: {
                            if (root.diffViewer) root.diffViewer.setViewMode("inline");
                        }
                    }

                    // 2. Split View (Side-by-side)
                    Button {
                        id: splitBtn
                        Layout.preferredHeight: 26
                        readonly property bool isCurrent: root.diffViewer ? (root.diffViewer.viewMode === "split" || root.diffViewer.viewMode === "") : true
                        background: Rectangle {
                            color: splitBtn.isCurrent ? "#1E293B" : (splitBtn.hovered ? "#161B22" : "transparent")
                            radius: 4
                            border.color: splitBtn.isCurrent ? "#38BDF8" : "#334155"
                            border.width: 1
                        }
                        contentItem: RowLayout {
                            spacing: 4
                            Text { text: "📑"; font.pixelSize: 10 }
                            Text {
                                text: "Split"
                                color: splitBtn.isCurrent ? "#38BDF8" : "#94A3B8"
                                font.pixelSize: 11
                                font.bold: splitBtn.isCurrent
                            }
                        }
                        onClicked: {
                            if (root.diffViewer) root.diffViewer.setViewMode("split");
                        }
                    }

                    // 3. Hunk View
                    Button {
                        id: hunkBtn
                        Layout.preferredHeight: 26
                        readonly property bool isCurrent: root.diffViewer ? root.diffViewer.viewMode === "hunk" : false
                        background: Rectangle {
                            color: hunkBtn.isCurrent ? "#1E293B" : (hunkBtn.hovered ? "#161B22" : "transparent")
                            radius: 4
                            border.color: hunkBtn.isCurrent ? "#38BDF8" : "#334155"
                            border.width: 1
                        }
                        contentItem: RowLayout {
                            spacing: 4
                            Text { text: "🔍"; font.pixelSize: 10 }
                            Text {
                                text: "Hunk"
                                color: hunkBtn.isCurrent ? "#38BDF8" : "#94A3B8"
                                font.pixelSize: 11
                                font.bold: hunkBtn.isCurrent
                            }
                        }
                        onClicked: {
                            if (root.diffViewer) root.diffViewer.setViewMode("hunk");
                        }
                    }

                    // 4. Blame View
                    Button {
                        id: blameBtn
                        Layout.preferredHeight: 26
                        readonly property bool isCurrent: root.diffViewer ? root.diffViewer.viewMode === "blame" : false
                        background: Rectangle {
                            color: blameBtn.isCurrent ? "#1E293B" : (blameBtn.hovered ? "#161B22" : "transparent")
                            radius: 4
                            border.color: blameBtn.isCurrent ? "#38BDF8" : "#334155"
                            border.width: 1
                        }
                        contentItem: RowLayout {
                            spacing: 4
                            Text { text: "👤"; font.pixelSize: 10 }
                            Text {
                                text: "Blame"
                                color: blameBtn.isCurrent ? "#38BDF8" : "#94A3B8"
                                font.pixelSize: 11
                                font.bold: blameBtn.isCurrent
                            }
                        }
                        onClicked: {
                            if (root.diffViewer) root.diffViewer.setViewMode("blame");
                        }
                    }
                }

                // Additions / Deletions stats
                RowLayout {
                    visible: root.diffViewer && !root.diffViewer.isEmpty && !root.diffViewer.isBinary
                    spacing: 6

                    Text {
                        text: root.diffViewer ? ("+" + root.diffViewer.additionsCount) : "+0"
                        color: "#34D399"
                        font.bold: true
                        font.pixelSize: 12
                    }

                    Text {
                        text: root.diffViewer ? ("-" + root.diffViewer.deletionsCount) : "-0"
                        color: "#F87171"
                        font.bold: true
                        font.pixelSize: 12
                    }
                }
            }
        }

        // ==========================================
        // DIFF CONTENT AREA
        // ==========================================
        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true

            // Empty State
            Text {
                anchors.centerIn: parent
                visible: !root.diffViewer || root.diffViewer.isEmpty
                text: "Seleziona un file per visualizzarne il diff o il blame"
                color: "#64748B"
                font.pixelSize: 13
            }

            // Binary File Notice
            Text {
                anchors.centerIn: parent
                visible: root.diffViewer && root.diffViewer.isBinary
                text: "📦 File binario — anteprima delle differenze testuali non disponibile"
                color: "#94A3B8"
                font.pixelSize: 13
            }

            // ------------------------------------------
            // 0. MARKDOWN RENDERED PREVIEW (GitHub Style)
            // ------------------------------------------
            MarkdownPreviewView {
                anchors.fill: parent
                visible: root.showingMarkdownPreview
                diffViewer: root.diffViewer
                monoFont: root.monoFont
            }

            // ------------------------------------------
            // 1. INLINE VIEW (Unified Diff)
            // ------------------------------------------
            Item {
                anchors.fill: parent
                visible: !root.showingMarkdownPreview && root.diffViewer && !root.diffViewer.isEmpty && !root.diffViewer.isBinary && root.diffViewer.viewMode === "inline"
                clip: true

                ListView {
                    id: inlineListView
                    anchors.fill: parent
                    anchors.bottomMargin: inlineHScrollBar.visible ? 12 : 0
                    focus: true
                    model: (!root.showingMarkdownPreview && root.diffViewer && root.diffViewer.viewMode === "inline") ? root.diffViewer.linesModel : null
                    boundsBehavior: Flickable.StopAtBounds
                    maximumFlickVelocity: 6000
                    flickDeceleration: 1000
                    ScrollBar.vertical: FluxScrollBar {}

                    Keys.onLeftPressed: (event) => {
                        if (root.inlineMaxScroll > 0) {
                            root.horizontalScrollX = Math.max(0, root.horizontalScrollX - 40);
                            event.accepted = true;
                        }
                    }
                    Keys.onRightPressed: (event) => {
                        if (root.inlineMaxScroll > 0) {
                            root.horizontalScrollX = Math.min(root.inlineMaxScroll, root.horizontalScrollX + 40);
                            event.accepted = true;
                        }
                    }

                    delegate: Rectangle {
                        id: inlineRowDelegate
                        width: inlineListView.width
                        readonly property bool isHeaderRow: model.lineKind === "header"
                        height: isHeaderRow
                                    ? 28
                                    : (root.effectiveWrap ? Math.max(20, inlineCodeText.implicitHeight + 6) : 20)
                        color: model.bgColor

                        Item {
                            anchors.fill: parent

                            // Pinned Left Gutter (Line Numbers + Indicator)
                            Row {
                                id: inlineGutter
                                anchors.left: parent.left
                                anchors.top: parent.top
                                anchors.bottom: parent.bottom
                                width: 108
                                z: 2

                                // Old Line Number
                                Rectangle {
                                    width: 44
                                    height: parent.height
                                    color: "#090D13"
                                    Text {
                                        anchors.fill: parent
                                        anchors.rightMargin: 6
                                        anchors.topMargin: 2
                                        text: model.oldLineNumber || ""
                                        color: "#64748B"
                                        font.family: root.monoFont
                                        font.pixelSize: 11
                                        horizontalAlignment: Text.AlignRight
                                        verticalAlignment: root.effectiveWrap ? Text.AlignTop : Text.AlignVCenter
                                    }
                                }

                                // New Line Number
                                Rectangle {
                                    width: 44
                                    height: parent.height
                                    color: "#0B1017"
                                    Text {
                                        anchors.fill: parent
                                        anchors.rightMargin: 6
                                        anchors.topMargin: 2
                                        text: model.newLineNumber || ""
                                        color: "#64748B"
                                        font.family: root.monoFont
                                        font.pixelSize: 11
                                        horizontalAlignment: Text.AlignRight
                                        verticalAlignment: root.effectiveWrap ? Text.AlignTop : Text.AlignVCenter
                                    }
                                }

                                // Prefix indicator (+/-)
                                Rectangle {
                                    width: 20
                                    height: parent.height
                                    color: "transparent"
                                    Text {
                                        anchors.horizontalCenter: parent.horizontalCenter
                                        anchors.top: parent.top
                                        anchors.topMargin: root.effectiveWrap ? 2 : Math.floor((parent.height - implicitHeight) / 2)
                                        text: {
                                            if (model.lineKind === "addition") return "+";
                                            if (model.lineKind === "deletion") return "-";
                                            return " ";
                                        }
                                        color: model.fgColor
                                        font.family: root.monoFont
                                        font.pixelSize: 12
                                        font.bold: true
                                    }
                                }
                            }

                            // Content Line
                            Item {
                                anchors.left: inlineGutter.right
                                anchors.right: parent.right
                                anchors.top: parent.top
                                anchors.bottom: parent.bottom
                                clip: true

                                Text {
                                    id: inlineCodeText
                                    x: root.effectiveWrap ? 6 : (6 - root.horizontalScrollX)
                                    anchors.verticalCenter: parent.verticalCenter
                                    width: (root.effectiveWrap && !inlineRowDelegate.isHeaderRow)
                                               ? Math.max(60, parent.width - 14)
                                               : Math.max(parent.width - 6, root.maxCodeWidth)
                                    text: model.richContent || model.content || ""
                                    textFormat: Text.RichText
                                    color: model.fgColor
                                    font.family: root.monoFont
                                    font.pixelSize: 12
                                    wrapMode: (root.effectiveWrap && !inlineRowDelegate.isHeaderRow) ? Text.WrapAtWordBoundaryOrAnywhere : Text.NoWrap
                                    elide: Text.ElideNone
                                }

                                HunkExpandActions {
                                    visible: inlineRowDelegate.isHeaderRow && root.diffViewer && !root.diffViewer.isFullContext
                                    anchors.right: parent.right
                                    anchors.rightMargin: 16
                                    anchors.verticalCenter: parent.verticalCenter
                                    z: 5
                                }
                            }
                        }
                    }

                    WheelHandler {
                        id: inlineHWheel
                        target: null
                        orientation: Qt.Horizontal
                        acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad | PointerDevice.TouchScreen
                        onWheel: (event) => {
                            if (root.inlineMaxScroll <= 0) return;
                            var dx = 0;
                            if (event.pixelDelta && event.pixelDelta.x !== 0) {
                                dx = event.pixelDelta.x;
                            } else if (event.angleDelta && event.angleDelta.x !== 0) {
                                dx = event.angleDelta.x;
                            }
                            if (dx !== 0) {
                                root.horizontalScrollX = Math.max(0, Math.min(root.inlineMaxScroll, root.horizontalScrollX - dx));
                                event.accepted = true;
                            }
                        }
                    }

                    WheelHandler {
                        id: inlineShiftWheel
                        target: null
                        orientation: Qt.Vertical
                        acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad | PointerDevice.TouchScreen
                        acceptedModifiers: Qt.ShiftModifier
                        onWheel: (event) => {
                            if (root.inlineMaxScroll <= 0) return;
                            var dy = 0;
                            if (event.pixelDelta && event.pixelDelta.y !== 0) {
                                dy = event.pixelDelta.y;
                            } else if (event.angleDelta && event.angleDelta.y !== 0) {
                                dy = event.angleDelta.y;
                            }
                            if (dy !== 0) {
                                root.horizontalScrollX = Math.max(0, Math.min(root.inlineMaxScroll, root.horizontalScrollX - dy));
                                event.accepted = true;
                            }
                        }
                    }
                }

                // Horizontal ScrollBar for Inline View
                DiffHScrollBar {
                    id: inlineHScrollBar
                    anchors.left: parent.left
                    anchors.leftMargin: 108
                    anchors.right: parent.right
                    anchors.rightMargin: 10
                    anchors.bottom: parent.bottom
                    contentWidth: root.effectiveWrap ? 0 : root.maxCodeWidth
                    viewWidth: root.inlineAvailWidth
                    scrollValue: root.horizontalScrollX
                    onScrollRequested: (val) => { root.horizontalScrollX = val; }
                }
            }

            // ------------------------------------------
            // 2. SPLIT VIEW (Side-by-Side Diff)
            // ------------------------------------------
            ColumnLayout {
                anchors.fill: parent
                visible: !root.showingMarkdownPreview && root.diffViewer && !root.diffViewer.isEmpty && !root.diffViewer.isBinary && (root.diffViewer.viewMode === "split" || root.diffViewer.viewMode === "")
                spacing: 0

                // Split Header (Original vs Modified)
                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 24
                    color: "#0F172A"
                    border.color: "#1E293B"
                    border.width: 1

                    RowLayout {
                        anchors.fill: parent
                        spacing: 0

                        Rectangle {
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            color: "transparent"
                            Text {
                                anchors.centerIn: parent
                                text: "Originale (Base)"
                                color: "#EF4444"
                                font.pixelSize: 11
                                font.bold: true
                            }
                        }

                        Rectangle {
                            width: 1
                            Layout.fillHeight: true
                            color: "#334155"
                        }

                        Rectangle {
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            color: "transparent"
                            Text {
                                anchors.centerIn: parent
                                text: "Modificato (Head)"
                                color: "#10B981"
                                font.pixelSize: 11
                                font.bold: true
                            }
                        }
                    }
                }

                Item {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true

                    ListView {
                        id: splitListView
                        anchors.fill: parent
                        anchors.bottomMargin: splitHScrollBar.visible ? 12 : 0
                        focus: true
                        model: (!root.showingMarkdownPreview && root.diffViewer && (root.diffViewer.viewMode === "split" || root.diffViewer.viewMode === "")) ? root.diffViewer.splitLines : null
                        boundsBehavior: Flickable.StopAtBounds
                        maximumFlickVelocity: 6000
                        flickDeceleration: 1000
                        ScrollBar.vertical: FluxScrollBar {}

                        Keys.onLeftPressed: (event) => {
                            if (root.splitMaxScroll > 0) {
                                root.horizontalScrollX = Math.max(0, root.horizontalScrollX - 40);
                                event.accepted = true;
                            }
                        }
                        Keys.onRightPressed: (event) => {
                            if (root.splitMaxScroll > 0) {
                                root.horizontalScrollX = Math.min(root.splitMaxScroll, root.horizontalScrollX + 40);
                                event.accepted = true;
                            }
                        }

                        delegate: Rectangle {
                            id: splitRow
                            width: splitListView.width
                            height: modelData.isHunkHeader
                                        ? 28
                                        : (root.effectiveWrap
                                               ? Math.max(20, leftCodeText.implicitHeight + 6, rightCodeText.implicitHeight + 6)
                                               : 20)
                            color: "#0D1117"

                            // Hunk banner across entire row
                            Rectangle {
                                anchors.fill: parent
                                visible: modelData.isHunkHeader === true
                                color: "#1E293B"
                                border.color: "#334155"
                                border.width: 1

                                Text {
                                    anchors.left: parent.left
                                    anchors.leftMargin: 12
                                    anchors.right: splitExpandActions.left
                                    anchors.rightMargin: 8
                                    anchors.verticalCenter: parent.verticalCenter
                                    text: modelData.hunkHeader || ""
                                    color: "#38BDF8"
                                    font.family: root.monoFont
                                    font.pixelSize: 10
                                    font.bold: true
                                    elide: Text.ElideRight
                                }

                                HunkExpandActions {
                                    id: splitExpandActions
                                    anchors.right: parent.right
                                    anchors.rightMargin: 16
                                    anchors.verticalCenter: parent.verticalCenter
                                }
                            }

                            // Side-by-side normal line row
                            RowLayout {
                                anchors.fill: parent
                                spacing: 0
                                visible: !modelData.isHunkHeader

                                // LEFT COLUMN (Original / Deletion)
                                Rectangle {
                                    Layout.preferredWidth: Math.floor(splitRow.width / 2)
                                    Layout.fillHeight: true
                                    color: modelData.leftBgColor || "transparent"

                                    RowLayout {
                                        anchors.fill: parent
                                        spacing: 0

                                        Rectangle {
                                            Layout.preferredWidth: 40
                                            Layout.fillHeight: true
                                            color: "#090D13"
                                            Text {
                                                anchors.fill: parent
                                                anchors.rightMargin: 6
                                                anchors.topMargin: 2
                                                text: modelData.leftLine || ""
                                                color: "#64748B"
                                                font.family: root.monoFont
                                                font.pixelSize: 10
                                                horizontalAlignment: Text.AlignRight
                                                verticalAlignment: root.effectiveWrap ? Text.AlignTop : Text.AlignVCenter
                                            }
                                        }

                                        Item {
                                            Layout.fillWidth: true
                                            Layout.fillHeight: true
                                            clip: true

                                            Text {
                                                id: leftCodeText
                                                x: root.effectiveWrap ? 6 : (6 - root.horizontalScrollX)
                                                anchors.verticalCenter: parent.verticalCenter
                                                width: root.effectiveWrap
                                                           ? Math.max(40, parent.width - 12)
                                                           : Math.max(parent.width - 6, root.maxCodeWidth)
                                                text: modelData.leftRichContent || modelData.leftContent || ""
                                                textFormat: Text.RichText
                                                color: modelData.leftFgColor || "#CBD5E1"
                                                font.family: root.monoFont
                                                font.pixelSize: 11
                                                wrapMode: root.effectiveWrap ? Text.WrapAtWordBoundaryOrAnywhere : Text.NoWrap
                                                elide: Text.ElideNone
                                            }
                                        }
                                    }
                                }

                                // Center column divider
                                Rectangle {
                                    width: 1
                                    Layout.fillHeight: true
                                    color: "#1E293B"
                                }

                                // RIGHT COLUMN (Modified / Addition)
                                Rectangle {
                                    Layout.fillWidth: true
                                    Layout.fillHeight: true
                                    color: modelData.rightBgColor || "transparent"

                                    RowLayout {
                                        anchors.fill: parent
                                        spacing: 0

                                        Rectangle {
                                            Layout.preferredWidth: 40
                                            Layout.fillHeight: true
                                            color: "#0B1017"
                                            Text {
                                                anchors.fill: parent
                                                anchors.rightMargin: 6
                                                anchors.topMargin: 2
                                                text: modelData.rightLine || ""
                                                color: "#64748B"
                                                font.family: root.monoFont
                                                font.pixelSize: 10
                                                horizontalAlignment: Text.AlignRight
                                                verticalAlignment: root.effectiveWrap ? Text.AlignTop : Text.AlignVCenter
                                            }
                                        }

                                        Item {
                                            Layout.fillWidth: true
                                            Layout.fillHeight: true
                                            clip: true

                                            Text {
                                                id: rightCodeText
                                                x: root.effectiveWrap ? 6 : (6 - root.horizontalScrollX)
                                                anchors.verticalCenter: parent.verticalCenter
                                                width: root.effectiveWrap
                                                           ? Math.max(40, parent.width - 12)
                                                           : Math.max(parent.width - 6, root.maxCodeWidth)
                                                text: modelData.rightRichContent || modelData.rightContent || ""
                                                textFormat: Text.RichText
                                                color: modelData.rightFgColor || "#CBD5E1"
                                                font.family: root.monoFont
                                                font.pixelSize: 11
                                                wrapMode: root.effectiveWrap ? Text.WrapAtWordBoundaryOrAnywhere : Text.NoWrap
                                                elide: Text.ElideNone
                                            }
                                        }
                                    }
                                }
                            }
                        }

                        WheelHandler {
                            id: splitHWheel
                            target: null
                            orientation: Qt.Horizontal
                            acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad | PointerDevice.TouchScreen
                            onWheel: (event) => {
                                if (root.splitMaxScroll <= 0) return;
                                var dx = 0;
                                if (event.pixelDelta && event.pixelDelta.x !== 0) {
                                    dx = event.pixelDelta.x;
                                } else if (event.angleDelta && event.angleDelta.x !== 0) {
                                    dx = event.angleDelta.x;
                                }
                                if (dx !== 0) {
                                    root.horizontalScrollX = Math.max(0, Math.min(root.splitMaxScroll, root.horizontalScrollX - dx));
                                    event.accepted = true;
                                }
                            }
                        }

                        WheelHandler {
                            id: splitShiftWheel
                            target: null
                            orientation: Qt.Vertical
                            acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad | PointerDevice.TouchScreen
                            acceptedModifiers: Qt.ShiftModifier
                            onWheel: (event) => {
                                if (root.splitMaxScroll <= 0) return;
                                var dy = 0;
                                if (event.pixelDelta && event.pixelDelta.y !== 0) {
                                    dy = event.pixelDelta.y;
                                } else if (event.angleDelta && event.angleDelta.y !== 0) {
                                    dy = event.angleDelta.y;
                                }
                                if (dy !== 0) {
                                    root.horizontalScrollX = Math.max(0, Math.min(root.splitMaxScroll, root.horizontalScrollX - dy));
                                    event.accepted = true;
                                }
                            }
                        }
                    }

                    // Horizontal ScrollBar for Split View
                    DiffHScrollBar {
                        id: splitHScrollBar
                        anchors.left: parent.left
                        anchors.leftMargin: 40
                        anchors.right: parent.right
                        anchors.rightMargin: 10
                        anchors.bottom: parent.bottom
                        contentWidth: root.effectiveWrap ? 0 : root.maxCodeWidth
                        viewWidth: root.splitAvailWidth
                        scrollValue: root.horizontalScrollX
                        onScrollRequested: (val) => { root.horizontalScrollX = val; }
                    }
                }
            }

            // ------------------------------------------
            // 3. HUNK VIEW (Cards per Hunk)
            // ------------------------------------------
            ScrollView {
                id: hunkScrollView
                anchors.fill: parent
                visible: !root.showingMarkdownPreview && root.diffViewer && !root.diffViewer.isEmpty && !root.diffViewer.isBinary && root.diffViewer.viewMode === "hunk"
                clip: true
                ScrollBar.vertical: FluxScrollBar {}
                ScrollBar.horizontal: FluxScrollBar { visible: !root.effectiveWrap }

                ListView {
                    id: hunkListView
                    width: root.effectiveWrap ? hunkScrollView.width : Math.max(hunkScrollView.width, root.maxCodeWidth + 120)
                    model: (!root.showingMarkdownPreview && root.diffViewer && root.diffViewer.viewMode === "hunk") ? root.diffViewer.hunks : null
                    spacing: 12
                    boundsBehavior: Flickable.StopAtBounds
                    maximumFlickVelocity: 6000
                    flickDeceleration: 1000

                    delegate: Rectangle {
                        x: 12
                        width: hunkListView.width - 24
                        implicitHeight: hunkCol.implicitHeight + 16
                        color: "#161B22"
                        radius: 6
                        border.color: "#30363D"
                        border.width: 1

                        ColumnLayout {
                            id: hunkCol
                            anchors.fill: parent
                            anchors.margins: 8
                            spacing: 4

                            // Hunk Header Banner
                            Rectangle {
                                Layout.fillWidth: true
                                Layout.preferredHeight: 30
                                color: "#0D1117"
                                radius: 4
                                border.color: "#21262D"

                                RowLayout {
                                    anchors.fill: parent
                                    anchors.leftMargin: 8
                                    anchors.rightMargin: 8
                                    spacing: 8
                                    Text {
                                        text: "🔍"
                                        font.pixelSize: 10
                                    }
                                    Text {
                                        text: modelData.header || ""
                                        color: "#38BDF8"
                                        font.family: root.monoFont
                                        font.pixelSize: 11
                                        font.bold: true
                                        elide: Text.ElideRight
                                        Layout.fillWidth: true
                                    }
                                    HunkExpandActions {}
                                }
                            }

                            // Hunk Lines
                            Repeater {
                                model: (modelData.lines && modelData.lines.length > 300) ? modelData.lines.slice(0, 300) : (modelData.lines || [])

                                delegate: Rectangle {
                                    Layout.fillWidth: true
                                    Layout.preferredHeight: root.effectiveWrap ? Math.max(20, hunkLineText.implicitHeight + 6) : 20
                                    color: modelData.bgColor || "transparent"

                                    RowLayout {
                                        anchors.fill: parent
                                        spacing: 6

                                        Text {
                                            Layout.preferredWidth: 36
                                            Layout.alignment: root.effectiveWrap ? Qt.AlignTop : Qt.AlignVCenter
                                            Layout.topMargin: root.effectiveWrap ? 2 : 0
                                            text: modelData.oldLineNumber || ""
                                            color: "#64748B"
                                            font.family: root.monoFont
                                            font.pixelSize: 10
                                            horizontalAlignment: Text.AlignRight
                                        }

                                        Text {
                                            Layout.preferredWidth: 36
                                            Layout.alignment: root.effectiveWrap ? Qt.AlignTop : Qt.AlignVCenter
                                            Layout.topMargin: root.effectiveWrap ? 2 : 0
                                            text: modelData.newLineNumber || ""
                                            color: "#64748B"
                                            font.family: root.monoFont
                                            font.pixelSize: 10
                                            horizontalAlignment: Text.AlignRight
                                        }

                                        Text {
                                            Layout.alignment: root.effectiveWrap ? Qt.AlignTop : Qt.AlignVCenter
                                            Layout.topMargin: root.effectiveWrap ? 2 : 0
                                            text: {
                                                if (modelData.kind === "addition") return "+";
                                                if (modelData.kind === "deletion") return "-";
                                                return " ";
                                            }
                                            color: modelData.fgColor || "#CBD5E1"
                                            font.family: root.monoFont
                                            font.pixelSize: 11
                                            font.bold: true
                                        }

                                        Text {
                                            id: hunkLineText
                                            Layout.fillWidth: true
                                            Layout.alignment: Qt.AlignVCenter
                                            text: modelData.richContent || modelData.content || ""
                                            textFormat: Text.RichText
                                            color: modelData.fgColor || "#CBD5E1"
                                            font.family: root.monoFont
                                            font.pixelSize: 11
                                            wrapMode: root.effectiveWrap ? Text.WrapAtWordBoundaryOrAnywhere : Text.NoWrap
                                            elide: Text.ElideNone
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }

            // ------------------------------------------
            // 4. BLAME VIEW (Line Annotations & Code)
            // ------------------------------------------
            Item {
                anchors.fill: parent
                visible: !root.showingMarkdownPreview && root.diffViewer && !root.diffViewer.isEmpty && !root.diffViewer.isBinary && root.diffViewer.viewMode === "blame"
                clip: true

                ListView {
                    id: blameListView
                    anchors.fill: parent
                    anchors.bottomMargin: blameHScrollBar.visible ? 12 : 0
                    focus: true
                    model: (!root.showingMarkdownPreview && root.diffViewer && root.diffViewer.viewMode === "blame") ? root.diffViewer.blameLines : null
                    boundsBehavior: Flickable.StopAtBounds
                    maximumFlickVelocity: 6000
                    flickDeceleration: 1000
                    ScrollBar.vertical: FluxScrollBar {}

                    Keys.onLeftPressed: (event) => {
                        if (root.blameMaxScroll > 0) {
                            root.horizontalScrollX = Math.max(0, root.horizontalScrollX - 40);
                            event.accepted = true;
                        }
                    }
                    Keys.onRightPressed: (event) => {
                        if (root.blameMaxScroll > 0) {
                            root.horizontalScrollX = Math.min(root.blameMaxScroll, root.horizontalScrollX + 40);
                            event.accepted = true;
                        }
                    }

                    delegate: Rectangle {
                        width: blameListView.width
                        height: 22
                        color: index % 2 === 0 ? "#0D1117" : "#111827"

                        RowLayout {
                            anchors.fill: parent
                            spacing: 8

                            // Blame Annotation Column (Pinned left)
                            Rectangle {
                                Layout.preferredWidth: 310
                                Layout.fillHeight: true
                                color: "#0B0F19"
                                border.color: "#1E293B"
                                border.width: 1

                                RowLayout {
                                    anchors.fill: parent
                                    anchors.leftMargin: 6
                                    anchors.rightMargin: 6
                                    spacing: 6

                                    // Author Avatar / Circle
                                    Rectangle {
                                        Layout.preferredWidth: 16
                                        Layout.preferredHeight: 16
                                        radius: 8
                                        color: modelData.color || "#6366F1"

                                        Image {
                                            id: blameAvatarImg
                                            anchors.fill: parent
                                            source: modelData.avatarUrl || ""
                                            fillMode: Image.PreserveAspectCrop
                                            smooth: true
                                            mipmap: true
                                        }

                                        Canvas {
                                            id: blameCornerMask
                                            anchors.fill: parent
                                            visible: blameAvatarImg.status === Image.Ready
                                            onVisibleChanged: if (visible) requestPaint()
                                            onAvailableChanged: if (available) requestPaint()
                                            onPaint: {
                                                var ctx = getContext("2d");
                                                ctx.reset();
                                                var cx = width / 2;
                                                var cy = height / 2;
                                                var r = Math.min(width, height) / 2 - 0.5;

                                                ctx.globalCompositeOperation = "source-over";
                                                ctx.fillStyle = "#0B0F19";
                                                ctx.fillRect(0, 0, width, height);

                                                ctx.globalCompositeOperation = "destination-out";
                                                ctx.beginPath();
                                                ctx.arc(cx, cy, r, 0, Math.PI * 2, false);
                                                ctx.closePath();
                                                ctx.fillStyle = "#000000";
                                                ctx.fill();
                                            }
                                        }

                                        Text {
                                            anchors.centerIn: parent
                                            text: modelData.initials || "??"
                                            color: "#FFFFFF"
                                            font.pixelSize: 8
                                            font.bold: true
                                            visible: blameAvatarImg.status !== Image.Ready
                                        }
                                    }

                                    // Short SHA
                                    Text {
                                        text: modelData.shortSha || ""
                                        color: "#38BDF8"
                                        font.family: root.monoFont
                                        font.pixelSize: 10
                                        Layout.preferredWidth: 54
                                    }

                                    // Author Name
                                    Text {
                                        text: modelData.author || ""
                                        color: "#CBD5E1"
                                        font.pixelSize: 10
                                        elide: Text.ElideRight
                                        Layout.fillWidth: true
                                    }

                                    // Date
                                    Text {
                                        text: modelData.date || ""
                                        color: "#64748B"
                                        font.pixelSize: 10
                                    }

                                    // Line Number
                                    Text {
                                        text: modelData.lineNumber || ""
                                        color: "#94A3B8"
                                        font.family: root.monoFont
                                        font.pixelSize: 10
                                        Layout.preferredWidth: 28
                                        horizontalAlignment: Text.AlignRight
                                    }
                                }
                            }

                            // Code Line Content (Horizontally scrollable)
                            Item {
                                Layout.fillWidth: true
                                Layout.fillHeight: true
                                clip: true

                                Text {
                                    x: 6 - root.horizontalScrollX
                                    width: Math.max(parent.width - 6, root.maxCodeWidth)
                                    height: parent.height
                                    text: modelData.richContent || modelData.content || ""
                                    textFormat: Text.RichText
                                    color: "#F1F5F9"
                                    font.family: root.monoFont
                                    font.pixelSize: 11
                                    verticalAlignment: Text.AlignVCenter
                                    elide: Text.ElideNone
                                }
                            }
                        }
                    }

                    WheelHandler {
                        id: blameHWheel
                        target: null
                        orientation: Qt.Horizontal
                        acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad | PointerDevice.TouchScreen
                        onWheel: (event) => {
                            if (root.blameMaxScroll <= 0) return;
                            var dx = 0;
                            if (event.pixelDelta && event.pixelDelta.x !== 0) {
                                dx = event.pixelDelta.x;
                            } else if (event.angleDelta && event.angleDelta.x !== 0) {
                                dx = event.angleDelta.x;
                            }
                            if (dx !== 0) {
                                root.horizontalScrollX = Math.max(0, Math.min(root.blameMaxScroll, root.horizontalScrollX - dx));
                                event.accepted = true;
                            }
                        }
                    }

                    WheelHandler {
                        id: blameShiftWheel
                        target: null
                        orientation: Qt.Vertical
                        acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad | PointerDevice.TouchScreen
                        acceptedModifiers: Qt.ShiftModifier
                        onWheel: (event) => {
                            if (root.blameMaxScroll <= 0) return;
                            var dy = 0;
                            if (event.pixelDelta && event.pixelDelta.y !== 0) {
                                dy = event.pixelDelta.y;
                            } else if (event.angleDelta && event.angleDelta.y !== 0) {
                                dy = event.angleDelta.y;
                            }
                            if (dy !== 0) {
                                root.horizontalScrollX = Math.max(0, Math.min(root.blameMaxScroll, root.horizontalScrollX - dy));
                                event.accepted = true;
                            }
                        }
                    }
                }

                // Horizontal ScrollBar for Blame View
                DiffHScrollBar {
                    id: blameHScrollBar
                    anchors.left: parent.left
                    anchors.leftMargin: 318
                    anchors.right: parent.right
                    anchors.rightMargin: 10
                    anchors.bottom: parent.bottom
                    contentWidth: root.maxCodeWidth
                    viewWidth: root.blameAvailWidth
                    scrollValue: root.horizontalScrollX
                    onScrollRequested: (val) => { root.horizontalScrollX = val; }
                }
            }
        }
    }
}
