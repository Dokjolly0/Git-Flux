import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root
    color: "#0F172A"

    property var sessionVm: null
    property string activeRepoPath: sessionVm ? sessionVm.activeRepoPath : ""
    property var commitBox: sessionVm ? sessionVm.commitBox : null
    property var workingTree: sessionVm ? sessionVm.workingTree : null

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 10
        spacing: 8

        // Commit Box Header
        RowLayout {
            Layout.fillWidth: true
            spacing: 6

            Text {
                text: "COMMIT"
                color: "#94A3B8"
                font.bold: true
                font.pixelSize: 11
                font.letterSpacing: 1
            }

            Item { Layout.fillWidth: true }

            // Amend Checkbox
            CheckBox {
                id: amendCheck
                text: "Amend"
                checked: commitBox ? commitBox.isAmend : false
                onToggled: {
                    if (commitBox) commitBox.isAmend = checked;
                }
                contentItem: Text {
                    text: parent.text
                    color: parent.checked ? "#38BDF8" : "#64748B"
                    font.pixelSize: 11
                    leftPadding: 20
                    verticalAlignment: Text.AlignVCenter
                }
            }
        }

        // Commit message textarea
        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            color: "#1E293B"
            radius: 6
            border.color: commitMessageArea.activeFocus ? "#3B82F6" : "#334155"
            border.width: 1

            ScrollView {
                anchors.fill: parent
                anchors.margins: 6
                clip: true

                TextArea {
                    id: commitMessageArea
                    placeholderText: "Scrivi messaggio di commit... (Ctrl+Enter)"
                    placeholderTextColor: "#64748B"
                    color: "#F8FAFC"
                    font.pixelSize: 12
                    font.family: "monospace"
                    wrapMode: TextEdit.Wrap
                    selectByMouse: true
                    text: commitBox ? commitBox.commitMessage : ""
                    onTextChanged: {
                        if (commitBox && commitBox.commitMessage !== text) {
                            commitBox.commitMessage = text;
                        }
                    }

                    Keys.onReturnPressed: function(event) {
                        if (event.modifiers & Qt.ControlModifier) {
                            doCommit();
                            event.accepted = true;
                        } else {
                            event.accepted = false;
                        }
                    }
                }
            }
        }

        // Error message if present
        Text {
            visible: commitBox ? commitBox.lastErrorMessage !== "" : false
            text: commitBox ? commitBox.lastErrorMessage : ""
            color: "#F87171"
            font.pixelSize: 11
            elide: Text.ElideRight
            Layout.fillWidth: true
        }

        // Commit submit button
        Button {
            id: commitBtn
            Layout.fillWidth: true
            Layout.preferredHeight: 32
            enabled: commitBox ? commitBox.canCommit : false
            ToolTip.visible: hovered
            ToolTip.text: "Esegui Commit (Ctrl+Invio)"

            background: Rectangle {
                color: {
                    if (!commitBtn.enabled) return "#1E293B";
                    if (commitBtn.pressed) return "#047857";
                    if (commitBtn.hovered) return "#059669";
                    return "#10B981";
                }
                radius: 6
            }

            contentItem: RowLayout {
                anchors.centerIn: parent
                spacing: 6

                Text {
                    text: "✓"
                    color: commitBtn.enabled ? "#FFFFFF" : "#64748B"
                    font.bold: true
                    font.pixelSize: 13
                }

                Text {
                    text: {
                        var b = (workingTree && workingTree.branchName !== "") ? (" to " + workingTree.branchName) : "";
                        var count = (workingTree) ? workingTree.stagedCount : 0;
                        if (amendCheck.checked) return "Amend Commit" + b;
                        return "Commit " + (count > 0 ? ("(" + count + ")") : "") + b;
                    }
                    color: commitBtn.enabled ? "#FFFFFF" : "#64748B"
                    font.bold: true
                    font.pixelSize: 12
                }
            }

            onClicked: {
                doCommit();
            }
        }

        Shortcut {
            sequence: "Ctrl+Return"
            enabled: commitBox ? commitBox.canCommit : false
            onActivated: doCommit()
        }
    }

    function doCommit() {
        if (commitBox && root.activeRepoPath !== "") {
            commitBox.executeCommit(root.activeRepoPath);
        }
    }
}
