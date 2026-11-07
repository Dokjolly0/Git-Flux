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

                Item { Layout.fillWidth: true }

                // View Mode Switcher Buttons (Inline, Split, Hunk, Blame)
                RowLayout {
                    visible: root.diffViewer && !root.diffViewer.isEmpty && !root.diffViewer.isBinary
                    spacing: 4

                    // 1. Inline View
                    Button {
                        Layout.preferredHeight: 26
                        checkable: true
                        checked: root.diffViewer ? root.diffViewer.viewMode === "inline" : true
                        background: Rectangle {
                            color: parent.checked ? "#1E293B" : (parent.hovered ? "#161B22" : "transparent")
                            radius: 4
                            border.color: parent.checked ? "#38BDF8" : "#334155"
                            border.width: 1
                        }
                        contentItem: RowLayout {
                            spacing: 4
                            Text { text: "📄"; font.pixelSize: 10 }
                            Text {
                                text: "Inline"
                                color: parent.parent.checked ? "#38BDF8" : "#94A3B8"
                                font.pixelSize: 11
                                font.bold: parent.parent.checked
                            }
                        }
                        onClicked: {
                            if (root.diffViewer) root.diffViewer.setViewMode("inline");
                        }
                    }

                    // 2. Split View (Side-by-side)
                    Button {
                        Layout.preferredHeight: 26
                        checkable: true
                        checked: root.diffViewer ? root.diffViewer.viewMode === "split" : false
                        background: Rectangle {
                            color: parent.checked ? "#1E293B" : (parent.hovered ? "#161B22" : "transparent")
                            radius: 4
                            border.color: parent.checked ? "#38BDF8" : "#334155"
                            border.width: 1
                        }
                        contentItem: RowLayout {
                            spacing: 4
                            Text { text: "📑"; font.pixelSize: 10 }
                            Text {
                                text: "Split"
                                color: parent.parent.checked ? "#38BDF8" : "#94A3B8"
                                font.pixelSize: 11
                                font.bold: parent.parent.checked
                            }
                        }
                        onClicked: {
                            if (root.diffViewer) root.diffViewer.setViewMode("split");
                        }
                    }

                    // 3. Hunk View
                    Button {
                        Layout.preferredHeight: 26
                        checkable: true
                        checked: root.diffViewer ? root.diffViewer.viewMode === "hunk" : false
                        background: Rectangle {
                            color: parent.checked ? "#1E293B" : (parent.hovered ? "#161B22" : "transparent")
                            radius: 4
                            border.color: parent.checked ? "#38BDF8" : "#334155"
                            border.width: 1
                        }
                        contentItem: RowLayout {
                            spacing: 4
                            Text { text: "🔍"; font.pixelSize: 10 }
                            Text {
                                text: "Hunk"
                                color: parent.parent.checked ? "#38BDF8" : "#94A3B8"
                                font.pixelSize: 11
                                font.bold: parent.parent.checked
                            }
                        }
                        onClicked: {
                            if (root.diffViewer) root.diffViewer.setViewMode("hunk");
                        }
                    }

                    // 4. Blame View
                    Button {
                        Layout.preferredHeight: 26
                        checkable: true
                        checked: root.diffViewer ? root.diffViewer.viewMode === "blame" : false
                        background: Rectangle {
                            color: parent.checked ? "#1E293B" : (parent.hovered ? "#161B22" : "transparent")
                            radius: 4
                            border.color: parent.checked ? "#38BDF8" : "#334155"
                            border.width: 1
                        }
                        contentItem: RowLayout {
                            spacing: 4
                            Text { text: "👤"; font.pixelSize: 10 }
                            Text {
                                text: "Blame"
                                color: parent.parent.checked ? "#38BDF8" : "#94A3B8"
                                font.pixelSize: 11
                                font.bold: parent.parent.checked
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
            ScrollView {
                anchors.fill: parent
                visible: root.diffViewer && !root.diffViewer.isEmpty && !root.diffViewer.isBinary && (root.diffViewer.viewMode === "inline" || root.diffViewer.viewMode === "")
                clip: true

                ListView {
                    id: inlineListView
                    width: parent.width
                    model: root.diffViewer ? root.diffViewer.linesModel : null
                    boundsBehavior: Flickable.StopAtBounds

                    delegate: Rectangle {
                        width: inlineListView.width
                        height: 20
                        color: model.bgColor

                        RowLayout {
                            anchors.fill: parent
                            spacing: 0

                            // Old Line Number
                            Rectangle {
                                Layout.preferredWidth: 44
                                Layout.fillHeight: true
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
                                Layout.preferredWidth: 44
                                Layout.fillHeight: true
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
                                Layout.preferredWidth: 20
                                Layout.fillHeight: true
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

                            // Content Line
                            Text {
                                Layout.fillWidth: true
                                Layout.fillHeight: true
                                text: model.content || ""
                                color: model.fgColor
                                font.family: "monospace"
                                font.pixelSize: 12
                                verticalAlignment: Text.AlignVCenter
                                elide: Text.ElideNone
                            }
                        }
                    }
                }
            }

            // ------------------------------------------
            // 2. SPLIT VIEW (Side-by-Side Diff)
            // ------------------------------------------
            ColumnLayout {
                anchors.fill: parent
                visible: root.diffViewer && !root.diffViewer.isEmpty && !root.diffViewer.isBinary && root.diffViewer.viewMode === "split"
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

                ScrollView {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true

                    ListView {
                        id: splitListView
                        width: parent.width
                        model: root.diffViewer ? root.diffViewer.splitLines : null
                        boundsBehavior: Flickable.StopAtBounds

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
                                    Layout.preferredWidth: splitRow.width / 2
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

                                        Text {
                                            Layout.fillWidth: true
                                            Layout.fillHeight: true
                                            anchors.leftMargin: 6
                                            text: modelData.leftContent || ""
                                            color: modelData.leftFgColor || "#CBD5E1"
                                            font.family: "monospace"
                                            font.pixelSize: 11
                                            verticalAlignment: Text.AlignVCenter
                                            elide: Text.ElideNone
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

                                        Text {
                                            Layout.fillWidth: true
                                            Layout.fillHeight: true
                                            anchors.leftMargin: 6
                                            text: modelData.rightContent || ""
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
                }
            }

            // ------------------------------------------
            // 3. HUNK VIEW (Cards per Hunk)
            // ------------------------------------------
            ScrollView {
                anchors.fill: parent
                visible: root.diffViewer && !root.diffViewer.isEmpty && !root.diffViewer.isBinary && root.diffViewer.viewMode === "hunk"
                clip: true

                ListView {
                    id: hunkListView
                    width: parent.width
                    model: root.diffViewer ? root.diffViewer.hunks : null
                    spacing: 12
                    boundsBehavior: Flickable.StopAtBounds

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
                                            text: modelData.content || ""
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
            ScrollView {
                anchors.fill: parent
                visible: root.diffViewer && !root.diffViewer.isEmpty && !root.diffViewer.isBinary && root.diffViewer.viewMode === "blame"
                clip: true

                ListView {
                    id: blameListView
                    width: parent.width
                    model: root.diffViewer ? root.diffViewer.blameLines : null
                    boundsBehavior: Flickable.StopAtBounds

                    delegate: Rectangle {
                        width: blameListView.width
                        height: 22
                        color: index % 2 === 0 ? "#0D1117" : "#111827"

                        RowLayout {
                            anchors.fill: parent
                            spacing: 8

                            // Blame Annotation Column
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

                            // Code Line Content
                            Text {
                                Layout.fillWidth: true
                                Layout.fillHeight: true
                                text: modelData.content || ""
                                color: "#F1F5F9"
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
    }
}
