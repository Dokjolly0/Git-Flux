import QtQuick
import QtQuick.Layouts
import "../themes"

ColumnLayout {
    id: splitRoot
    readonly property FluxTheme theme: FluxTheme {}

    property var panel: null
    readonly property var diffViewer: panel ? panel.diffViewer : null
    readonly property alias listView: splitListView
    readonly property alias overviewWidth: splitOverviewBar.width

    spacing: 0

    // Split Header (Original vs Modified)
    Rectangle {
        Layout.fillWidth: true
        Layout.preferredHeight: 24
        color: splitRoot.theme.surface
        border.color: splitRoot.theme.borderSubtle
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
                    color: splitRoot.theme.danger
                    font.pixelSize: 11
                    font.bold: true
                }
            }

            Rectangle {
                width: 1
                Layout.fillHeight: true
                color: splitRoot.theme.borderHighlight
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.fillHeight: true
                color: "transparent"
                Text {
                    anchors.centerIn: parent
                    text: "Modificato (Head)"
                    color: splitRoot.theme.success
                    font.pixelSize: 11
                    font.bold: true
                }
            }

            Rectangle {
                Layout.preferredWidth: splitOverviewBar.width
                Layout.fillHeight: true
                color: splitRoot.theme.inputBg

                Rectangle {
                    anchors.left: parent.left
                    anchors.top: parent.top
                    anchors.bottom: parent.bottom
                    width: 1
                    color: splitRoot.theme.borderSubtle
                }

                Rectangle {
                    x: Math.floor(parent.width / 2)
                    anchors.top: parent.top
                    anchors.bottom: parent.bottom
                    width: 1
                    color: splitRoot.theme.borderSubtle
                }

                Text {
                    x: 1
                    width: Math.floor(parent.width / 2) - 1
                    anchors.verticalCenter: parent.verticalCenter
                    text: "-"
                    color: splitRoot.theme.danger
                    font.pixelSize: 10
                    font.bold: true
                    horizontalAlignment: Text.AlignHCenter
                }

                Text {
                    x: Math.floor(parent.width / 2) + 1
                    width: parent.width - x
                    anchors.verticalCenter: parent.verticalCenter
                    text: "+"
                    color: splitRoot.theme.success
                    font.pixelSize: 10
                    font.bold: true
                    horizontalAlignment: Text.AlignHCenter
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
            SmoothWheelScroller {}
            anchors.left: parent.left
            anchors.right: splitOverviewBar.left
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            anchors.bottomMargin: splitHScrollBar.visible ? 12 : 0
            focus: true
            model: splitRoot.visible && splitRoot.diffViewer ? splitRoot.diffViewer.splitLines : null
            boundsBehavior: Flickable.StopAtBounds
            maximumFlickVelocity: 6000
            flickDeceleration: 1000

            Keys.onPressed: (event) => {
                if (splitRoot.panel) splitRoot.panel.handleDiffKeys(event);
            }
            Keys.onLeftPressed: (event) => {
                if (splitRoot.panel && splitRoot.panel.splitMaxScroll > 0) {
                    splitRoot.panel.horizontalScrollX = Math.max(0, splitRoot.panel.horizontalScrollX - 40);
                    event.accepted = true;
                }
            }
            Keys.onRightPressed: (event) => {
                if (splitRoot.panel && splitRoot.panel.splitMaxScroll > 0) {
                    splitRoot.panel.horizontalScrollX = Math.min(splitRoot.panel.splitMaxScroll, splitRoot.panel.horizontalScrollX + 40);
                    event.accepted = true;
                }
            }

            delegate: Rectangle {
                id: splitRow
                width: splitListView.width
                readonly property bool isHeaderRow: modelData.isHunkHeader === true
                readonly property real leftTextAreaWidth: Math.max(40, Math.floor(splitRow.width / 2) - 40)
                readonly property real rightTextAreaWidth: Math.max(40, splitRow.width - Math.floor(splitRow.width / 2) - 41)
                readonly property alias leftCodeEdit: leftCodeText
                readonly property alias rightCodeEdit: rightCodeText
                readonly property bool wrapEnabled: Boolean(splitRoot.panel && splitRoot.panel.effectiveWrap)
                height: isHeaderRow
                    ? 28
                    : (wrapEnabled
                        ? Math.max(20, leftCodeText.implicitHeight + 6, rightCodeText.implicitHeight + 6)
                        : 20)
                color: splitRoot.theme.canvas

                Rectangle {
                    anchors.fill: parent
                    visible: splitRow.isHeaderRow
                    color: splitRoot.theme.cardHover
                    border.color: splitRoot.theme.borderHighlight
                    border.width: 1

                    Text {
                        anchors.left: parent.left
                        anchors.leftMargin: 12
                        anchors.right: splitExpandActions.left
                        anchors.rightMargin: 8
                        anchors.verticalCenter: parent.verticalCenter
                        text: modelData.hunkHeader || ""
                        color: splitRoot.theme.borderFocus
                        font.family: splitRoot.panel ? splitRoot.panel.monoFont : "Consolas"
                        font.pixelSize: 10
                        font.bold: true
                        elide: Text.ElideRight
                    }

                    DiffHunkExpandActions {
                        id: splitExpandActions
                        diffViewer: splitRoot.diffViewer
                        anchors.right: parent.right
                        anchors.rightMargin: 16
                        anchors.verticalCenter: parent.verticalCenter
                    }
                }

                RowLayout {
                    anchors.fill: parent
                    spacing: 0
                    visible: !splitRow.isHeaderRow

                    // LEFT COLUMN
                    Rectangle {
                        id: leftColumn
                        Layout.preferredWidth: Math.floor(splitRow.width / 2)
                        Layout.fillHeight: true
                        color: modelData.leftBgColor || "transparent"
                        readonly property bool isEmptySide: modelData.leftKind === "empty"

                        RowLayout {
                            anchors.fill: parent
                            spacing: 0

                            Rectangle {
                                Layout.preferredWidth: 40
                                Layout.fillHeight: true
                                color: leftColumn.isEmptySide ? "transparent" : splitRoot.theme.inputBg
                                Text {
                                    anchors.fill: parent
                                    anchors.rightMargin: 6
                                    anchors.topMargin: 2
                                    text: modelData.leftLine || ""
                                    color: splitRoot.theme.textMuted
                                    font.family: splitRoot.panel ? splitRoot.panel.monoFont : "Consolas"
                                    font.pixelSize: 10
                                    horizontalAlignment: Text.AlignRight
                                    verticalAlignment: splitRow.wrapEnabled ? Text.AlignTop : Text.AlignVCenter
                                }
                                Rectangle {
                                    anchors.right: parent.right
                                    anchors.top: parent.top
                                    anchors.bottom: parent.bottom
                                    width: 1
                                    visible: !leftColumn.isEmptySide
                                    color: splitRoot.panel ? splitRoot.panel.gutterBorderColor : splitRoot.theme.borderSubtle
                                }
                            }

                            Item {
                                Layout.fillWidth: true
                                Layout.fillHeight: true
                                clip: true

                                DiffCodeText {
                                    id: leftCodeText
                                    panel: splitRoot.panel
                                    selTargetName: "split-left"
                                    rowIndex: index
                                    rowDisabled: splitRow.isHeaderRow || modelData.leftKind === "empty"
                                    x: splitRow.wrapEnabled ? 6 : (6 - (splitRoot.panel ? splitRoot.panel.horizontalScrollX : 0))
                                    anchors.verticalCenter: parent.verticalCenter
                                    width: splitRow.wrapEnabled
                                        ? Math.max(40, splitRow.leftTextAreaWidth - 12)
                                        : Math.max(splitRow.leftTextAreaWidth - 6, splitRoot.panel ? splitRoot.panel.maxCodeWidth : 0)
                                    height: implicitHeight
                                    text: modelData.leftRichContent || modelData.leftContent || ""
                                    color: modelData.leftFgColor || splitRoot.theme.textPrimary
                                    font.pixelSize: 11
                                    wrapMode: splitRow.wrapEnabled ? TextEdit.WrapAtWordBoundaryOrAnywhere : TextEdit.NoWrap
                                }
                            }
                        }

                        Rectangle {
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.bottom: parent.bottom
                            height: 1
                            visible: !parent.isEmptySide
                            color: splitRoot.panel ? splitRoot.panel.gridLineColor : Qt.rgba(0.58, 0.64, 0.72, 0.14)
                            z: 4
                        }
                    }

                    Rectangle {
                        width: 1
                        Layout.fillHeight: true
                        color: splitRoot.theme.borderSubtle
                    }

                    // RIGHT COLUMN
                    Rectangle {
                        id: rightColumn
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        color: modelData.rightBgColor || "transparent"
                        readonly property bool isEmptySide: modelData.rightKind === "empty"

                        RowLayout {
                            anchors.fill: parent
                            spacing: 0

                            Rectangle {
                                Layout.preferredWidth: 40
                                Layout.fillHeight: true
                                color: rightColumn.isEmptySide ? "transparent" : splitRoot.theme.surface
                                Text {
                                    anchors.fill: parent
                                    anchors.rightMargin: 6
                                    anchors.topMargin: 2
                                    text: modelData.rightLine || ""
                                    color: splitRoot.theme.textMuted
                                    font.family: splitRoot.panel ? splitRoot.panel.monoFont : "Consolas"
                                    font.pixelSize: 10
                                    horizontalAlignment: Text.AlignRight
                                    verticalAlignment: splitRow.wrapEnabled ? Text.AlignTop : Text.AlignVCenter
                                }
                                Rectangle {
                                    anchors.right: parent.right
                                    anchors.top: parent.top
                                    anchors.bottom: parent.bottom
                                    width: 1
                                    visible: !rightColumn.isEmptySide
                                    color: splitRoot.panel ? splitRoot.panel.gutterBorderColor : splitRoot.theme.borderSubtle
                                }
                            }

                            Item {
                                Layout.fillWidth: true
                                Layout.fillHeight: true
                                clip: true

                                DiffCodeText {
                                    id: rightCodeText
                                    panel: splitRoot.panel
                                    selTargetName: "split-right"
                                    rowIndex: index
                                    rowDisabled: splitRow.isHeaderRow || modelData.rightKind === "empty"
                                    x: splitRow.wrapEnabled ? 6 : (6 - (splitRoot.panel ? splitRoot.panel.horizontalScrollX : 0))
                                    anchors.verticalCenter: parent.verticalCenter
                                    width: splitRow.wrapEnabled
                                        ? Math.max(40, splitRow.rightTextAreaWidth - 12)
                                        : Math.max(splitRow.rightTextAreaWidth - 6, splitRoot.panel ? splitRoot.panel.maxCodeWidth : 0)
                                    height: implicitHeight
                                    text: modelData.rightRichContent || modelData.rightContent || ""
                                    color: modelData.rightFgColor || splitRoot.theme.textPrimary
                                    font.pixelSize: 11
                                    wrapMode: splitRow.wrapEnabled ? TextEdit.WrapAtWordBoundaryOrAnywhere : TextEdit.NoWrap
                                }
                            }
                        }

                        Rectangle {
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.bottom: parent.bottom
                            height: 1
                            visible: !parent.isEmptySide
                            color: splitRoot.panel ? splitRoot.panel.gridLineColor : Qt.rgba(0.58, 0.64, 0.72, 0.14)
                            z: 4
                        }
                    }
                }
            }

            DiffHorizontalWheelHandler {
                maxScroll: splitRoot.panel ? splitRoot.panel.splitMaxScroll : 0
                scrollX: splitRoot.panel ? splitRoot.panel.horizontalScrollX : 0
                onScrolledTo: (nextX) => {
                    if (splitRoot.panel) splitRoot.panel.horizontalScrollX = nextX;
                }
            }
        }

        DiffListSelectionMouseArea {
            anchors.fill: splitListView
            panel: splitRoot.panel
            listView: splitListView
            viewType: "split"
            z: 5
        }

        DiffHScrollBar {
            id: splitHScrollBar
            anchors.left: parent.left
            anchors.leftMargin: 40
            anchors.right: splitOverviewBar.left
            anchors.rightMargin: 8
            anchors.bottom: parent.bottom
            contentWidth: (splitRoot.panel && splitRoot.panel.effectiveWrap) ? 0 : (splitRoot.panel ? splitRoot.panel.maxCodeWidth : 0)
            viewWidth: splitRoot.panel ? splitRoot.panel.splitAvailWidth : 100
            scrollValue: splitRoot.panel ? splitRoot.panel.horizontalScrollX : 0
            onScrollRequested: (val) => {
                if (splitRoot.panel) splitRoot.panel.horizontalScrollX = val;
            }
        }

        DiffOverviewScrollBar {
            id: splitOverviewBar
            anchors.top: parent.top
            anchors.bottom: splitListView.bottom
            anchors.right: parent.right
            flickable: splitListView
            markers: splitRoot.diffViewer ? splitRoot.diffViewer.splitOverviewMarkers : []
        }
    }
}
