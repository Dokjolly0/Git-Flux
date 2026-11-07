import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root
    color: "#0E1117"
    implicitWidth: 800
    implicitHeight: 600

    property var sessionVm: null
    property var branchManager: (sessionVm && sessionVm.branchManager) ? sessionVm.branchManager : null
    property var selectedCommit: null

    // Automatically select the first commit when commit history is loaded or updated
    Connections {
        target: (root.branchManager && root.branchManager.commitHistory) ? root.branchManager.commitHistory : null
        function onModelReset() {
            root.autoSelectFirstCommit();
        }
        function onRowsInserted() {
            root.autoSelectFirstCommit();
        }
    }

    function autoSelectFirstCommit() {
        if (!root.branchManager || !root.branchManager.commitHistory) {
            root.selectedCommit = null;
            return;
        }
        var model = root.branchManager.commitHistory;
        if (model.rowCount() > 0) {
            var firstIdx = model.index(0, 0);
            root.selectedCommit = {
                sha: model.data(firstIdx, 257),         // ShaRole
                shortSha: model.data(firstIdx, 258),    // ShortShaRole
                subject: model.data(firstIdx, 259),     // SubjectRole
                body: model.data(firstIdx, 260),        // BodyRole
                authorName: model.data(firstIdx, 261),  // AuthorNameRole
                authorEmail: model.data(firstIdx, 262), // AuthorEmailRole
                authorDate: model.data(firstIdx, 263),  // AuthorDateRole
                decorations: model.data(firstIdx, 264), // DecorationsRole
                parents: model.data(firstIdx, 265)      // ParentsRole
            };
        } else {
            root.selectedCommit = null;
        }
    }

    Component.onCompleted: {
        root.autoSelectFirstCommit();
    }

    RowLayout {
        anchors.fill: parent
        spacing: 0

        // ==========================================
        // 1. COMMIT LIST (Table / Graph)
        // ==========================================
        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.minimumWidth: 380
            color: "#161B22"

            ColumnLayout {
                anchors.fill: parent
                spacing: 0

                // Search & Filter header
                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 44
                    color: "#0F172A"
                    border.color: "#1E293B"
                    border.width: 1

                    RowLayout {
                        anchors.fill: parent
                        anchors.margins: 6
                        spacing: 8

                        Text {
                            text: "🔍"
                            font.pixelSize: 12
                            color: "#94A3B8"
                        }

                        TextField {
                            id: historySearchField
                            Layout.fillWidth: true
                            Layout.preferredHeight: 30
                            placeholderText: "Filtra per messaggio, autore o SHA..."
                            placeholderTextColor: "#64748B"
                            color: "#F8FAFC"
                            font.pixelSize: 12
                            background: Rectangle {
                                color: "#1E293B"
                                radius: 4
                                border.color: parent.activeFocus ? "#3B82F6" : "#334155"
                                border.width: 1
                            }
                        }

                        Button {
                            text: "Aggiorna"
                            Layout.preferredHeight: 30
                            contentItem: RowLayout {
                                spacing: 4
                                Text { text: "🔄"; font.pixelSize: 11 }
                                Text { text: "Aggiorna"; color: "#CBD5E1"; font.pixelSize: 11; font.bold: true }
                            }
                            background: Rectangle {
                                color: parent.hovered ? "#334155" : "#1E293B"
                                radius: 4
                                border.color: "#334155"
                            }
                            onClicked: {
                                if (root.branchManager) {
                                    root.branchManager.refresh();
                                }
                            }
                        }
                    }
                }

                // Commit list
                ListView {
                    id: commitListView
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    model: root.branchManager ? root.branchManager.commitHistory : null

                    delegate: Rectangle {
                        id: commitRow
                        required property string sha
                        required property string shortSha
                        required property string subject
                        required property string body
                        required property string authorName
                        required property string authorEmail
                        required property string authorDate
                        required property var decorations
                        required property var parents
                        required property int index

                        width: commitListView.width
                        height: isMatchingFilter ? 52 : 0
                        visible: isMatchingFilter

                        property bool isMatchingFilter: {
                            if (historySearchField.text.trim().length === 0) return true;
                            var query = historySearchField.text.trim().toLowerCase();
                            var subj = (commitRow.subject || "").toLowerCase();
                            var sSha = (commitRow.shortSha || "").toLowerCase();
                            var auth = (commitRow.authorName || "").toLowerCase();
                            return subj.indexOf(query) !== -1 || sSha.indexOf(query) !== -1 || auth.indexOf(query) !== -1;
                        }

                        color: (root.selectedCommit && root.selectedCommit.sha === commitRow.sha)
                               ? "#1E293B"
                               : (itemMouse.containsMouse ? "#1E293B66" : "transparent")
                        border.color: (root.selectedCommit && root.selectedCommit.sha === commitRow.sha) ? "#3B82F6" : "#1E293B"
                        border.width: 1

                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 12
                            anchors.rightMargin: 12
                            spacing: 10

                            // Commit dot in graph
                            Rectangle {
                                width: 10
                                height: 10
                                radius: 5
                                color: "#3B82F6"
                                border.color: "#60A5FA"
                                border.width: 2
                            }

                            // Commit details
                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 2

                                RowLayout {
                                    spacing: 6
                                    Layout.fillWidth: true

                                    // Ref Decorations Badge (Branches/Tags)
                                    Repeater {
                                        model: commitRow.decorations || []

                                        delegate: Rectangle {
                                            height: 18
                                            implicitWidth: decText.implicitWidth + 8
                                            radius: 3
                                            color: (modelData && modelData.indexOf("tag:") !== -1) ? "#854D0E" : "#1E3A8A"

                                            Text {
                                                id: decText
                                                anchors.centerIn: parent
                                                text: modelData || ""
                                                color: (modelData && modelData.indexOf("tag:") !== -1) ? "#FEF08A" : "#93C5FD"
                                                font.pixelSize: 10
                                                font.bold: true
                                            }
                                        }
                                    }

                                    Text {
                                        text: commitRow.subject || "(Nessun messaggio)"
                                        color: "#F8FAFC"
                                        font.bold: true
                                        font.pixelSize: 12
                                        elide: Text.ElideRight
                                        Layout.fillWidth: true
                                    }
                                }

                                RowLayout {
                                    spacing: 8
                                    Text {
                                        text: commitRow.authorName || ""
                                        color: "#94A3B8"
                                        font.pixelSize: 11
                                    }
                                    Text {
                                        text: "•"
                                        color: "#475569"
                                        font.pixelSize: 10
                                        visible: (commitRow.authorName || "").length > 0
                                    }
                                    Text {
                                        text: commitRow.authorDate || ""
                                        color: "#64748B"
                                        font.pixelSize: 11
                                    }
                                }
                            }

                            // Short SHA monospace
                            Rectangle {
                                Layout.preferredHeight: 20
                                Layout.preferredWidth: 64
                                radius: 3
                                color: "#0F172A"
                                border.color: "#334155"
                                border.width: 1

                                Text {
                                    anchors.centerIn: parent
                                    text: commitRow.shortSha || ""
                                    color: "#38BDF8"
                                    font.family: "monospace"
                                    font.pixelSize: 10
                                }
                            }
                        }

                        MouseArea {
                            id: itemMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                root.selectedCommit = {
                                    sha: commitRow.sha,
                                    shortSha: commitRow.shortSha,
                                    subject: commitRow.subject,
                                    body: commitRow.body,
                                    authorName: commitRow.authorName,
                                    authorEmail: commitRow.authorEmail,
                                    authorDate: commitRow.authorDate,
                                    decorations: commitRow.decorations,
                                    parents: commitRow.parents
                                };
                            }
                        }
                    }

                    // Empty state when repository has no commits or history is empty
                    Rectangle {
                        anchors.centerIn: parent
                        visible: commitListView.count === 0
                        color: "transparent"
                        width: 320
                        height: 140

                        ColumnLayout {
                            anchors.centerIn: parent
                            spacing: 10

                            Text {
                                text: "📜"
                                font.pixelSize: 32
                                Layout.alignment: Qt.AlignHCenter
                            }

                            Text {
                                text: "Nessun commit trovato per questo repository."
                                color: "#F0F6FC"
                                font.pixelSize: 13
                                font.bold: true
                                horizontalAlignment: Text.AlignHCenter
                                Layout.alignment: Qt.AlignHCenter
                            }

                            Text {
                                text: "Il repository potrebbe essere vuoto o inizializzato di recente."
                                color: "#8B949E"
                                font.pixelSize: 11
                                horizontalAlignment: Text.AlignHCenter
                                Layout.alignment: Qt.AlignHCenter
                            }

                            Button {
                                Layout.alignment: Qt.AlignHCenter
                                text: "🔄 Ricarica cronologia"
                                contentItem: Text {
                                    text: "🔄 Ricarica cronologia"
                                    color: "#38BDF8"
                                    font.pixelSize: 11
                                    font.bold: true
                                }
                                background: Rectangle {
                                    color: parent.hovered ? "#1E293B" : "#0F172A"
                                    radius: 4
                                    border.color: "#38BDF8"
                                }
                                onClicked: {
                                    if (root.branchManager) root.branchManager.refresh();
                                }
                            }
                        }
                    }
                }
            }
        }

        // ==========================================
        // 2. SELECTED COMMIT DETAILS PANEL
        // ==========================================
        Rectangle {
            Layout.preferredWidth: 380
            Layout.fillHeight: true
            color: "#0F172A"
            border.color: "#1E293B"
            border.width: 1

            // Commit details view when a commit is selected
            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 16
                spacing: 12
                visible: root.selectedCommit !== null

                Text {
                    text: "Dettaglio Commit"
                    color: "#94A3B8"
                    font.bold: true
                    font.pixelSize: 11
                }

                Text {
                    text: root.selectedCommit ? root.selectedCommit.subject : ""
                    color: "#FFFFFF"
                    font.bold: true
                    font.pixelSize: 14
                    wrapMode: Text.Wrap
                    Layout.fillWidth: true
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 1
                    color: "#334155"
                }

                // Author and date
                RowLayout {
                    spacing: 8
                    Text {
                        text: "Autore:"
                        color: "#64748B"
                        font.pixelSize: 12
                    }
                    Text {
                        text: root.selectedCommit ? (root.selectedCommit.authorName + " <" + root.selectedCommit.authorEmail + ">") : ""
                        color: "#E2E8F0"
                        font.pixelSize: 12
                        elide: Text.ElideRight
                        Layout.fillWidth: true
                    }
                }

                RowLayout {
                    spacing: 8
                    Text {
                        text: "Data:"
                        color: "#64748B"
                        font.pixelSize: 12
                    }
                    Text {
                        text: root.selectedCommit ? root.selectedCommit.authorDate : ""
                        color: "#CBD5E1"
                        font.pixelSize: 12
                    }
                }

                RowLayout {
                    spacing: 8
                    Text {
                        text: "SHA:"
                        color: "#64748B"
                        font.pixelSize: 12
                    }
                    Text {
                        text: root.selectedCommit ? root.selectedCommit.sha : ""
                        color: "#38BDF8"
                        font.family: "monospace"
                        font.pixelSize: 11
                        elide: Text.ElideMiddle
                        Layout.fillWidth: true
                    }
                }

                // Message body
                Text {
                    text: "Messaggio esteso:"
                    color: "#64748B"
                    font.pixelSize: 12
                }

                ScrollView {
                    Layout.fillWidth: true
                    Layout.fillHeight: true

                    TextArea {
                        readOnly: true
                        text: (root.selectedCommit && root.selectedCommit.body && root.selectedCommit.body.length > 0)
                              ? root.selectedCommit.body
                              : "(Nessun corpo aggiuntivo nel commit)"
                        color: "#CBD5E1"
                        font.pixelSize: 12
                        wrapMode: Text.Wrap
                        background: Rectangle {
                            color: "#161B22"
                            radius: 4
                            border.color: "#1E293B"
                            border.width: 1
                        }
                    }
                }
            }

            // Placeholder when no commit is selected
            Rectangle {
                anchors.centerIn: parent
                visible: root.selectedCommit === null
                color: "transparent"
                width: 260
                height: 120

                ColumnLayout {
                    anchors.centerIn: parent
                    spacing: 8

                    Text {
                        text: "👆"
                        font.pixelSize: 28
                        Layout.alignment: Qt.AlignHCenter
                    }

                    Text {
                        text: "Seleziona un commit dalla lista per visualizzarne i dettagli completi, autore, data e messaggio."
                        color: "#64748B"
                        font.pixelSize: 12
                        wrapMode: Text.Wrap
                        horizontalAlignment: Text.AlignHCenter
                        Layout.fillWidth: true
                    }
                }
            }
        }
    }
}
