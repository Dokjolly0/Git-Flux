import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../themes"

Item {
    id: hunkRoot
    readonly property FluxTheme theme: FluxTheme {}

    property var panel: null
    readonly property var diffViewer: panel ? panel.diffViewer : null
    readonly property alias listView: hunkListView

    clip: true

    ScrollView {
        id: hunkScrollView
        anchors.left: parent.left
        anchors.right: hunkOverviewBar.left
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        clip: true
        ScrollBar.vertical: ScrollBar {
            policy: ScrollBar.AlwaysOff
            visible: false
            width: 0
        }
        ScrollBar.horizontal: FluxScrollBar {
            visible: !(hunkRoot.panel && hunkRoot.panel.effectiveWrap)
        }

        ListView {
            id: hunkListView
            SmoothWheelScroller {}
            focus: true
            width: (hunkRoot.panel && hunkRoot.panel.effectiveWrap)
                ? hunkScrollView.width
                : Math.max(hunkScrollView.width, (hunkRoot.panel ? hunkRoot.panel.maxCodeWidth : 0) + 120)
            model: hunkRoot.visible && hunkRoot.diffViewer ? hunkRoot.diffViewer.hunks : null
            spacing: 12
            boundsBehavior: Flickable.StopAtBounds
            maximumFlickVelocity: 6000
            flickDeceleration: 1000
            Keys.onPressed: (event) => {
                if (hunkRoot.panel) hunkRoot.panel.handleDiffKeys(event);
            }

            delegate: Rectangle {
                id: hunkCardDelegate
                readonly property int hunkIndex: index
                readonly property real hunkTextAreaWidth: Math.max(60, hunkCardDelegate.width - 125)
                readonly property bool wrapEnabled: Boolean(hunkRoot.panel && hunkRoot.panel.effectiveWrap)
                x: 12
                width: hunkListView.width - 24
                implicitHeight: hunkCol.implicitHeight + 16
                color: hunkRoot.theme.surface
                radius: hunkRoot.theme.radiusXs
                border.color: hunkRoot.theme.border
                border.width: 1

                ColumnLayout {
                    id: hunkCol
                    anchors.fill: parent
                    anchors.margins: 8
                    spacing: 4

                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 30
                        color: hunkRoot.theme.canvas
                        radius: 4
                        border.color: hunkRoot.theme.borderSubtle

                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 8
                            anchors.rightMargin: 8
                            spacing: 8
                            Text { text: "🔍"; font.pixelSize: 10 }
                            Text {
                                text: modelData.header || ""
                                color: hunkRoot.theme.borderFocus
                                font.family: hunkRoot.panel ? hunkRoot.panel.monoFont : "Consolas"
                                font.pixelSize: 11
                                font.bold: true
                                elide: Text.ElideRight
                                Layout.fillWidth: true
                            }
                            DiffHunkExpandActions {
                                diffViewer: hunkRoot.diffViewer
                            }
                        }
                    }

                    Item {
                        Layout.fillWidth: true
                        implicitHeight: hunkLinesCol.implicitHeight

                        ColumnLayout {
                            id: hunkLinesCol
                            anchors.fill: parent
                            spacing: 4

                            Repeater {
                                id: hunkLinesRepeater
                                model: (modelData.lines && modelData.lines.length > 300) ? modelData.lines.slice(0, 300) : (modelData.lines || [])

                                delegate: Rectangle {
                                    readonly property alias codeEdit: hunkLineText
                                    Layout.fillWidth: true
                                    Layout.preferredHeight: hunkCardDelegate.wrapEnabled ? Math.max(20, hunkLineText.implicitHeight + 6) : 20
                                    color: modelData.bgColor || "transparent"

                                    RowLayout {
                                        anchors.fill: parent
                                        spacing: 6

                                        Text {
                                            Layout.preferredWidth: 36
                                            Layout.alignment: hunkCardDelegate.wrapEnabled ? Qt.AlignTop : Qt.AlignVCenter
                                            Layout.topMargin: hunkCardDelegate.wrapEnabled ? 2 : 0
                                            text: modelData.oldLineNumber || ""
                                            color: hunkRoot.theme.textMuted
                                            font.family: hunkRoot.panel ? hunkRoot.panel.monoFont : "Consolas"
                                            font.pixelSize: 10
                                            horizontalAlignment: Text.AlignRight
                                        }

                                        Text {
                                            Layout.preferredWidth: 36
                                            Layout.alignment: hunkCardDelegate.wrapEnabled ? Qt.AlignTop : Qt.AlignVCenter
                                            Layout.topMargin: hunkCardDelegate.wrapEnabled ? 2 : 0
                                            text: modelData.newLineNumber || ""
                                            color: hunkRoot.theme.textMuted
                                            font.family: hunkRoot.panel ? hunkRoot.panel.monoFont : "Consolas"
                                            font.pixelSize: 10
                                            horizontalAlignment: Text.AlignRight
                                        }

                                        Rectangle {
                                            Layout.preferredWidth: 1
                                            Layout.fillHeight: true
                                            color: hunkRoot.panel ? hunkRoot.panel.gutterBorderColor : hunkRoot.theme.borderSubtle
                                        }

                                        Text {
                                            Layout.preferredWidth: 12
                                            Layout.alignment: hunkCardDelegate.wrapEnabled ? Qt.AlignTop : Qt.AlignVCenter
                                            Layout.topMargin: hunkCardDelegate.wrapEnabled ? 2 : 0
                                            text: {
                                                if (modelData.kind === "addition") return "+";
                                                if (modelData.kind === "deletion") return "-";
                                                return " ";
                                            }
                                            color: modelData.fgColor || hunkRoot.theme.textPrimary
                                            font.family: hunkRoot.panel ? hunkRoot.panel.monoFont : "Consolas"
                                            font.pixelSize: 11
                                            font.bold: true
                                            horizontalAlignment: Text.AlignHCenter
                                        }

                                        Item {
                                            Layout.fillWidth: true
                                            Layout.fillHeight: true
                                            clip: true

                                            DiffCodeText {
                                                id: hunkLineText
                                                panel: hunkRoot.panel
                                                selTargetName: "hunk"
                                                hunkIdx: hunkCardDelegate.hunkIndex
                                                rowIndex: index
                                                anchors.verticalCenter: parent.verticalCenter
                                                width: hunkCardDelegate.wrapEnabled
                                                    ? hunkCardDelegate.hunkTextAreaWidth
                                                    : Math.max(hunkCardDelegate.hunkTextAreaWidth, hunkRoot.panel ? hunkRoot.panel.maxCodeWidth : 0)
                                                height: implicitHeight
                                                text: modelData.richContent || modelData.content || ""
                                                color: modelData.fgColor || hunkRoot.theme.textPrimary
                                                font.pixelSize: 11
                                                wrapMode: hunkCardDelegate.wrapEnabled ? TextEdit.WrapAtWordBoundaryOrAnywhere : TextEdit.NoWrap
                                            }
                                        }
                                    }

                                    Rectangle {
                                        anchors.left: parent.left
                                        anchors.right: parent.right
                                        anchors.bottom: parent.bottom
                                        height: 1
                                        color: hunkRoot.panel ? hunkRoot.panel.gridLineColor : Qt.rgba(0.58, 0.64, 0.72, 0.14)
                                    }
                                }
                            }
                        }

                        MouseArea {
                            id: hunkSelectionMouse
                            anchors.fill: parent
                            anchors.leftMargin: 96
                            acceptedButtons: Qt.LeftButton | Qt.RightButton
                            hoverEnabled: true
                            preventStealing: isSelecting
                            cursorShape: Qt.IBeamCursor
                            property bool isSelecting: false

                            function hitTestHunk(mouseX, mouseY) {
                                const count = hunkLinesRepeater.count;
                                if (count <= 0) return null;
                                let bestIdx = 0;
                                if (mouseY >= 0) {
                                    for (let i = 0; i < count; ++i) {
                                        const it = hunkLinesRepeater.itemAt(i);
                                        if (!it) continue;
                                        bestIdx = i;
                                        if (mouseY <= it.y + it.height + 2) {
                                            break;
                                        }
                                    }
                                }
                                const rowItem = hunkLinesRepeater.itemAt(bestIdx);
                                if (!rowItem || !rowItem.codeEdit) return null;
                                const ce = rowItem.codeEdit;
                                const pt = mapToItem(ce, mouseX, mouseY);
                                const cx = Math.max(0, Math.min(ce.width, pt.x));
                                const cy = Math.max(0, Math.min(Math.max(0, ce.height - 1), pt.y));
                                return { index: bestIdx, col: ce.positionAt(cx, cy), codeEdit: ce };
                            }

                            onPressed: (mouse) => {
                                if (!hunkRoot.panel) return;
                                hunkListView.forceActiveFocus();
                                const hit = hitTestHunk(mouse.x, mouse.y);
                                if (mouse.button === Qt.RightButton) {
                                    hunkRoot.panel.contextMenuRow = hit ? hit.index : -1;
                                    hunkRoot.panel.contextMenuTarget = "hunk";
                                    hunkRoot.panel.contextMenuHunkIndex = hunkCardDelegate.hunkIndex;
                                    hunkRoot.panel.copyMenu.popup(hunkSelectionMouse, mouse.x, mouse.y);
                                    return;
                                }
                                if (mouse.button !== Qt.LeftButton || !hit) {
                                    mouse.accepted = false;
                                    return;
                                }
                                isSelecting = true;
                                if ((mouse.modifiers & Qt.ShiftModifier) !== 0
                                        && hunkRoot.panel.selTarget === "hunk"
                                        && hunkRoot.panel.selHunkIndex === hunkCardDelegate.hunkIndex
                                        && hunkRoot.panel.selAnchorRow >= 0) {
                                    hunkRoot.panel.selFocusRow = hit.index;
                                    hunkRoot.panel.selFocusCol = hit.col;
                                } else {
                                    hunkRoot.panel.selTarget = "hunk";
                                    hunkRoot.panel.selHunkIndex = hunkCardDelegate.hunkIndex;
                                    hunkRoot.panel.selAnchorRow = hit.index;
                                    hunkRoot.panel.selAnchorCol = hit.col;
                                    hunkRoot.panel.selFocusRow = hit.index;
                                    hunkRoot.panel.selFocusCol = hit.col;
                                }
                                hunkRoot.panel.selectionStateChanged();
                            }

                            onPositionChanged: (mouse) => {
                                if (!isSelecting || !hunkRoot.panel) return;
                                if ((mouse.buttons & Qt.LeftButton) === 0) {
                                    isSelecting = false;
                                    return;
                                }
                                const hit = hitTestHunk(mouse.x, mouse.y);
                                if (!hit) return;
                                if (hunkRoot.panel.selFocusRow !== hit.index || hunkRoot.panel.selFocusCol !== hit.col) {
                                    hunkRoot.panel.selFocusRow = hit.index;
                                    hunkRoot.panel.selFocusCol = hit.col;
                                    hunkRoot.panel.selectionStateChanged();
                                }
                            }

                            onDoubleClicked: (mouse) => {
                                if (!hunkRoot.panel || mouse.button !== Qt.LeftButton) return;
                                const hit = hitTestHunk(mouse.x, mouse.y);
                                if (!hit || !hit.codeEdit) return;
                                hit.codeEdit.cursorPosition = hit.col;
                                hit.codeEdit.selectWord();
                                hunkRoot.panel.selTarget = "hunk";
                                hunkRoot.panel.selHunkIndex = hunkCardDelegate.hunkIndex;
                                hunkRoot.panel.selAnchorRow = hit.index;
                                hunkRoot.panel.selAnchorCol = hit.codeEdit.selectionStart;
                                hunkRoot.panel.selFocusRow = hit.index;
                                hunkRoot.panel.selFocusCol = hit.codeEdit.selectionEnd;
                                hunkRoot.panel.selectionStateChanged();
                                isSelecting = false;
                            }

                            onReleased: isSelecting = false
                            onCanceled: isSelecting = false
                        }
                    }
                }
            }
        }
    }

    DiffOverviewScrollBar {
        id: hunkOverviewBar
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.right: parent.right
        flickable: hunkListView
        markers: hunkRoot.diffViewer ? hunkRoot.diffViewer.hunkOverviewMarkers : []
    }
}
