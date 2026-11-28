import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../themes"

Item {
    id: blameRoot
    readonly property FluxTheme theme: FluxTheme {}

    property var panel: null
    readonly property var diffViewer: panel ? panel.diffViewer : null
    readonly property alias listView: blameListView

    clip: true

    ListView {
        id: blameListView
        SmoothWheelScroller {}
        anchors.fill: parent
        anchors.bottomMargin: blameHScrollBar.visible ? 12 : 0
        focus: true
        model: blameRoot.visible && blameRoot.diffViewer ? blameRoot.diffViewer.blameLines : null
        boundsBehavior: Flickable.StopAtBounds
        maximumFlickVelocity: 6000
        flickDeceleration: 1000
        ScrollBar.vertical: FluxScrollBar {}

        Keys.onPressed: (event) => {
            if (blameRoot.panel) blameRoot.panel.handleDiffKeys(event);
        }
        Keys.onLeftPressed: (event) => {
            if (blameRoot.panel && blameRoot.panel.blameMaxScroll > 0) {
                blameRoot.panel.horizontalScrollX = Math.max(0, blameRoot.panel.horizontalScrollX - 40);
                event.accepted = true;
            }
        }
        Keys.onRightPressed: (event) => {
            if (blameRoot.panel && blameRoot.panel.blameMaxScroll > 0) {
                blameRoot.panel.horizontalScrollX = Math.min(blameRoot.panel.blameMaxScroll, blameRoot.panel.horizontalScrollX + 40);
                event.accepted = true;
            }
        }

        delegate: Rectangle {
            readonly property bool isHeaderRow: false
            readonly property alias codeEdit: blameCodeText
            width: blameListView.width
            height: 22
            color: index % 2 === 0 ? blameRoot.theme.canvas : blameRoot.theme.surface

            RowLayout {
                anchors.fill: parent
                spacing: 8

                Rectangle {
                    Layout.preferredWidth: 310
                    Layout.fillHeight: true
                    color: blameRoot.theme.inputBg
                    border.color: blameRoot.theme.borderSubtle
                    border.width: 1

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 6
                        anchors.rightMargin: 6
                        spacing: 6

                        FluxAvatarBadge {
                            Layout.preferredWidth: 16
                            Layout.preferredHeight: 16
                            avatarUrl: modelData.avatarUrl || ""
                            initials: modelData.initials || "??"
                            avatarColor: modelData.color || blameRoot.theme.accentPurple
                            maskColor: blameRoot.theme.inputBg
                            initialsFontSize: 8
                        }

                        Text {
                            text: modelData.shortSha || ""
                            color: blameRoot.theme.borderFocus
                            font.family: blameRoot.panel ? blameRoot.panel.monoFont : "Consolas"
                            font.pixelSize: 10
                            Layout.preferredWidth: 54
                        }

                        Text {
                            text: modelData.author || ""
                            color: blameRoot.theme.textPrimary
                            font.pixelSize: 10
                            elide: Text.ElideRight
                            Layout.fillWidth: true
                        }

                        Text {
                            text: modelData.date || ""
                            color: blameRoot.theme.textMuted
                            font.pixelSize: 10
                        }

                        Text {
                            text: modelData.lineNumber || ""
                            color: blameRoot.theme.textSecondary
                            font.family: blameRoot.panel ? blameRoot.panel.monoFont : "Consolas"
                            font.pixelSize: 10
                            Layout.preferredWidth: 28
                            horizontalAlignment: Text.AlignRight
                        }
                    }
                }

                Item {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true

                    DiffCodeText {
                        id: blameCodeText
                        panel: blameRoot.panel
                        selTargetName: "blame"
                        rowIndex: index
                        x: 6 - (blameRoot.panel ? blameRoot.panel.horizontalScrollX : 0)
                        width: Math.max(parent.width - 6, blameRoot.panel ? blameRoot.panel.maxCodeWidth : 0)
                        height: implicitHeight
                        anchors.verticalCenter: parent.verticalCenter
                        text: modelData.richContent || modelData.content || ""
                        color: blameRoot.theme.textPrimary
                        font.pixelSize: 11
                        wrapMode: TextEdit.NoWrap
                    }
                }
            }

            Rectangle {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                height: 1
                color: blameRoot.panel ? blameRoot.panel.gridLineColor : Qt.rgba(0.58, 0.64, 0.72, 0.14)
                z: 4
            }
        }

        DiffHorizontalWheelHandler {
            maxScroll: blameRoot.panel ? blameRoot.panel.blameMaxScroll : 0
            scrollX: blameRoot.panel ? blameRoot.panel.horizontalScrollX : 0
            onScrolledTo: (nextX) => {
                if (blameRoot.panel) blameRoot.panel.horizontalScrollX = nextX;
            }
        }
    }

    DiffListSelectionMouseArea {
        anchors.fill: blameListView
        panel: blameRoot.panel
        listView: blameListView
        viewType: "blame"
        z: 5
    }

    DiffHScrollBar {
        id: blameHScrollBar
        anchors.left: parent.left
        anchors.leftMargin: 318
        anchors.right: parent.right
        anchors.rightMargin: 10
        anchors.bottom: parent.bottom
        contentWidth: blameRoot.panel ? blameRoot.panel.maxCodeWidth : 0
        viewWidth: blameRoot.panel ? blameRoot.panel.blameAvailWidth : 100
        scrollValue: blameRoot.panel ? blameRoot.panel.horizontalScrollX : 0
        onScrollRequested: (val) => {
            if (blameRoot.panel) blameRoot.panel.horizontalScrollX = val;
        }
    }
}
