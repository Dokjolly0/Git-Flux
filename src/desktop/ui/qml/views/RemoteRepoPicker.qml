import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../themes"

ColumnLayout {
    id: pickerRoot
    readonly property FluxTheme theme: FluxTheme {}

    property var profilesVm: null
    property string selectedRepoFullName: ""
    property bool selectedRepoIsPrivate: false
    property string selectedUrlText: ""
    property bool showRepoPicker: false
    property bool repoTouched: false

    signal togglePickerRequested()
    signal openPickerRequested()
    signal repoChosen(string fullName, bool isPrivate, string cloneUrl, string defaultDirName)
    signal insertTokenRequested()

    function clearSearch() {
        searchField.text = "";
    }

    function focusSearch() {
        searchField.forceActiveFocus();
    }

    function activeUrlForRepo(fullName) {
        if (!profilesVm || !fullName || fullName.length === 0) return "";
        for (let i = 0; i < repoListView.count; ++i) {
            const r = profilesVm.remoteReposModel.getRepoAt(i);
            if (r.fullName === fullName) {
                return r.activeCloneUrl;
            }
        }
        return "";
    }

    spacing: 6

    Text {
        text: "Repository da Clonare:"
        color: pickerRoot.theme.textMuted
        font.pixelSize: 12
        font.bold: true
    }

    // Compact Clickable Selector Box
    Rectangle {
        Layout.fillWidth: true
        height: 42
        radius: 6
        color: repoSelectorMouse.containsMouse ? pickerRoot.theme.cardHover : pickerRoot.theme.inputBg
        border.color: (pickerRoot.repoTouched && !pickerRoot.showRepoPicker && pickerRoot.selectedRepoFullName.length === 0)
            ? pickerRoot.theme.warning
            : (pickerRoot.showRepoPicker ? pickerRoot.theme.accentCyan : (pickerRoot.selectedRepoFullName.length > 0 ? pickerRoot.theme.accentBlue : pickerRoot.theme.border))
        border.width: (pickerRoot.showRepoPicker || (pickerRoot.repoTouched && !pickerRoot.showRepoPicker && pickerRoot.selectedRepoFullName.length === 0)) ? 2 : 1

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 12
            anchors.rightMargin: 12
            spacing: 10

            Text {
                text: pickerRoot.selectedRepoFullName.length > 0 ? (pickerRoot.selectedRepoIsPrivate ? "🔒" : "🌐") : "📂"
                font.pixelSize: 16
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 1

                Text {
                    text: pickerRoot.selectedRepoFullName.length > 0
                        ? pickerRoot.selectedRepoFullName
                        : "Clicca per selezionare un repository da " + (pickerRoot.profilesVm ? pickerRoot.profilesVm.activeProfileDisplayName : "server") + "..."
                    color: pickerRoot.selectedRepoFullName.length > 0 ? pickerRoot.theme.textPrimary : pickerRoot.theme.textMuted
                    font.pixelSize: 12
                    font.bold: pickerRoot.selectedRepoFullName.length > 0
                    elide: Text.ElideRight
                    Layout.fillWidth: true
                }

                Text {
                    visible: pickerRoot.selectedRepoFullName.length > 0
                    text: pickerRoot.selectedUrlText
                    color: pickerRoot.theme.accentCyan
                    font.pixelSize: 10
                    elide: Text.ElideRight
                    Layout.fillWidth: true
                }
            }

            Rectangle {
                implicitHeight: 24
                implicitWidth: actionBtnText.implicitWidth + 14
                radius: 4
                color: pickerRoot.showRepoPicker ? pickerRoot.theme.accentBlue : (repoSelectorMouse.containsMouse ? pickerRoot.theme.borderHighlight : pickerRoot.theme.surfaceElevated)
                border.color: pickerRoot.showRepoPicker ? pickerRoot.theme.accentCyan : pickerRoot.theme.borderHighlight
                Text {
                    id: actionBtnText
                    anchors.centerIn: parent
                    text: pickerRoot.showRepoPicker
                        ? "Chiudi ▲"
                        : (pickerRoot.selectedRepoFullName.length > 0 ? "Cambia ▼" : "Scegli ▼")
                    color: pickerRoot.showRepoPicker ? pickerRoot.theme.textPrimary : pickerRoot.theme.textMuted
                    font.pixelSize: 11
                    font.bold: true
                }
            }
        }

        MouseArea {
            id: repoSelectorMouse
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: {
                pickerRoot.togglePickerRequested();
                if (pickerRoot.showRepoPicker) searchField.forceActiveFocus();
            }
        }
    }

    // Warning if closed without selecting
    Rectangle {
        Layout.fillWidth: true
        Layout.preferredHeight: 32
        visible: pickerRoot.repoTouched && !pickerRoot.showRepoPicker && pickerRoot.selectedRepoFullName.length === 0 && Boolean(pickerRoot.profilesVm && pickerRoot.profilesVm.activeProfileId !== "url")
        color: pickerRoot.theme.warningBg
        radius: 6
        border.color: pickerRoot.theme.warning
        border.width: 1

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 10
            anchors.rightMargin: 10
            spacing: 8

            Text {
                text: "⚠️ Nessun repository selezionato. Clicca sul riquadro sopra per sceglierne uno."
                color: pickerRoot.theme.warningText
                font.pixelSize: 11
                font.bold: true
                Layout.fillWidth: true
            }

            Button {
                Layout.preferredHeight: 22
                onClicked: {
                    pickerRoot.openPickerRequested();
                    searchField.forceActiveFocus();
                }
                contentItem: Text {
                    text: "Scegli ▼"; color: pickerRoot.theme.warningText; font.pixelSize: 10; font.bold: true
                }
                background: Rectangle {
                    color: pickerRoot.theme.warning; opacity: 0.35; radius: 4
                }
            }
        }
    }

    // Expandable Dropdown Drawer
    Rectangle {
        Layout.fillWidth: true
        visible: pickerRoot.showRepoPicker
        implicitHeight: repoPickerLayout.implicitHeight + 16
        color: pickerRoot.theme.canvas
        radius: 6
        border.color: pickerRoot.theme.accentCyan
        border.width: 1

        onVisibleChanged: {
            if (visible) searchField.forceActiveFocus();
        }

        ColumnLayout {
            id: repoPickerLayout
            anchors.fill: parent
            anchors.margins: 8
            spacing: 6

            RowLayout {
                Layout.fillWidth: true
                spacing: 8

                FluxTextField {
                    id: searchField
                    Layout.fillWidth: true
                    Layout.preferredHeight: 32
                    placeholderText: "🔍 Cerca tra i tuoi repository (es. FinancialManager, FileHub)..."
                    showClearButton: true
                    onTextChanged: {
                        if (pickerRoot.profilesVm && pickerRoot.profilesVm.remoteReposModel) {
                            pickerRoot.profilesVm.remoteReposModel.searchFilter = text;
                        }
                    }
                }

                Rectangle {
                    implicitHeight: 32
                    implicitWidth: countText.implicitWidth + 16
                    color: pickerRoot.theme.surfaceElevated
                    radius: 6
                    border.color: pickerRoot.theme.border
                    Text {
                        id: countText
                        anchors.centerIn: parent
                        text: {
                            if (!pickerRoot.profilesVm || !pickerRoot.profilesVm.remoteReposModel) return "0 repo";
                            const filtered = pickerRoot.profilesVm.remoteReposModel.filteredCount;
                            const total = pickerRoot.profilesVm.remoteReposModel.totalCount;
                            return filtered + " di " + total + " repo";
                        }
                        color: pickerRoot.theme.accentCyan
                        font.pixelSize: 11
                        font.bold: true
                    }
                }
            }

            Rectangle {
                Layout.fillWidth: true
                height: 36
                visible: Boolean(pickerRoot.profilesVm && pickerRoot.profilesVm.isFetching)
                color: pickerRoot.theme.surface
                radius: 6
                RowLayout {
                    anchors.centerIn: parent
                    spacing: 8
                    ProgressBar { indeterminate: true; Layout.preferredWidth: 100 }
                    Text {
                        text: "Caricamento repository in corso..."
                        color: pickerRoot.theme.accentCyan
                        font.pixelSize: 12
                    }
                }
            }

            Rectangle {
                Layout.fillWidth: true
                visible: Boolean(pickerRoot.profilesVm && pickerRoot.profilesVm.fetchError.length > 0 && !pickerRoot.profilesVm.isFetching)
                implicitHeight: fetchErrText.implicitHeight + 16
                color: pickerRoot.theme.dangerBg
                radius: 6
                border.color: pickerRoot.theme.danger
                border.width: 1
                RowLayout {
                    anchors.fill: parent
                    anchors.margins: 8
                    spacing: 8
                    Text {
                        id: fetchErrText
                        text: pickerRoot.profilesVm ? pickerRoot.profilesVm.fetchError : ""
                        color: pickerRoot.theme.dangerText
                        font.pixelSize: 11
                        wrapMode: Text.Wrap
                        Layout.fillWidth: true
                    }
                    FluxActionButton {
                        text: "Inserisci Token"
                        variant: "danger"
                        Layout.preferredHeight: 24
                        onClicked: pickerRoot.insertTokenRequested()
                    }
                }
            }

            Rectangle {
                Layout.fillWidth: true
                height: 190
                color: pickerRoot.theme.inputBg
                radius: 6
                border.color: pickerRoot.theme.borderSubtle
                border.width: 1
                clip: true

                ListView {
                    id: repoListView
                    SmoothWheelScroller {}
                    anchors.fill: parent
                    anchors.margins: 4
                    spacing: 4
                    model: pickerRoot.profilesVm ? pickerRoot.profilesVm.remoteReposModel : null
                    boundsBehavior: Flickable.StopAtBounds
                    maximumFlickVelocity: 6000
                    flickDeceleration: 1000
                    ScrollBar.vertical: FluxScrollBar {}

                    delegate: Rectangle {
                        id: repoCard
                        width: repoListView.width - 8
                        height: 50
                        radius: 5

                        readonly property bool isSelected: pickerRoot.selectedRepoFullName === model.fullName

                        color: isSelected ? pickerRoot.theme.infoBg : (cardMouse.containsMouse ? pickerRoot.theme.cardHover : pickerRoot.theme.surface)
                        border.color: isSelected ? pickerRoot.theme.accentCyan : (cardMouse.containsMouse ? pickerRoot.theme.borderHighlight : pickerRoot.theme.borderSubtle)
                        border.width: isSelected ? 2 : 1

                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 10
                            anchors.rightMargin: 10
                            spacing: 8

                            Text {
                                text: model.isPrivate ? "🔒" : "🌐"
                                font.pixelSize: 13
                            }

                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 1

                                RowLayout {
                                    spacing: 6
                                    Text {
                                        text: model.fullName
                                        color: repoCard.isSelected ? pickerRoot.theme.accentCyan : pickerRoot.theme.textPrimary
                                        font.pixelSize: 12
                                        font.bold: true
                                        elide: Text.ElideRight
                                    }

                                    Text {
                                        visible: model.starsCount > 0
                                        text: "⭐ " + model.starsCount
                                        color: pickerRoot.theme.warning
                                        font.pixelSize: 10
                                    }
                                }

                                Text {
                                    text: model.description.length > 0 ? model.description : (model.activeCloneUrl)
                                    color: pickerRoot.theme.textMuted
                                    font.pixelSize: 11
                                    elide: Text.ElideRight
                                    Layout.fillWidth: true
                                }
                            }

                            Text {
                                visible: repoCard.isSelected
                                text: "✔"
                                color: pickerRoot.theme.accentCyan
                                font.pixelSize: 14
                                font.bold: true
                            }
                        }

                        MouseArea {
                            id: cardMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                let targetUrl = model.activeCloneUrl;
                                if (!targetUrl || targetUrl.length === 0) {
                                    targetUrl = (pickerRoot.profilesVm && pickerRoot.profilesVm.selectedProtocol === "ssh") ? model.sshUrl : model.httpsUrl;
                                }
                                if (!targetUrl || targetUrl.length === 0) {
                                    targetUrl = model.httpsUrl ? model.httpsUrl : model.sshUrl;
                                }
                                const defaultName = model.fullName ? model.fullName.split("/").pop() : model.name;
                                pickerRoot.repoChosen(model.fullName, model.isPrivate, targetUrl, defaultName);
                            }
                        }
                    }

                    Text {
                        anchors.centerIn: parent
                        visible: repoListView.count === 0 && (!pickerRoot.profilesVm || !pickerRoot.profilesVm.isFetching)
                        text: searchField.text.length > 0
                            ? "Nessun repository trovato per '" + searchField.text + "'"
                            : "Nessun repository disponibile per questo account."
                        color: pickerRoot.theme.textMuted
                        font.pixelSize: 12
                    }
                }
            }
        }
    }
}
