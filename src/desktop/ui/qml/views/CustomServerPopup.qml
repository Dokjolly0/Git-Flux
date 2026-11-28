import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../themes"

Popup {
    id: customServerPopup
    readonly property FluxTheme theme: FluxTheme {}

    property var profilesVm: null
    property bool showToken: false

    signal serverAdded()

    parent: Overlay.overlay
    anchors.centerIn: parent
    width: 460
    modal: true
    focus: true
    padding: 0
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

    function openForNew() {
        customNameField.text = "";
        customHostField.text = "";
        customApiField.text = "";
        customUserField.text = "";
        customTokenField.text = "";
        customServerPopup.showToken = false;
        customServerPopup.open();
        customHostField.forceActiveFocus();
    }

    function submit() {
        if (!customServerPopup.profilesVm || customHostField.text.trim().length === 0) return;
        customServerPopup.profilesVm.addCustomProfile(
            customHostField.text,
            customNameField.text,
            customTokenField.text,
            customApiField.text,
            customUserField.text
        );
        customTokenField.text = "";
        customServerPopup.close();
        customServerPopup.serverAdded();
    }

    background: Rectangle {
        color: customServerPopup.theme.surfaceElevated
        radius: customServerPopup.theme.radiusMd
        border.color: customServerPopup.theme.borderFocus
        border.width: 1
    }

    contentItem: ColumnLayout {
        spacing: 10

        Item { Layout.preferredHeight: 4 }

        RowLayout {
            Layout.leftMargin: 16
            Layout.rightMargin: 16
            spacing: 8

            Image {
                source: "qrc:/qt/qml/GitFlux/Desktop/assets/icons/providers/server.png"
                Layout.preferredWidth: 16
                Layout.preferredHeight: 16
                Layout.alignment: Qt.AlignVCenter
                fillMode: Image.PreserveAspectFit
                smooth: true
                mipmap: true
            }

            Text {
                text: "Aggiungi server Git personalizzato"
                color: customServerPopup.theme.textPrimary
                font.bold: true
                font.pixelSize: 14
            }
        }

        Text {
            Layout.leftMargin: 16
            Layout.rightMargin: 16
            Layout.fillWidth: true
            text: "Gitea, Forgejo, Codeberg o qualsiasi istanza compatibile con l'API /api/v1. Il token è opzionale: senza token vengono elencati solo i repository pubblici."
            color: customServerPopup.theme.textMuted
            font.pixelSize: 11
            wrapMode: Text.Wrap
        }

        GridLayout {
            Layout.leftMargin: 16
            Layout.rightMargin: 16
            Layout.fillWidth: true
            columns: 2
            columnSpacing: 10
            rowSpacing: 8

            Text { text: "Host *"; color: customServerPopup.theme.textSecondary; font.pixelSize: 11; font.bold: true }
            FluxTextField {
                id: customHostField
                Layout.fillWidth: true
                Layout.preferredHeight: 32
                placeholderText: "gitea.example.com o https://192.168.1.30:3000"
                onAccepted: customNameField.forceActiveFocus()
                onEditingFinished: {
                    const t = text.trim();
                    if (customApiField.text.trim().length === 0 && t.indexOf("://") > 0) {
                        customApiField.text = t;
                    }
                }
            }

            Text { text: "Nome"; color: customServerPopup.theme.textSecondary; font.pixelSize: 11; font.bold: true }
            FluxTextField {
                id: customNameField
                Layout.fillWidth: true
                Layout.preferredHeight: 32
                placeholderText: "Es. Gitea di casa (default: host)"
                onAccepted: customApiField.forceActiveFocus()
            }

            Text { text: "URL API"; color: customServerPopup.theme.textSecondary; font.pixelSize: 11; font.bold: true }
            FluxTextField {
                id: customApiField
                Layout.fillWidth: true
                Layout.preferredHeight: 32
                placeholderText: "Default: https://<host> (+ /api/v1)"
                onAccepted: customUserField.forceActiveFocus()
            }

            Text { text: "Username"; color: customServerPopup.theme.textSecondary; font.pixelSize: 11; font.bold: true }
            FluxTextField {
                id: customUserField
                Layout.fillWidth: true
                Layout.preferredHeight: 32
                placeholderText: "Opzionale"
                onAccepted: customTokenField.forceActiveFocus()
            }

            Text { text: "Token"; color: customServerPopup.theme.textSecondary; font.pixelSize: 11; font.bold: true }
            RowLayout {
                Layout.fillWidth: true
                spacing: 6
                FluxTextField {
                    id: customTokenField
                    Layout.fillWidth: true
                    Layout.preferredHeight: 32
                    placeholderText: "Access Token (opzionale)"
                    echoMode: customServerPopup.showToken ? TextInput.Normal : TextInput.Password
                    onAccepted: customServerPopup.submit()
                }
                FluxActionButton {
                    text: customServerPopup.showToken ? "🙈" : "👁️"
                    Layout.preferredWidth: 32
                    Layout.preferredHeight: 32
                    onClicked: customServerPopup.showToken = !customServerPopup.showToken
                }
            }
        }

        RowLayout {
            Layout.leftMargin: 16
            Layout.rightMargin: 16
            Layout.bottomMargin: 14
            Layout.topMargin: 4
            Layout.fillWidth: true
            spacing: 8

            Item { Layout.fillWidth: true }

            FluxActionButton {
                text: "Annulla"
                Layout.preferredHeight: 32
                onClicked: customServerPopup.close()
            }

            FluxActionButton {
                text: "＋ Aggiungi server"
                variant: "primary"
                Layout.preferredHeight: 32
                enabled: customHostField.text.trim().length > 0
                onClicked: customServerPopup.submit()
            }
        }
    }
}
