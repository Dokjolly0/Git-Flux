import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs

Dialog {
    id: root
    title: "Clona Repository Remoto"
    modal: true
    width: 540
    anchors.centerIn: parent

    property var homeHubVm: null

    function openWithDefaultFolder(defaultParentFolder) {
        urlField.text = "";
        dirNameField.text = "";
        displayNameField.text = "";
        if (defaultParentFolder && defaultParentFolder.length > 0) {
            destFolderField.text = defaultParentFolder;
        }
        root.open();
        urlField.forceActiveFocus();
    }

    Connections {
        target: root.homeHubVm
        function onCloneCompleted(success, message) {
            if (success) {
                root.close();
            }
        }
    }

    FolderDialog {
        id: cloneParentFolderDialog
        title: "Seleziona cartella di destinazione locale"
        onAccepted: {
            var selected = cloneParentFolderDialog.selectedFolder.toString();
            if (selected.startsWith("file://")) {
                selected = selected.substring(7);
            }
            destFolderField.text = selected;
        }
    }

    background: Rectangle {
        color: "#0F172A"
        radius: 8
        border.color: "#334155"
        border.width: 1
    }

    header: Rectangle {
        color: "#1E293B"
        height: 48
        radius: 8
        Rectangle {
            anchors.bottom: parent.bottom
            width: parent.width
            height: 8
            color: "#1E293B"
        }
        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 16
            anchors.rightMargin: 16
            Text {
                text: "⬇ Clona Repository Remoto"
                color: "#F8FAFC"
                font.pixelSize: 15
                font.bold: true
                Layout.fillWidth: true
            }
            Button {
                text: "✕"
                flat: true
                enabled: !(root.homeHubVm && root.homeHubVm.isCloning)
                onClicked: root.close()
                contentItem: Text {
                    text: "✕"
                    color: parent.hovered ? "#EF4444" : "#94A3B8"
                    font.pixelSize: 13
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
                background: Item {}
            }
        }
    }

    contentItem: ColumnLayout {
        spacing: 14

        // 1. Remote URL
        ColumnLayout {
            Layout.fillWidth: true
            spacing: 4

            Text {
                text: "URL Repository Remoto (HTTPS o SSH):"
                color: "#94A3B8"
                font.pixelSize: 12
                font.bold: true
            }

            TextField {
                id: urlField
                Layout.fillWidth: true
                Layout.preferredHeight: 34
                enabled: !(root.homeHubVm && root.homeHubVm.isCloning)
                placeholderText: "es. https://github.com/utente/progetto.git o git@github.com:..."
                placeholderTextColor: "#475569"
                color: "#F8FAFC"
                font.pixelSize: 13
                background: Rectangle {
                    color: "#0B1120"
                    radius: 6
                    border.color: urlField.activeFocus ? "#38BDF8" : "#334155"
                    border.width: 1
                }
                onTextChanged: {
                    if (root.homeHubVm && text.length > 0) {
                        var extracted = root.homeHubVm.extractRepoNameFromUrl(text);
                        if (dirNameField.text.length === 0 || dirNameField.propertyModified !== true) {
                            dirNameField.text = extracted;
                        }
                    }
                }
            }
        }

        // 2. Local Destination Folder
        ColumnLayout {
            Layout.fillWidth: true
            spacing: 4

            Text {
                text: "Cartella Locale di Destinazione:"
                color: "#94A3B8"
                font.pixelSize: 12
                font.bold: true
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 8

                TextField {
                    id: destFolderField
                    Layout.fillWidth: true
                    Layout.preferredHeight: 34
                    enabled: !(root.homeHubVm && root.homeHubVm.isCloning)
                    placeholderText: "Seleziona cartella in cui scaricare il progetto..."
                    placeholderTextColor: "#475569"
                    color: "#F8FAFC"
                    font.pixelSize: 13
                    background: Rectangle {
                        color: "#0B1120"
                        radius: 6
                        border.color: destFolderField.activeFocus ? "#38BDF8" : "#334155"
                        border.width: 1
                    }
                }

                Button {
                    text: "Sfoglia..."
                    Layout.preferredHeight: 34
                    enabled: !(root.homeHubVm && root.homeHubVm.isCloning)
                    onClicked: cloneParentFolderDialog.open()
                    contentItem: Text {
                        text: "Sfoglia..."
                        color: "#38BDF8"
                        font.pixelSize: 12
                        font.bold: true
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                    background: Rectangle {
                        color: parent.hovered ? "#1E293B" : "#0F172A"
                        radius: 6
                        border.color: "#334155"
                        border.width: 1
                    }
                }
            }
        }

        // 3. Directory Name & Optional Display Name
        RowLayout {
            Layout.fillWidth: true
            spacing: 12

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 4

                property bool propertyModified: false

                Text {
                    text: "Nome Cartella:"
                    color: "#94A3B8"
                    font.pixelSize: 12
                    font.bold: true
                }

                TextField {
                    id: dirNameField
                    Layout.fillWidth: true
                    Layout.preferredHeight: 34
                    property bool propertyModified: false
                    enabled: !(root.homeHubVm && root.homeHubVm.isCloning)
                    placeholderText: "nome-cartella"
                    placeholderTextColor: "#475569"
                    color: "#F8FAFC"
                    font.pixelSize: 13
                    background: Rectangle {
                        color: "#0B1120"
                        radius: 6
                        border.color: dirNameField.activeFocus ? "#38BDF8" : "#334155"
                        border.width: 1
                    }
                    onTextEdited: propertyModified = true
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 4

                Text {
                    text: "Nome nel Catalogo (opzionale):"
                    color: "#94A3B8"
                    font.pixelSize: 12
                }

                TextField {
                    id: displayNameField
                    Layout.fillWidth: true
                    Layout.preferredHeight: 34
                    enabled: !(root.homeHubVm && root.homeHubVm.isCloning)
                    placeholderText: "Come mostrare in Git Flux"
                    placeholderTextColor: "#475569"
                    color: "#F8FAFC"
                    font.pixelSize: 13
                    background: Rectangle {
                        color: "#0B1120"
                        radius: 6
                        border.color: displayNameField.activeFocus ? "#38BDF8" : "#334155"
                        border.width: 1
                    }
                }
            }
        }

        // Progress Section
        ColumnLayout {
            Layout.fillWidth: true
            visible: root.homeHubVm ? root.homeHubVm.isCloning : false
            spacing: 6

            ProgressBar {
                Layout.fillWidth: true
                indeterminate: true
            }

            Text {
                text: root.homeHubVm ? root.homeHubVm.cloneStatusMessage : ""
                color: "#38BDF8"
                font.pixelSize: 12
                wrapMode: Text.Wrap
                Layout.fillWidth: true
            }
        }

        // Error message banner
        Rectangle {
            Layout.fillWidth: true
            visible: (root.homeHubVm && root.homeHubVm.lastError.length > 0 && !root.homeHubVm.isCloning)
            color: "#450A0A"
            radius: 6
            border.color: "#DC2626"
            border.width: 1
            implicitHeight: errorText.implicitHeight + 16

            Text {
                id: errorText
                anchors.fill: parent
                anchors.margins: 8
                text: root.homeHubVm ? root.homeHubVm.lastError : ""
                color: "#FECACA"
                font.pixelSize: 12
                wrapMode: Text.Wrap
            }
        }
    }

    footer: Rectangle {
        color: "#1E293B"
        height: 52
        radius: 8
        Rectangle {
            anchors.top: parent.top
            width: parent.width
            height: 8
            color: "#1E293B"
        }
        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 16
            anchors.rightMargin: 16
            spacing: 10

            Item { Layout.fillWidth: true }

            Button {
                text: "Annulla"
                enabled: !(root.homeHubVm && root.homeHubVm.isCloning)
                onClicked: root.close()
                contentItem: Text {
                    text: "Annulla"
                    color: "#94A3B8"
                    font.pixelSize: 13
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
                background: Rectangle {
                    color: parent.hovered ? "#334155" : "transparent"
                    radius: 6
                }
            }

            Button {
                id: cloneButton
                text: "⬇ Avvia Clonazione"
                enabled: !(root.homeHubVm && root.homeHubVm.isCloning) &&
                         urlField.text.trim().length > 0 &&
                         destFolderField.text.trim().length > 0
                onClicked: {
                    if (root.homeHubVm) {
                        root.homeHubVm.cloneRepositoryAsync(
                            urlField.text,
                            destFolderField.text,
                            dirNameField.text,
                            displayNameField.text
                        );
                    }
                }
                contentItem: Text {
                    text: (root.homeHubVm && root.homeHubVm.isCloning) ? "Clonazione in corso..." : "⬇ Avvia Clonazione"
                    color: cloneButton.enabled ? "#FFFFFF" : "#64748B"
                    font.pixelSize: 13
                    font.bold: true
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
                background: Rectangle {
                    color: cloneButton.enabled ? (cloneButton.hovered ? "#0284C7" : "#0369A1") : "#1E293B"
                    radius: 6
                }
            }
        }
    }
}
