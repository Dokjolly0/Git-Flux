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

    property real horizontalScrollX: 0

    TextMetrics {
        id: monoMetrics
        font.family: "monospace"
        font.pixelSize: 12
        text: "M"
    }

    readonly property real charWidth: Math.max(7.5, monoMetrics.width)
    readonly property int maxLineLength: (root.diffViewer && root.diffViewer.maxLineLength > 0) ? root.diffViewer.maxLineLength : 0
    readonly property real maxCodeWidth: maxLineLength > 0 ? (maxLineLength * charWidth + 80) : 0

    readonly property real inlineAvailWidth: Math.max(100, root.width - 108)
    readonly property real inlineMaxScroll: Math.max(0, maxCodeWidth - inlineAvailWidth)

    readonly property real splitAvailWidth: Math.max(100, Math.floor(root.width / 2) - 44)
    readonly property real splitMaxScroll: Math.max(0, maxCodeWidth - splitAvailWidth)

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

    property string lastLoadedFileKey: ""

    Connections {
        target: root.diffViewer
        function onDiffLoaded() {
            var rev = (root.diffViewer && root.diffViewer.currentRevision) ? root.diffViewer.currentRevision : "";
            var file = (root.diffViewer && root.diffViewer.filePath) ? root.diffViewer.filePath : "";
            var key = rev + "::" + file;
            if (key !== root.lastLoadedFileKey) {
                root.lastLoadedFileKey = key;
                root.horizontalScrollX = 0;
            } else {
                // Same file reloaded (e.g. background polling / git status check):
                // Preserve user scroll position! Clamp to max scroll if file became shorter.
                if (root.horizontalScrollX > root.currentMaxScroll) {
                    root.horizontalScrollX = root.currentMaxScroll;
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
                onReleased: {
                    isDragging = false;
                }
                onCanceled: {
                    isDragging = false;
                }
            }
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
                spacing: 10

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
                    Layout.maximumWidth: root.width * 0.35
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

                // Language Badge (e.g. ⚡ C++, 🐍 Python, ⚛ React / TSX)
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

                Item { Layout.fillWidth: true }

                // View Mode Switcher Buttons (Inline, Split, Hunk, Blame)
                RowLayout {
                    visible: root.diffViewer && !root.diffViewer.isEmpty && !root.diffViewer.isBinary
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
                    spacing: 8

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
            // 1. INLINE VIEW (Unified Diff)
            // ------------------------------------------
            Item {
                anchors.fill: parent
                visible: root.diffViewer && !root.diffViewer.isEmpty && !root.diffViewer.isBinary && root.diffViewer.viewMode === "inline"
                clip: true

                ListView {
                    id: inlineListView
                    anchors.fill: parent
                    anchors.bottomMargin: inlineHScrollBar.visible ? 12 : 0
                    focus: true
                    model: root.diffViewer ? root.diffViewer.linesModel : null
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
                        width: inlineListView.width
                        height: 20
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
                                        text: model.oldLineNumber || ""
                                        color: "#64748B"
                                        font.family: "monospace"
                                        font.pixelSize: 11
                                        horizontalAlignment: Text.AlignRight
                                        verticalAlignment: Text.AlignVCenter
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
                                        text: model.newLineNumber || ""
                                        color: "#64748B"
                                        font.family: "monospace"
                                        font.pixelSize: 11
                                        horizontalAlignment: Text.AlignRight
                                        verticalAlignment: Text.AlignVCenter
                                    }
                                }

                                // Prefix indicator (+/-)
                                Rectangle {
                                    width: 20
                                    height: parent.height
                                    color: "transparent"
                                    Text {
                                        anchors.centerIn: parent
                                        text: {
                                            if (model.lineKind === "addition") return "+";
                                            if (model.lineKind === "deletion") return "-";
                                            return " ";
                                        }
                                        color: model.fgColor
                                        font.family: "monospace"
                                        font.pixelSize: 12
                                        font.bold: true
                                    }
                                }
                            }

                            // Horizontally Scrollable Content Line
                            Item {
                                anchors.left: inlineGutter.right
                                anchors.right: parent.right
                                anchors.top: parent.top
                                anchors.bottom: parent.bottom
                                clip: true

                                Text {
                                    x: 6 - root.horizontalScrollX
                                    width: Math.max(implicitWidth, Math.max(parent.width - 6, root.maxCodeWidth))
                                    height: parent.height
                                    text: model.richContent || model.content || ""
                                    textFormat: Text.RichText
                                    color: model.fgColor
                                    font.family: "monospace"
                                    font.pixelSize: 12
                                    verticalAlignment: Text.AlignVCenter
                                    elide: Text.ElideNone
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
                    contentWidth: root.maxCodeWidth
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
                visible: root.diffViewer && !root.diffViewer.isEmpty && !root.diffViewer.isBinary && (root.diffViewer.viewMode === "split" || root.diffViewer.viewMode === "")
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
                        model: root.diffViewer ? root.diffViewer.splitLines : null
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
                            height: modelData.isHunkHeader ? 22 : 20
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
                                    anchors.verticalCenter: parent.verticalCenter
                                    text: modelData.hunkHeader || ""
                                    color: "#38BDF8"
                                    font.family: "monospace"
                                    font.pixelSize: 10
                                    font.bold: true
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
                                                text: modelData.leftLine || ""
                                                color: "#64748B"
                                                font.family: "monospace"
                                                font.pixelSize: 10
                                                horizontalAlignment: Text.AlignRight
                                                verticalAlignment: Text.AlignVCenter
                                            }
                                        }

                                        Item {
                                            Layout.fillWidth: true
                                            Layout.fillHeight: true
                                            clip: true

                                            Text {
                                                x: 6 - root.horizontalScrollX
                                                width: Math.max(implicitWidth, Math.max(parent.width - 6, root.maxCodeWidth))
                                                height: parent.height
                                                text: modelData.leftRichContent || modelData.leftContent || ""
                                                textFormat: Text.RichText
                                                color: modelData.leftFgColor || "#CBD5E1"
                                                font.family: "monospace"
                                                font.pixelSize: 11
                                                verticalAlignment: Text.AlignVCenter
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
                                                text: modelData.rightLine || ""
                                                color: "#64748B"
                                                font.family: "monospace"
                                                font.pixelSize: 10
                                                horizontalAlignment: Text.AlignRight
                                                verticalAlignment: Text.AlignVCenter
                                            }
                                        }

                                        Item {
                                            Layout.fillWidth: true
                                            Layout.fillHeight: true
                                            clip: true

                                            Text {
                                                x: 6 - root.horizontalScrollX
                                                width: Math.max(implicitWidth, Math.max(parent.width - 6, root.maxCodeWidth))
                                                height: parent.height
                                                text: modelData.rightRichContent || modelData.rightContent || ""
                                                textFormat: Text.RichText
                                                color: modelData.rightFgColor || "#CBD5E1"
                                                font.family: "monospace"
                                                font.pixelSize: 11
                                                verticalAlignment: Text.AlignVCenter
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
                        contentWidth: root.maxCodeWidth
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
                anchors.fill: parent
                visible: root.diffViewer && !root.diffViewer.isEmpty && !root.diffViewer.isBinary && root.diffViewer.viewMode === "hunk"
                clip: true
                ScrollBar.vertical: FluxScrollBar {}
                ScrollBar.horizontal: FluxScrollBar {}

                ListView {
                    id: hunkListView
                    width: Math.max(parent.width, root.maxCodeWidth + 120)
                    model: root.diffViewer ? root.diffViewer.hunks : null
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
                                Layout.preferredHeight: 28
                                color: "#0D1117"
                                radius: 4
                                border.color: "#21262D"

                                RowLayout {
                                    anchors.fill: parent
                                    anchors.margins: 6
                                    spacing: 8
                                    Text {
                                        text: "🔍"
                                        font.pixelSize: 10
                                    }
                                    Text {
                                        text: modelData.header || ""
                                        color: "#38BDF8"
                                        font.family: "monospace"
                                        font.pixelSize: 11
                                        font.bold: true
                                    }
                                }
                            }

                            // Hunk Lines
                            Repeater {
                                model: modelData.lines || []

                                delegate: Rectangle {
                                    Layout.fillWidth: true
                                    Layout.preferredHeight: 20
                                    color: modelData.bgColor || "transparent"

                                    RowLayout {
                                        anchors.fill: parent
                                        spacing: 6

                                        Text {
                                            Layout.preferredWidth: 36
                                            text: modelData.oldLineNumber || ""
                                            color: "#64748B"
                                            font.family: "monospace"
                                            font.pixelSize: 10
                                            horizontalAlignment: Text.AlignRight
                                        }

                                        Text {
                                            Layout.preferredWidth: 36
                                            text: modelData.newLineNumber || ""
                                            color: "#64748B"
                                            font.family: "monospace"
                                            font.pixelSize: 10
                                            horizontalAlignment: Text.AlignRight
                                        }

                                        Text {
                                            text: {
                                                if (modelData.kind === "addition") return "+";
                                                if (modelData.kind === "deletion") return "-";
                                                return " ";
                                            }
                                            color: modelData.fgColor || "#CBD5E1"
                                            font.family: "monospace"
                                            font.pixelSize: 11
                                            font.bold: true
                                        }

                                        Text {
                                            Layout.fillWidth: true
                                            text: modelData.richContent || modelData.content || ""
                                            textFormat: Text.RichText
                                            color: modelData.fgColor || "#CBD5E1"
                                            font.family: "monospace"
                                            font.pixelSize: 11
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
                visible: root.diffViewer && !root.diffViewer.isEmpty && !root.diffViewer.isBinary && root.diffViewer.viewMode === "blame"
                clip: true

                ListView {
                    id: blameListView
                    anchors.fill: parent
                    anchors.bottomMargin: blameHScrollBar.visible ? 12 : 0
                    focus: true
                    model: root.diffViewer ? root.diffViewer.blameLines : null
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
                                        width: 16
                                        height: 16
                                        radius: 8
                                        color: modelData.color || "#6366F1"
                                        clip: true

                                        Image {
                                            anchors.fill: parent
                                            source: modelData.avatarUrl || ""
                                            fillMode: Image.PreserveAspectCrop
                                            smooth: true
                                        }

                                        Text {
                                            anchors.centerIn: parent
                                            text: modelData.initials || "??"
                                            color: "#FFFFFF"
                                            font.pixelSize: 8
                                            font.bold: true
                                            visible: parent.children[0].status !== Image.Ready
                                        }
                                    }

                                    // Short SHA
                                    Text {
                                        text: modelData.shortSha || ""
                                        color: "#38BDF8"
                                        font.family: "monospace"
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
                                        font.family: "monospace"
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
                                    width: Math.max(implicitWidth, Math.max(parent.width - 6, root.maxCodeWidth))
                                    height: parent.height
                                    text: modelData.richContent || modelData.content || ""
                                    textFormat: Text.RichText
                                    color: "#F1F5F9"
                                    font.family: "monospace"
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
