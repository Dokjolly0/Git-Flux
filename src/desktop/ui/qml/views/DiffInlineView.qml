import QtQuick
import "../themes"

Item {
    id: inlineRoot
    readonly property FluxTheme theme: FluxTheme {}

    property var panel: null
    readonly property var diffViewer: panel ? panel.diffViewer : null
    readonly property alias listView: inlineListView
    readonly property alias overviewWidth: inlineOverviewBar.width

    clip: true

    ListView {
        id: inlineListView
        SmoothWheelScroller {}
        anchors.left: parent.left
        anchors.right: inlineOverviewBar.left
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.bottomMargin: inlineHScrollBar.visible ? 12 : 0
        focus: true
        model: inlineRoot.visible && inlineRoot.diffViewer ? inlineRoot.diffViewer.linesModel : null
        boundsBehavior: Flickable.StopAtBounds
        maximumFlickVelocity: 6000
        flickDeceleration: 1000

        Keys.onPressed: (event) => {
            if (inlineRoot.panel) inlineRoot.panel.handleDiffKeys(event);
        }
        Keys.onLeftPressed: (event) => {
            if (inlineRoot.panel && inlineRoot.panel.inlineMaxScroll > 0) {
                inlineRoot.panel.horizontalScrollX = Math.max(0, inlineRoot.panel.horizontalScrollX - 40);
                event.accepted = true;
            }
        }
        Keys.onRightPressed: (event) => {
            if (inlineRoot.panel && inlineRoot.panel.inlineMaxScroll > 0) {
                inlineRoot.panel.horizontalScrollX = Math.min(inlineRoot.panel.inlineMaxScroll, inlineRoot.panel.horizontalScrollX + 40);
                event.accepted = true;
            }
        }

        delegate: Rectangle {
            id: inlineRowDelegate
            width: inlineListView.width
            readonly property bool isHeaderRow: model.lineKind === "header"
            readonly property real textAreaWidth: Math.max(60, inlineRowDelegate.width - 108)
            readonly property alias codeEdit: inlineCodeText
            readonly property bool wrapEnabled: Boolean(inlineRoot.panel && inlineRoot.panel.effectiveWrap)
            height: isHeaderRow
                ? 28
                : (wrapEnabled ? Math.max(20, inlineCodeText.implicitHeight + 6) : 20)
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

                    Rectangle {
                        width: 44
                        height: parent.height
                        color: inlineRoot.theme.inputBg
                        Text {
                            anchors.fill: parent
                            anchors.rightMargin: 6
                            anchors.topMargin: 2
                            text: model.oldLineNumber || ""
                            color: inlineRoot.theme.textMuted
                            font.family: inlineRoot.panel ? inlineRoot.panel.monoFont : "Consolas"
                            font.pixelSize: 11
                            horizontalAlignment: Text.AlignRight
                            verticalAlignment: inlineRowDelegate.wrapEnabled ? Text.AlignTop : Text.AlignVCenter
                        }
                        Rectangle {
                            anchors.right: parent.right
                            anchors.top: parent.top
                            anchors.bottom: parent.bottom
                            width: 1
                            color: inlineRoot.panel ? inlineRoot.panel.gutterBorderColor : inlineRoot.theme.borderSubtle
                        }
                    }

                    Rectangle {
                        width: 44
                        height: parent.height
                        color: inlineRoot.theme.surface
                        Text {
                            anchors.fill: parent
                            anchors.rightMargin: 6
                            anchors.topMargin: 2
                            text: model.newLineNumber || ""
                            color: inlineRoot.theme.textMuted
                            font.family: inlineRoot.panel ? inlineRoot.panel.monoFont : "Consolas"
                            font.pixelSize: 11
                            horizontalAlignment: Text.AlignRight
                            verticalAlignment: inlineRowDelegate.wrapEnabled ? Text.AlignTop : Text.AlignVCenter
                        }
                        Rectangle {
                            anchors.right: parent.right
                            anchors.top: parent.top
                            anchors.bottom: parent.bottom
                            width: 1
                            color: inlineRoot.panel ? inlineRoot.panel.gutterBorderColor : inlineRoot.theme.borderSubtle
                        }
                    }

                    Rectangle {
                        width: 20
                        height: parent.height
                        color: "transparent"
                        Text {
                            anchors.horizontalCenter: parent.horizontalCenter
                            anchors.top: parent.top
                            anchors.topMargin: inlineRowDelegate.wrapEnabled ? 2 : Math.floor((parent.height - implicitHeight) / 2)
                            text: {
                                if (model.lineKind === "addition") return "+";
                                if (model.lineKind === "deletion") return "-";
                                return " ";
                            }
                            color: model.fgColor
                            font.family: inlineRoot.panel ? inlineRoot.panel.monoFont : "Consolas"
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

                    DiffCodeText {
                        id: inlineCodeText
                        panel: inlineRoot.panel
                        selTargetName: "inline"
                        rowIndex: index
                        rowDisabled: inlineRowDelegate.isHeaderRow
                        x: inlineRowDelegate.wrapEnabled ? 6 : (6 - (inlineRoot.panel ? inlineRoot.panel.horizontalScrollX : 0))
                        anchors.verticalCenter: parent.verticalCenter
                        width: (inlineRowDelegate.wrapEnabled && !inlineRowDelegate.isHeaderRow)
                            ? Math.max(60, inlineRowDelegate.textAreaWidth - 14)
                            : Math.max(inlineRowDelegate.textAreaWidth - 6, inlineRoot.panel ? inlineRoot.panel.maxCodeWidth : 0)
                        height: implicitHeight
                        text: model.richContent || model.content || ""
                        color: model.fgColor
                        font.pixelSize: 12
                        wrapMode: (inlineRowDelegate.wrapEnabled && !inlineRowDelegate.isHeaderRow) ? TextEdit.WrapAtWordBoundaryOrAnywhere : TextEdit.NoWrap
                    }

                    DiffHunkExpandActions {
                        visible: inlineRowDelegate.isHeaderRow && inlineRoot.diffViewer && !inlineRoot.diffViewer.isFullContext
                        diffViewer: inlineRoot.diffViewer
                        anchors.right: parent.right
                        anchors.rightMargin: 16
                        anchors.verticalCenter: parent.verticalCenter
                        z: 5
                    }
                }
            }

            Rectangle {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                height: 1
                color: inlineRoot.panel ? inlineRoot.panel.gridLineColor : Qt.rgba(0.58, 0.64, 0.72, 0.14)
                z: 4
            }
        }

        DiffHorizontalWheelHandler {
            maxScroll: inlineRoot.panel ? inlineRoot.panel.inlineMaxScroll : 0
            scrollX: inlineRoot.panel ? inlineRoot.panel.horizontalScrollX : 0
            onScrolledTo: (nextX) => {
                if (inlineRoot.panel) inlineRoot.panel.horizontalScrollX = nextX;
            }
        }
    }

    DiffListSelectionMouseArea {
        anchors.fill: inlineListView
        panel: inlineRoot.panel
        listView: inlineListView
        viewType: "inline"
        z: 5
    }

    DiffHScrollBar {
        id: inlineHScrollBar
        anchors.left: parent.left
        anchors.leftMargin: 108
        anchors.right: inlineOverviewBar.left
        anchors.rightMargin: 8
        anchors.bottom: parent.bottom
        contentWidth: (inlineRoot.panel && inlineRoot.panel.effectiveWrap) ? 0 : (inlineRoot.panel ? inlineRoot.panel.maxCodeWidth : 0)
        viewWidth: inlineRoot.panel ? inlineRoot.panel.inlineAvailWidth : 100
        scrollValue: inlineRoot.panel ? inlineRoot.panel.horizontalScrollX : 0
        onScrollRequested: (val) => {
            if (inlineRoot.panel) inlineRoot.panel.horizontalScrollX = val;
        }
    }

    DiffOverviewScrollBar {
        id: inlineOverviewBar
        anchors.top: parent.top
        anchors.bottom: inlineListView.bottom
        anchors.right: parent.right
        flickable: inlineListView
        markers: inlineRoot.diffViewer ? inlineRoot.diffViewer.inlineOverviewMarkers : []
    }
}
