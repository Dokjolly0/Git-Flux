import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root
    color: "#0E1117"

    property var sessionVm: null

    signal returnToHomeRequested()
    signal quickOpenRequested()

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // ==========================================
        // 1. REPO TABS HEADER (Wireframe Pagina 04)
        // ==========================================
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 44
            color: "#090D13"

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 8
                anchors.rightMargin: 8
                spacing: 8

                // Pulsante Torna a Home Hub
                Button {
                    Layout.preferredHeight: 32
                    background: Rectangle {
                        color: parent.hovered ? "#1E293B" : "transparent"
                        radius: 6
                    }
                    contentItem: RowLayout {
                        spacing: 6
                        Text {
                            text: "🏠"
                            font.pixelSize: 14
                        }
                        Text {
                            text: "Home"
                            color: "#CBD5E1"
                            font.bold: true
                            font.pixelSize: 12
                        }
                    }
                    onClicked: {
                        root.returnToHomeRequested();
                    }
                }

                // Pulsante Quick Open
                Button {
                    Layout.preferredHeight: 32
                    background: Rectangle {
                        color: parent.hovered ? "#1E293B" : "#161B22"
                        radius: 6
                        border.color: "#334155"
                        border.width: 1
                    }
                    contentItem: RowLayout {
                        spacing: 6
                        Text {
                            text: "⚡"
                            font.pixelSize: 13
                        }
                        Text {
                            text: "Quick Open"
                            color: "#F8FAFC"
                            font.bold: true
                            font.pixelSize: 12
                        }
                    }
                    onClicked: {
                        root.quickOpenRequested();
                    }
                }

                // Separatore
                Rectangle {
                    Layout.preferredWidth: 1
                    Layout.preferredHeight: 22
                    color: "#334155"
                }

                // Tab Bar Scorrevole dei Repository Aperti
                ScrollView {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true

                    RowLayout {
                        spacing: 4

                        Repeater {
                            model: sessionVm ? sessionVm.tabs : null

                            delegate: Rectangle {
                                id: tabRect
                                Layout.preferredHeight: 34
                                implicitWidth: tabRow.implicitWidth + 24
                                radius: 6
                                color: (sessionVm && sessionVm.activeTabIndex === index) ? "#1E293B" : (tabMouseArea.containsMouse ? "#161B22" : "transparent")
                                border.color: (sessionVm && sessionVm.activeTabIndex === index) ? "#3B82F6" : "transparent"
                                border.width: 1

                                RowLayout {
                                    id: tabRow
                                    anchors.centerIn: parent
                                    spacing: 8

                                    // Indicatore Dirty
                                    Rectangle {
                                        width: 8
                                        height: 8
                                        radius: 4
                                        color: modelData.isDirty ? "#F59E0B" : "transparent"
                                        visible: modelData.isDirty
                                    }

                                    // Icona Cartella
                                    Text {
                                        text: "📁"
                                        font.pixelSize: 12
                                    }

                                    // Nome Repository
                                    Text {
                                        text: modelData.name
                                        color: (sessionVm && sessionVm.activeTabIndex === index) ? "#FFFFFF" : "#94A3B8"
                                        font.bold: (sessionVm && sessionVm.activeTabIndex === index)
                                        font.pixelSize: 12
                                    }

                                    // Crocetta Chiusura Tab
                                    MouseArea {
                                        width: 16
                                        height: 16
                                        hoverEnabled: true
                                        cursorShape: Qt.PointingHandCursor

                                        Text {
                                            anchors.centerIn: parent
                                            text: "×"
                                            color: parent.containsMouse ? "#EF4444" : "#64748B"
                                            font.pixelSize: 14
                                            font.bold: true
                                        }

                                        onClicked: {
                                            if (sessionVm) {
                                                sessionVm.closeTab(index);
                                                if (!sessionVm.hasOpenTabs) {
                                                    root.returnToHomeRequested();
                                                }
                                            }
                                        }
                                    }
                                }

                                MouseArea {
                                    id: tabMouseArea
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: {
                                        if (sessionVm) {
                                            sessionVm.activeTabIndex = index;
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }

        // ==========================================
        // 2. CORPO DELLA SESSIONE (SplitView)
        // ==========================================
        SplitView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            orientation: Qt.Horizontal

            // Colonna Sinistra: Working Tree + Commit Box
            SplitView {
                SplitView.preferredWidth: 380
                SplitView.minimumWidth: 280
                SplitView.maximumWidth: 600
                orientation: Qt.Vertical

                WorkingTreePanel {
                    SplitView.fillHeight: true
                    SplitView.minimumHeight: 200
                    sessionVm: root.sessionVm
                }

                CommitBox {
                    SplitView.preferredHeight: 180
                    SplitView.minimumHeight: 140
                    SplitView.maximumHeight: 300
                    sessionVm: root.sessionVm
                }
            }

            // Colonna Destra: Diff Viewer
            DiffViewerPanel {
                SplitView.fillWidth: true
                SplitView.minimumWidth: 400
                sessionVm: root.sessionVm
            }
        }
    }
}
