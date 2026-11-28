import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../themes"

Rectangle {
    id: commitBox
    readonly property FluxTheme theme: FluxTheme {}

    property var sessionVm: null
    property int totalFilesCount: 0

    signal commitRequested(string summary, string description)

    implicitHeight: commitBoxCol.implicitHeight + 20
    radius: 10
    color: commitBox.theme.inputBg
    border.color: commitBox.theme.border
    border.width: 1

    Connections {
        target: (commitBox.sessionVm && commitBox.sessionVm.commitBox) ? commitBox.sessionVm.commitBox : null

        function onCommitSucceeded() {
            wipSummaryField.text = "";
            wipDescriptionField.text = "";
        }
    }

    ColumnLayout {
        id: commitBoxCol
        anchors.fill: parent
        anchors.margins: 10
        spacing: 8

        // Commit Title / Summary
        FluxTextField {
            id: wipSummaryField
            Layout.fillWidth: true
            Layout.preferredHeight: 34
            placeholderText: "Riepilogo commit (es: feat: nuova funzionalità)"
            font.pixelSize: 11
        }

        // Commit Description (multiline optional)
        TextArea {
            id: wipDescriptionField
            Layout.fillWidth: true
            Layout.preferredHeight: 52
            placeholderText: "Descrizione dettagliata (opzionale)..."
            placeholderTextColor: commitBox.theme.textMuted
            color: commitBox.theme.textSecondary
            font.pixelSize: 10
            wrapMode: Text.Wrap
            selectByMouse: true
            background: Rectangle {
                color: commitBox.theme.card
                radius: 8
                border.color: wipDescriptionField.activeFocus ? commitBox.theme.accentCyan : commitBox.theme.border
                border.width: 1
                Behavior on border.color {
                    ColorAnimation {
                        duration: commitBox.theme.animFast
                    }
                }
            }
        }

        // Commit Action Button
        Button {
            id: wipCommitButton
            Layout.fillWidth: true
            Layout.preferredHeight: 34
            hoverEnabled: true
            enabled: wipSummaryField.text.trim().length > 0 && commitBox.totalFilesCount > 0

            background: Rectangle {
                color: !wipCommitButton.enabled ? commitBox.theme.surfaceElevated : (wipCommitButton.hovered ? commitBox.theme.accentTeal : commitBox.theme.success)
                radius: 8
                border.color: wipCommitButton.enabled ? commitBox.theme.accentTeal : commitBox.theme.border
                border.width: 1
                Behavior on color {
                    ColorAnimation {
                        duration: commitBox.theme.animFast
                    }
                }
            }

            contentItem: RowLayout {
                anchors.centerIn: parent
                spacing: 6
                Text {
                    text: "✓"
                    color: wipCommitButton.enabled ? commitBox.theme.selectionText : commitBox.theme.textMuted
                    font.bold: true
                    font.pixelSize: 12
                }
                Text {
                    text: {
                        const staged = (commitBox.sessionVm && commitBox.sessionVm.workingTree) ? commitBox.sessionVm.workingTree.stagedCount : 0;
                        if (staged > 0) {
                            return "Crea commit (" + staged + " file in staging)";
                        }
                        return "Metti in staging e crea commit (" + commitBox.totalFilesCount + " file)";
                    }
                    color: wipCommitButton.enabled ? commitBox.theme.selectionText : commitBox.theme.textMuted
                    font.bold: true
                    font.pixelSize: 11
                }
            }

            onClicked: {
                commitBox.commitRequested(wipSummaryField.text.trim(), wipDescriptionField.text.trim());
            }
        }
    }
}
