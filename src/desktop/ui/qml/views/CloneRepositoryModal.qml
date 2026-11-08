import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs

Dialog {
    id: root
    title: "Clona Repository"
    modal: true
    width: 680
    anchors.centerIn: Overlay.overlay
    padding: 0
    topPadding: 0
    bottomPadding: 0
    leftPadding: 0
    rightPadding: 0

    property var homeHubVm: null
    property var profilesVm: null

    property bool showTokenConfig: false
    property bool showTokenText: false
    property string selectedRepoFullName: ""

    function openWithDefaultFolder(defaultParentFolder) {
        urlField.text = "";
        dirNameField.text = "";
        displayNameField.text = "";
        selectedRepoFullName = "";
        showTokenConfig = false;
        dirNameField.propertyModified = false;

        if (defaultParentFolder && defaultParentFolder.length > 0) {
            destFolderField.text = defaultParentFolder;
        }

        if (root.profilesVm) {
            root.profilesVm.selectProfile("github");
        }

        root.open();
        if (searchField.visible) {
            searchField.forceActiveFocus();
        } else {
            urlField.forceActiveFocus();
        }
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
        color: "#0B1120"
        radius: 8
        border.color: "#334155"
        border.width: 1
    }

    contentItem: ColumnLayout {
        id: mainModalLayout
        spacing: 0

        // ==========================================
        // 1. MODAL HEADER
        // ==========================================
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 48
            color: "#1E293B"
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
                    text: "⬇ Clona Repository Remoto (Stile GitKraken)"
                    color: "#F8FAFC"
                    font.pixelSize: 15
                    font.bold: true
                    Layout.fillWidth: true
                }

                Button {
                    text: "✕"
                    flat: true
                    Layout.preferredWidth: 28
                    Layout.preferredHeight: 28
                    enabled: !(root.homeHubVm && root.homeHubVm.isCloning)
                    onClicked: root.close()
                    contentItem: Text {
                        text: "✕"
                        color: parent.hovered ? "#EF4444" : "#94A3B8"
                        font.pixelSize: 14
                        font.bold: true
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                    background: Item {}
                }
            }
        }

        // ==========================================
        // 2. PROVIDER SELECTOR TABS
        // ==========================================
        Rectangle {
            Layout.fillWidth: true
            height: 44
            color: "#0F172A"
            border.color: "#1E293B"
            border.width: 1

            ScrollView {
                anchors.fill: parent
                ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
                ScrollBar.vertical.policy: ScrollBar.AlwaysOff

                RowLayout {
                    spacing: 8
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.left: parent.left
                    anchors.leftMargin: 14

                    Repeater {
                        model: root.profilesVm ? root.profilesVm.profiles : []
                        delegate: Rectangle {
                            id: tabRect
                            implicitHeight: 32
                            implicitWidth: tabRow.implicitWidth + 24
                            radius: 6

                            readonly property bool isSelected: root.profilesVm && (root.profilesVm.activeProfileId === modelData.id)

                            color: isSelected ? "#0369A1" : (tabMouse.containsMouse ? "#1E293B" : "#0B1120")
                            border.color: isSelected ? "#38BDF8" : (tabMouse.containsMouse ? "#475569" : "#1E293B")
                            border.width: 1

                            RowLayout {
                                id: tabRow
                                anchors.centerIn: parent
                                spacing: 6

                                Text {
                                    text: modelData.icon
                                    font.pixelSize: 13
                                }

                                Text {
                                    text: modelData.displayName
                                    color: tabRect.isSelected ? "#FFFFFF" : (tabMouse.containsMouse ? "#F8FAFC" : "#94A3B8")
                                    font.pixelSize: 12
                                    font.bold: tabRect.isSelected
                                }

                                // Status dot (Green if configured/SSH, gray if not)
                                Rectangle {
                                    visible: modelData.id !== "url"
                                    width: 6
                                    height: 6
                                    radius: 3
                                    color: (modelData.hasSsh || modelData.hasToken) ? "#10B981" : "#475569"
                                }
                            }

                            MouseArea {
                                id: tabMouse
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: {
                                    if (root.profilesVm) {
                                        root.profilesVm.selectProfile(modelData.id);
                                        root.selectedRepoFullName = "";
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }

        // ==========================================
        // 3. FORM BODY
        // ==========================================
        Item {
            Layout.fillWidth: true
            implicitHeight: formLayout.implicitHeight + 28

            ColumnLayout {
                id: formLayout
                anchors.fill: parent
                anchors.leftMargin: 16
                anchors.rightMargin: 16
                anchors.topMargin: 14
                anchors.bottomMargin: 14
                spacing: 12

                // ---------------------------------------------------------
                // A. PROVIDER SUB-HEADER (When a Git provider is selected)
                // ---------------------------------------------------------
                Rectangle {
                    Layout.fillWidth: true
                    height: 40
                    visible: root.profilesVm && root.profilesVm.activeProfileId !== "url"
                    color: "#131C2E"
                    radius: 6
                    border.color: "#1E293B"
                    border.width: 1

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 12
                        anchors.rightMargin: 12
                        spacing: 10

                        // Profile Title & Account Name
                        Text {
                            text: (root.profilesVm ? root.profilesVm.activeProfileDisplayName : "") +
                                  (root.profilesVm && root.profilesVm.activeProfileUsername.length > 0
                                   ? " • @" + root.profilesVm.activeProfileUsername : "")
                            color: "#F8FAFC"
                            font.bold: true
                            font.pixelSize: 12
                        }

                        // SSH Status Badge
                        Rectangle {
                            visible: root.profilesVm && root.profilesVm.activeProfileHasSsh
                            implicitHeight: 22
                            implicitWidth: sshText.implicitWidth + 14
                            color: "#064E3B"
                            radius: 4
                            border.color: "#059669"
                            border.width: 1
                            Text {
                                id: sshText
                                anchors.centerIn: parent
                                text: "🔑 SSH Attivo"
                                color: "#A7F3D0"
                                font.pixelSize: 11
                                font.bold: true
                            }
                        }

                        // Token Status Badge
                        Rectangle {
                            visible: root.profilesVm && root.profilesVm.activeProfileHasToken
                            implicitHeight: 22
                            implicitWidth: tokenText.implicitWidth + 14
                            color: "#1E3A8A"
                            radius: 4
                            border.color: "#3B82F6"
                            border.width: 1
                            Text {
                                id: tokenText
                                anchors.centerIn: parent
                                text: "🔒 Token Cifrato (Argon2id)"
                                color: "#BFDBFE"
                                font.pixelSize: 11
                                font.bold: true
                            }
                        }

                        Item { Layout.fillWidth: true }

                        // Protocol Switcher (SSH vs HTTPS)
                        RowLayout {
                            spacing: 2
                            Text {
                                text: "Protocollo:"
                                color: "#94A3B8"
                                font.pixelSize: 11
                            }

                            Rectangle {
                                width: 44
                                height: 26
                                radius: 4
                                color: (root.profilesVm && root.profilesVm.selectedProtocol === "ssh") ? "#0284C7" : "#0F172A"
                                border.color: (root.profilesVm && root.profilesVm.selectedProtocol === "ssh") ? "#38BDF8" : "#334155"
                                Text {
                                    anchors.centerIn: parent
                                    text: "SSH"
                                    color: (root.profilesVm && root.profilesVm.selectedProtocol === "ssh") ? "#FFFFFF" : "#94A3B8"
                                    font.pixelSize: 11
                                    font.bold: true
                                }
                                MouseArea {
                                    anchors.fill: parent
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: if (root.profilesVm) root.profilesVm.setSelectedProtocol("ssh")
                                }
                            }

                            Rectangle {
                                width: 52
                                height: 26
                                radius: 4
                                color: (root.profilesVm && root.profilesVm.selectedProtocol === "https") ? "#0284C7" : "#0F172A"
                                border.color: (root.profilesVm && root.profilesVm.selectedProtocol === "https") ? "#38BDF8" : "#334155"
                                Text {
                                    anchors.centerIn: parent
                                    text: "HTTPS"
                                    color: (root.profilesVm && root.profilesVm.selectedProtocol === "https") ? "#FFFFFF" : "#94A3B8"
                                    font.pixelSize: 11
                                    font.bold: true
                                }
                                MouseArea {
                                    anchors.fill: parent
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: if (root.profilesVm) root.profilesVm.setSelectedProtocol("https")
                                }
                            }
                        }

                        // Refresh Button
                        Button {
                            text: "🔄"
                            Layout.preferredHeight: 26
                            Layout.preferredWidth: 28
                            onClicked: if (root.profilesVm) root.profilesVm.refreshRepositories()
                            contentItem: Text {
                                text: "🔄"
                                font.pixelSize: 12
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                            }
                            background: Rectangle {
                                color: parent.hovered ? "#334155" : "#1E293B"
                                radius: 4
                            }
                        }

                        // Config Token Button
                        Button {
                            text: root.showTokenConfig ? "Nascondi" : (root.profilesVm && root.profilesVm.activeProfileHasToken ? "Modifica Token" : "+ Inserisci Token")
                            Layout.preferredHeight: 26
                            onClicked: root.showTokenConfig = !root.showTokenConfig
                            contentItem: Text {
                                text: parent.text
                                color: root.showTokenConfig ? "#38BDF8" : "#94A3B8"
                                font.pixelSize: 11
                                font.bold: true
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                            }
                            background: Rectangle {
                                color: parent.hovered ? "#334155" : "#1E293B"
                                radius: 4
                                border.color: root.showTokenConfig ? "#38BDF8" : "#334155"
                                border.width: 1
                            }
                        }
                    }
                }

                // ---------------------------------------------------------
                // B. EXPANDABLE TOKEN CONFIGURATION CARD (Argon2id + AES)
                // ---------------------------------------------------------
                Rectangle {
                    Layout.fillWidth: true
                    visible: root.showTokenConfig && root.profilesVm && root.profilesVm.activeProfileId !== "url"
                    implicitHeight: tokenConfigCol.implicitHeight + 20
                    color: "#080D1A"
                    radius: 6
                    border.color: "#38BDF8"
                    border.width: 1

                    ColumnLayout {
                        id: tokenConfigCol
                        anchors.fill: parent
                        anchors.margins: 10
                        spacing: 8

                        Text {
                            text: "🔒 Configura Personal Access Token (PAT) per " + (root.profilesVm ? root.profilesVm.activeProfileDisplayName : "")
                            color: "#F8FAFC"
                            font.bold: true
                            font.pixelSize: 12
                        }

                        Text {
                            text: "Il token viene salvato nel database locale cifrato con Argon2id (KDF) e AES-256-GCM. Sblocca repository privati e delle organizzazioni."
                            color: "#94A3B8"
                            font.pixelSize: 11
                            wrapMode: Text.Wrap
                            Layout.fillWidth: true
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 8

                            TextField {
                                id: tokenUsernameInput
                                Layout.preferredWidth: 160
                                Layout.preferredHeight: 32
                                placeholderText: "Username (es. Dokjolly0)"
                                placeholderTextColor: "#475569"
                                text: root.profilesVm ? root.profilesVm.activeProfileUsername : ""
                                color: "#F8FAFC"
                                font.pixelSize: 12
                                background: Rectangle {
                                    color: "#0B1120"
                                    radius: 4
                                    border.color: "#334155"
                                }
                            }

                            TextField {
                                id: tokenSecretInput
                                Layout.fillWidth: true
                                Layout.preferredHeight: 32
                                placeholderText: "Token (ghp_... o API token)"
                                placeholderTextColor: "#475569"
                                echoMode: root.showTokenText ? TextInput.Normal : TextInput.Password
                                color: "#F8FAFC"
                                font.pixelSize: 12
                                background: Rectangle {
                                    color: "#0B1120"
                                    radius: 4
                                    border.color: "#334155"
                                }
                            }

                            Button {
                                text: root.showTokenText ? "🙈" : "👁️"
                                Layout.preferredWidth: 32
                                Layout.preferredHeight: 32
                                onClicked: root.showTokenText = !root.showTokenText
                                background: Rectangle { color: "#1E293B"; radius: 4 }
                            }

                            Button {
                                text: "💾 Salva e Carica"
                                Layout.preferredHeight: 32
                                enabled: tokenSecretInput.text.trim().length > 0 || tokenUsernameInput.text.trim().length > 0
                                onClicked: {
                                    if (root.profilesVm) {
                                        root.profilesVm.saveProfileToken(
                                            root.profilesVm.activeProfileId,
                                            tokenSecretInput.text,
                                            tokenUsernameInput.text
                                        );
                                        tokenSecretInput.text = "";
                                        root.showTokenConfig = false;
                                    }
                                }
                                contentItem: Text {
                                    text: "💾 Salva e Carica"
                                    color: "#FFFFFF"
                                    font.bold: true
                                    font.pixelSize: 11
                                    horizontalAlignment: Text.AlignHCenter
                                    verticalAlignment: Text.AlignVCenter
                                }
                                background: Rectangle {
                                    color: parent.enabled ? (parent.hovered ? "#0284C7" : "#0369A1") : "#1E293B"
                                    radius: 4
                                }
                            }

                            Button {
                                visible: root.profilesVm && root.profilesVm.activeProfileHasToken
                                text: "🗑 Rimuovi"
                                Layout.preferredHeight: 32
                                onClicked: {
                                    if (root.profilesVm) {
                                        root.profilesVm.clearProfileToken(root.profilesVm.activeProfileId);
                                        root.showTokenConfig = false;
                                    }
                                }
                                contentItem: Text {
                                    text: "🗑 Rimuovi"
                                    color: "#EF4444"
                                    font.bold: true
                                    font.pixelSize: 11
                                    horizontalAlignment: Text.AlignHCenter
                                    verticalAlignment: Text.AlignVCenter
                                }
                                background: Rectangle {
                                    color: parent.hovered ? "#450A0A" : "#1E293B"
                                    radius: 4
                                    border.color: "#DC2626"
                                    border.width: 1
                                }
                            }
                        }
                    }
                }

                // ---------------------------------------------------------
                // C. REPOSITORY SEARCH & LIST (When Provider is selected)
                // ---------------------------------------------------------
                ColumnLayout {
                    Layout.fillWidth: true
                    visible: root.profilesVm && root.profilesVm.activeProfileId !== "url"
                    spacing: 6

                    // Search Bar & Counter
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 8

                        TextField {
                            id: searchField
                            Layout.fillWidth: true
                            Layout.preferredHeight: 32
                            placeholderText: "🔍 Cerca tra i tuoi repository (es. Git-Flux, template, app)..."
                            placeholderTextColor: "#475569"
                            color: "#F8FAFC"
                            font.pixelSize: 12
                            selectByMouse: true
                            background: Rectangle {
                                color: "#0F172A"
                                radius: 6
                                border.color: searchField.activeFocus ? "#38BDF8" : "#334155"
                                border.width: 1
                            }
                            onTextChanged: {
                                if (root.profilesVm && root.profilesVm.remoteReposModel) {
                                    root.profilesVm.remoteReposModel.searchFilter = text;
                                }
                            }
                        }

                        // Clear search button
                        Button {
                            visible: searchField.text.length > 0
                            text: "✕"
                            Layout.preferredWidth: 32
                            Layout.preferredHeight: 32
                            onClicked: searchField.text = ""
                            background: Rectangle { color: "#1E293B"; radius: 4 }
                            contentItem: Text {
                                text: "✕"
                                color: "#94A3B8"
                                font.pixelSize: 11
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                            }
                        }

                        // Counter Badge
                        Rectangle {
                            implicitHeight: 32
                            implicitWidth: countText.implicitWidth + 16
                            color: "#1E293B"
                            radius: 6
                            border.color: "#334155"
                            Text {
                                id: countText
                                anchors.centerIn: parent
                                text: {
                                    if (!root.profilesVm || !root.profilesVm.remoteReposModel) return "0 repo";
                                    var filtered = root.profilesVm.remoteReposModel.filteredCount;
                                    var total = root.profilesVm.remoteReposModel.totalCount;
                                    return filtered + " di " + total + " repo";
                                }
                                color: "#38BDF8"
                                font.pixelSize: 11
                                font.bold: true
                            }
                        }
                    }

                    // Loading State
                    Rectangle {
                        Layout.fillWidth: true
                        height: 38
                        visible: root.profilesVm && root.profilesVm.isFetching
                        color: "#0F172A"
                        radius: 6
                        RowLayout {
                            anchors.centerIn: parent
                            spacing: 8
                            ProgressBar { indeterminate: true; Layout.preferredWidth: 100 }
                            Text {
                                text: "Caricamento repository in corso..."
                                color: "#38BDF8"
                                font.pixelSize: 12
                            }
                        }
                    }

                    // Fetch Error Banner
                    Rectangle {
                        Layout.fillWidth: true
                        visible: root.profilesVm && root.profilesVm.fetchError.length > 0 && !root.profilesVm.isFetching
                        implicitHeight: fetchErrText.implicitHeight + 16
                        color: "#450A0A"
                        radius: 6
                        border.color: "#DC2626"
                        border.width: 1
                        RowLayout {
                            anchors.fill: parent
                            anchors.margins: 8
                            spacing: 8
                            Text {
                                id: fetchErrText
                                text: root.profilesVm ? root.profilesVm.fetchError : ""
                                color: "#FECACA"
                                font.pixelSize: 11
                                wrapMode: Text.Wrap
                                Layout.fillWidth: true
                            }
                            Button {
                                text: "Inserisci Token"
                                Layout.preferredHeight: 24
                                onClicked: root.showTokenConfig = true
                                background: Rectangle { color: "#7F1D1D"; radius: 4 }
                                contentItem: Text { text: "Inserisci Token"; color: "#FFFFFF"; font.pixelSize: 10; font.bold: true }
                            }
                        }
                    }

                    // Scrollable Remote Repository List View
                    Rectangle {
                        Layout.fillWidth: true
                        height: 180
                        color: "#070B14"
                        radius: 6
                        border.color: "#1E293B"
                        border.width: 1
                        clip: true

                        ListView {
                            id: repoListView
                            anchors.fill: parent
                            anchors.margins: 4
                            spacing: 4
                            model: root.profilesVm ? root.profilesVm.remoteReposModel : null
                            ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

                            delegate: Rectangle {
                                id: repoCard
                                width: repoListView.width - 8
                                height: 50
                                radius: 5

                                readonly property bool isSelected: root.selectedRepoFullName === model.fullName

                                color: isSelected ? "#0F2942" : (cardMouse.containsMouse ? "#1E293B" : "#0F172A")
                                border.color: isSelected ? "#38BDF8" : (cardMouse.containsMouse ? "#475569" : "#1E293B")
                                border.width: isSelected ? 2 : 1

                                RowLayout {
                                    anchors.fill: parent
                                    anchors.leftMargin: 10
                                    anchors.rightMargin: 10
                                    spacing: 8

                                    // Privacy Icon (Lock or Globe)
                                    Text {
                                        text: model.isPrivate ? "🔒" : "🌐"
                                        font.pixelSize: 13
                                    }

                                    // Repository Details
                                    ColumnLayout {
                                        Layout.fillWidth: true
                                        spacing: 1

                                        RowLayout {
                                            spacing: 6
                                            Text {
                                                text: model.fullName
                                                color: repoCard.isSelected ? "#38BDF8" : "#F8FAFC"
                                                font.pixelSize: 12
                                                font.bold: true
                                                elide: Text.ElideRight
                                            }

                                            Text {
                                                visible: model.starsCount > 0
                                                text: "⭐ " + model.starsCount
                                                color: "#FBBF24"
                                                font.pixelSize: 10
                                            }
                                        }

                                        Text {
                                            text: model.description.length > 0 ? model.description : (model.activeCloneUrl)
                                            color: "#94A3B8"
                                            font.pixelSize: 11
                                            elide: Text.ElideRight
                                            Layout.fillWidth: true
                                        }
                                    }

                                    // Selection Indicator Checkmark
                                    Text {
                                        visible: repoCard.isSelected
                                        text: "✔"
                                        color: "#38BDF8"
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
                                        root.selectedRepoFullName = model.fullName;
                                        urlField.text = model.activeCloneUrl;
                                        dirNameField.text = model.name;
                                        dirNameField.propertyModified = true;
                                    }
                                }
                            }

                            // Empty list placeholder
                            Text {
                                anchors.centerIn: parent
                                visible: repoListView.count === 0 && (!root.profilesVm || !root.profilesVm.isFetching)
                                text: searchField.text.length > 0
                                      ? "Nessun repository trovato per '" + searchField.text + "'"
                                      : "Nessun repository disponibile per questo account."
                                color: "#64748B"
                                font.pixelSize: 12
                            }
                        }
                    }
                }

                // ---------------------------------------------------------
                // D. DIRECT URL INPUT (Visible in "URL Diretto" mode or as editable field)
                // ---------------------------------------------------------
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 4

                    Text {
                        text: (root.profilesVm && root.profilesVm.activeProfileId !== "url")
                              ? "URL Repository Selezionato:"
                              : "URL Repository Remoto (HTTPS o SSH):"
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
                        selectByMouse: true
                        background: Rectangle {
                            color: "#0B1120"
                            radius: 6
                            border.color: urlField.activeFocus ? "#38BDF8" : "#334155"
                            border.width: 1
                        }
                        onTextChanged: {
                            if (root.homeHubVm && text.length > 0 && root.profilesVm && root.profilesVm.activeProfileId === "url") {
                                var extracted = root.homeHubVm.extractRepoNameFromUrl(text);
                                if (dirNameField.text.length === 0 || dirNameField.propertyModified !== true) {
                                    dirNameField.text = extracted;
                                }
                            }
                        }
                        onAccepted: {
                            if (cloneButton.enabled) cloneButton.clicked();
                        }
                    }
                }

                // ---------------------------------------------------------
                // E. LOCAL DESTINATION FOLDER
                // ---------------------------------------------------------
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
                            selectByMouse: true
                            background: Rectangle {
                                color: "#0B1120"
                                radius: 6
                                border.color: destFolderField.activeFocus ? "#38BDF8" : "#334155"
                                border.width: 1
                            }
                            onAccepted: {
                                if (cloneButton.enabled) cloneButton.clicked();
                            }
                        }

                        Button {
                            text: "Sfoglia..."
                            Layout.preferredHeight: 34
                            Layout.preferredWidth: 84
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

                // ---------------------------------------------------------
                // F. DIRECTORY NAME & CATALOG NAME
                // ---------------------------------------------------------
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 12

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 4

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
                            selectByMouse: true
                            background: Rectangle {
                                color: "#0B1120"
                                radius: 6
                                border.color: dirNameField.activeFocus ? "#38BDF8" : "#334155"
                                border.width: 1
                            }
                            onTextEdited: propertyModified = true
                            onAccepted: {
                                if (cloneButton.enabled) cloneButton.clicked();
                            }
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
                            selectByMouse: true
                            background: Rectangle {
                                color: "#0B1120"
                                radius: 6
                                border.color: displayNameField.activeFocus ? "#38BDF8" : "#334155"
                                border.width: 1
                            }
                            onAccepted: {
                                if (cloneButton.enabled) cloneButton.clicked();
                            }
                        }
                    }
                }

                // Destination Path Preview
                Text {
                    visible: destFolderField.text.trim().length > 0 && dirNameField.text.trim().length > 0
                    text: "📁 Salvataggio in: " + destFolderField.text.trim() + "/" + dirNameField.text.trim()
                    color: "#38BDF8"
                    font.pixelSize: 11
                    font.bold: true
                    elide: Text.ElideMiddle
                    Layout.fillWidth: true
                }

                // Progress Indicator
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

                // Error Banner
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
        }

        // ==========================================
        // 4. MODAL FOOTER
        // ==========================================
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 52
            color: "#1E293B"
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
                    Layout.preferredHeight: 34
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
                    Layout.preferredHeight: 34
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
}
