import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../themes"

Dialog {
    id: fileHistoryModal
    readonly property FluxTheme theme: FluxTheme {}

    property var branchManager: null
    property var allRepoFiles: []
    property var filteredRepoFiles: []

    signal fileSelected(string filePath)

    title: "Cronologia File (File History)"
    modal: true
    width: 580
    height: 480
    anchors.centerIn: Overlay.overlay

    function getDirectory(path) {
        if (!path) return "";
        const lastSlash = Math.max(path.lastIndexOf('/'), path.lastIndexOf('\\'));
        if (lastSlash === -1) return "";
        return path.substring(0, lastSlash + 1);
    }

    function getFilename(path) {
        if (!path) return "";
        const lastSlash = Math.max(path.lastIndexOf('/'), path.lastIndexOf('\\'));
        if (lastSlash === -1) return path;
        return path.substring(lastSlash + 1);
    }

    function openModal() {
        // Asynchronous: the list fills in when `git ls-files` completes (onRepositoryFilesLoaded).
        allRepoFiles = [];
        fileHistorySearchField.text = "";
        applyFilter("");
        if (fileHistoryModal.branchManager) fileHistoryModal.branchManager.requestRepositoryFiles();
        open();
        fileHistorySearchField.forceActiveFocus();
    }

    Connections {
        target: fileHistoryModal.branchManager
        enabled: fileHistoryModal.visible

        function onRepositoryFilesLoaded(files) {
            fileHistoryModal.allRepoFiles = files || [];
            fileHistoryModal.applyFilter(fileHistorySearchField.text);
        }
    }

    function applyFilter(query) {
        const q = query.trim().toLowerCase();
        if (q === "") {
            filteredRepoFiles = allRepoFiles.slice(0, 200);
        } else {
            const res = [];
            for (let i = 0; i < allRepoFiles.length && res.length < 200; ++i) {
                const f = allRepoFiles[i];
                if (f.toLowerCase().indexOf(q) !== -1) {
                    res.push(f);
                }
            }
            filteredRepoFiles = res;
        }
        if (fileHistoryResultsList.count > 0) {
            fileHistoryResultsList.currentIndex = 0;
        } else {
            fileHistoryResultsList.currentIndex = -1;
        }
    }

    function confirmSelection() {
        let path = "";
        if (fileHistoryResultsList.currentIndex >= 0 && fileHistoryResultsList.currentIndex < filteredRepoFiles.length) {
            path = filteredRepoFiles[fileHistoryResultsList.currentIndex];
        } else if (fileHistorySearchField.text.trim().length > 0) {
            path = fileHistorySearchField.text.trim();
        }
        if (path !== "") {
            fileHistoryModal.fileSelected(path);
        }
    }

    background: FluxModalBackground {}

    header: FluxModalHeader {
        titleText: "📄 Cronologia File (File History) • Ctrl+H"
        onCloseRequested: fileHistoryModal.close()
    }

    contentItem: ColumnLayout {
        spacing: 10

        RowLayout {
            Layout.fillWidth: true
            Text {
                text: "Seleziona o cerca un file per esaminarne l'intera storia dei commit e visualizzarne i diff:"
                color: fileHistoryModal.theme.textMuted
                font.pixelSize: 11
                Layout.fillWidth: true
                elide: Text.ElideRight
            }
            Text {
                text: (fileHistoryModal.allRepoFiles.length) + " file tracciati"
                color: fileHistoryModal.theme.textMuted
                font.pixelSize: 11
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 36
            radius: 6
            color: fileHistoryModal.theme.inputBg
            border.color: fileHistorySearchField.activeFocus ? fileHistoryModal.theme.accentCyan : fileHistoryModal.theme.border
            border.width: 1

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 10
                anchors.rightMargin: 10
                spacing: 8

                Text {
                    text: "🔍"
                    font.pixelSize: 12
                    color: fileHistoryModal.theme.textMuted
                }

                TextField {
                    id: fileHistorySearchField
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    color: fileHistoryModal.theme.textPrimary
                    placeholderTextColor: fileHistoryModal.theme.textPlaceholder
                    font.pixelSize: 12
                    background: Item {
                    }
                    placeholderText: "Cerca percorso o nome file (es. main.cpp, src/core/...)..."
                    selectByMouse: true

                    onTextChanged: fileHistoryModal.applyFilter(text)

                    Keys.onDownPressed: function (event) {
                        if (fileHistoryResultsList.currentIndex < fileHistoryModal.filteredRepoFiles.length - 1) {
                            fileHistoryResultsList.currentIndex++;
                            fileHistoryResultsList.positionViewAtIndex(fileHistoryResultsList.currentIndex, ListView.Contain);
                        }
                        event.accepted = true;
                    }

                    Keys.onUpPressed: function (event) {
                        if (fileHistoryResultsList.currentIndex > 0) {
                            fileHistoryResultsList.currentIndex--;
                            fileHistoryResultsList.positionViewAtIndex(fileHistoryResultsList.currentIndex, ListView.Contain);
                        }
                        event.accepted = true;
                    }

                    Keys.onReturnPressed: function (event) {
                        fileHistoryModal.confirmSelection();
                        event.accepted = true;
                    }
                    Keys.onEnterPressed: function (event) {
                        fileHistoryModal.confirmSelection();
                        event.accepted = true;
                    }
                    Keys.onEscapePressed: function (event) {
                        fileHistoryModal.close();
                        event.accepted = true;
                    }
                }

                Button {
                    visible: fileHistorySearchField.text.length > 0
                    text: "✕"
                    flat: true
                    onClicked: fileHistorySearchField.text = ""
                    contentItem: Text {
                        text: "✕"
                        color: fileHistoryModal.theme.textMuted
                        font.pixelSize: 11
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                    background: Item {
                    }
                }
            }
        }

        // Results List
        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            radius: 6
            color: fileHistoryModal.theme.canvas
            border.color: fileHistoryModal.theme.borderSubtle
            border.width: 1
            clip: true

            ListView {
                id: fileHistoryResultsList
                SmoothWheelScroller {}
                anchors.fill: parent
                anchors.margins: 4
                model: fileHistoryModal.filteredRepoFiles
                spacing: 2
                boundsBehavior: Flickable.StopAtBounds
                ScrollBar.vertical: FluxScrollBar {
                }

                delegate: Rectangle {
                    id: fileItemRect
                    required property var modelData
                    required property int index
                    width: fileHistoryResultsList.width - 8
                    height: 32
                    radius: 4
                    property bool isCurrent: fileHistoryResultsList.currentIndex === index
                    color: isCurrent ? fileHistoryModal.theme.infoBg : (fileItemMouse.containsMouse ? fileHistoryModal.theme.cardHover : "transparent")
                    border.color: isCurrent ? fileHistoryModal.theme.accentCyan : "transparent"
                    border.width: 1

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 8
                        anchors.rightMargin: 8
                        spacing: 8

                        Text {
                            text: "📄"
                            font.pixelSize: 12
                        }

                        // Directory prefix (dimmed)
                        Text {
                            text: fileHistoryModal.getDirectory(fileItemRect.modelData)
                            color: fileHistoryModal.theme.textMuted
                            font.pixelSize: 11
                            elide: Text.ElideMiddle
                            Layout.maximumWidth: Math.floor(fileItemRect.width * 0.45)
                            visible: text !== ""
                        }

                        // Filename (highlighted)
                        Text {
                            text: fileHistoryModal.getFilename(fileItemRect.modelData)
                            color: fileItemRect.isCurrent ? fileHistoryModal.theme.accentCyan : fileHistoryModal.theme.textPrimary
                            font.bold: true
                            font.pixelSize: 11
                            elide: Text.ElideRight
                            Layout.fillWidth: true
                        }
                    }

                    MouseArea {
                        id: fileItemMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            fileHistoryResultsList.currentIndex = fileItemRect.index;
                            fileHistoryModal.confirmSelection();
                        }
                    }
                }

                Text {
                    anchors.centerIn: parent
                    text: "Nessun file trovato"
                    color: fileHistoryModal.theme.textMuted
                    font.pixelSize: 12
                    visible: fileHistoryModal.filteredRepoFiles.length === 0
                }
            }
        }
    }

    footer: FluxModalFooter {
        footerHeight: 52
        buttonSpacing: 10

        Text {
            text: "Suggerimento: usa i tasti ↑ ↓ per navigare e Invio per selezionare"
            color: fileHistoryModal.theme.textMuted
            font.pixelSize: 10
        }

        Item {
            Layout.fillWidth: true
        }

        FluxActionButton {
            text: "Annulla"
            Layout.preferredHeight: 32
            onClicked: fileHistoryModal.close()
        }

        FluxActionButton {
            text: "📄 Mostra Cronologia"
            variant: "primary"
            fontBold: true
            Layout.preferredHeight: 32
            leftPadding: 16
            rightPadding: 16
            enabled: (fileHistoryResultsList.currentIndex >= 0 && fileHistoryResultsList.currentIndex < fileHistoryModal.filteredRepoFiles.length) || fileHistorySearchField.text.trim().length > 0
            onClicked: fileHistoryModal.confirmSelection()
        }
    }
}
