import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs

Dialog {
    id: root
    title: "Clona Repository"
    modal: true
    width: 820
    anchors.centerIn: Overlay.overlay
    padding: 0
    topPadding: 0
    bottomPadding: 0
    leftPadding: 0
    rightPadding: 0

    property var hubVm: null
    readonly property var effectiveHubVm: root.hubVm !== null ? root.hubVm : ((typeof homeHubVm !== "undefined") ? homeHubVm : null)
    property var profilesVm: (typeof gitProfilesVm !== "undefined") ? gitProfilesVm : null

    property bool showTokenConfig: false
    property bool showTokenText: false
    property bool showRepoPicker: false
    property string selectedRepoFullName: ""
    property bool selectedRepoIsPrivate: false
    property bool cloneAttempted: false

    function resetFormAndCache() {
        console.log("[CloneModal] Reset completo del form e della cache...");
        urlField.text = "";
        destFolderField.text = "";
        dirNameField.text = "";
        displayNameField.text = "";
        selectedRepoFullName = "";
        selectedRepoIsPrivate = false;
        showTokenConfig = false;
        showRepoPicker = false;
        cloneAttempted = false;
        dirNameField.propertyModified = false;

        if (typeof searchField !== "undefined" && searchField) {
            searchField.text = "";
        }
        if (typeof tokenSecretInput !== "undefined" && tokenSecretInput) {
            tokenSecretInput.text = "";
        }

        if (root.profilesVm) {
            if (root.profilesVm.remoteReposModel) {
                root.profilesVm.remoteReposModel.searchFilter = "";
            }
            root.profilesVm.clearCache();
        }

        if (root.effectiveHubVm) {
            root.effectiveHubVm.resetCloneState();
        }
    }

    onClosed: {
        root.resetFormAndCache();
    }

    function openWithDefaultFolder(defaultParentFolder) {
        root.resetFormAndCache();

        if (defaultParentFolder && defaultParentFolder.length > 0) {
            destFolderField.text = defaultParentFolder;
        }

        if (root.profilesVm) {
            if (!root.profilesVm.activeProfileId || root.profilesVm.activeProfileId.length === 0 || root.profilesVm.activeProfileId === "url") {
                root.profilesVm.selectProfile("bitbucket");
            } else {
                root.profilesVm.refreshRepositories();
            }
            showRepoPicker = false;
        }

        root.open();
        if (typeof urlField !== "undefined" && urlField && urlField.visible) {
            urlField.forceActiveFocus();
        }
    }

    Connections {
        target: root.effectiveHubVm
        function onCloneCompleted(success, message) {
            if (success) {
                root.resetFormAndCache();
                root.close();
            }
        }
    }

    FolderDialog {
        id: cloneParentFolderDialog
        title: "Seleziona cartella di destinazione locale"
        currentFolder: destFolderField.text.length > 0 ? ("file://" + destFolderField.text) : ""
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
                    text: "⬇ Clona Repository (Stile GitKraken)"
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
                    enabled: !(root.effectiveHubVm && root.effectiveHubVm.isCloning)
                    onClicked: {
                        root.resetFormAndCache();
                        root.close();
                    }
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
        // 2. MAIN TWO-COLUMN BODY (Sidebar + Content)
        // ==========================================
        RowLayout {
            Layout.fillWidth: true
            spacing: 0

            // -------------------------------------------------------------
            // LEFT SIDEBAR: Stacked list of Git Servers/Providers (GitKraken)
            // -------------------------------------------------------------
            Rectangle {
                Layout.preferredWidth: 210
                Layout.fillHeight: true
                color: "#080D18"
                border.color: "#1E293B"
                border.width: 1

                ColumnLayout {
                    anchors.fill: parent
                    spacing: 0

                    // Sidebar Section Label
                    Item {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 34
                        Text {
                            anchors.left: parent.left
                            anchors.leftMargin: 16
                            anchors.verticalCenter: parent.verticalCenter
                            text: "SERVIZI & PROVIDER"
                            color: "#64748B"
                            font.pixelSize: 10
                            font.bold: true
                            font.letterSpacing: 0.8
                        }
                    }

                    // Stacked Vertical List of Providers
                    ListView {
                        id: providerListView
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        clip: true
                        interactive: false
                        model: root.profilesVm ? root.profilesVm.profiles : []

                        delegate: Rectangle {
                            id: providerItem
                            width: providerListView.width
                            height: 42

                            readonly property bool isSelected: root.profilesVm && (root.profilesVm.activeProfileId === modelData.id)

                            color: isSelected ? "#1E293B" : (providerMouse.containsMouse ? "#111827" : "transparent")

                            // Active Left Indicator Bar
                            Rectangle {
                                width: 3
                                height: parent.height
                                color: "#38BDF8"
                                visible: providerItem.isSelected
                            }

                            RowLayout {
                                anchors.fill: parent
                                anchors.leftMargin: 16
                                anchors.rightMargin: 12
                                spacing: 10

                                Text {
                                    text: modelData.icon
                                    font.pixelSize: 14
                                }

                                Text {
                                    text: modelData.displayName
                                    color: providerItem.isSelected ? "#FFFFFF" : (providerMouse.containsMouse ? "#F8FAFC" : "#94A3B8")
                                    font.pixelSize: 12
                                    font.bold: providerItem.isSelected
                                    elide: Text.ElideRight
                                    Layout.fillWidth: true
                                }

                                // Status Dot: Green if SSH or Token configured
                                Rectangle {
                                    visible: modelData.id !== "url"
                                    width: 7
                                    height: 7
                                    radius: 3.5
                                    color: (modelData.hasSsh || modelData.hasToken) ? "#10B981" : "#475569"
                                }
                            }

                            MouseArea {
                                id: providerMouse
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: {
                                    if (root.profilesVm) {
                                        root.profilesVm.selectProfile(modelData.id);
                                        root.selectedRepoFullName = "";
                                        root.selectedRepoIsPrivate = false;
                                        urlField.text = "";
                                        root.showRepoPicker = (modelData.id !== "url");
                                    }
                                }
                            }
                        }
                    }
                }
            }

            // -------------------------------------------------------------
            // RIGHT MAIN CONTENT: Provider Details, Repositories & Path
            // -------------------------------------------------------------
            Item {
                Layout.fillWidth: true
                implicitHeight: rightContentLayout.implicitHeight + 24

                ColumnLayout {
                    id: rightContentLayout
                    anchors.fill: parent
                    anchors.leftMargin: 16
                    anchors.rightMargin: 16
                    anchors.topMargin: 12
                    anchors.bottomMargin: 12
                    spacing: 10

                    // ---------------------------------------------------------
                    // A. PROVIDER SUB-HEADER (Account, Badges, Protocol Switch)
                    // ---------------------------------------------------------
                    Rectangle {
                        Layout.fillWidth: true
                        height: 40
                        visible: root.profilesVm ? (root.profilesVm.activeProfileId !== "url") : false
                        color: "#131C2E"
                        radius: 6
                        border.color: "#1E293B"
                        border.width: 1

                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 12
                            anchors.rightMargin: 12
                            spacing: 8

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
                                visible: Boolean(root.profilesVm && root.profilesVm.activeProfileHasSsh)
                                implicitHeight: 22
                                implicitWidth: sshText.implicitWidth + 12
                                color: "#064E3B"
                                radius: 4
                                border.color: "#059669"
                                border.width: 1
                                Text {
                                    id: sshText
                                    anchors.centerIn: parent
                                    text: "🔑 SSH"
                                    color: "#A7F3D0"
                                    font.pixelSize: 10
                                    font.bold: true
                                }
                            }

                            // Token Status Badge
                            Rectangle {
                                visible: Boolean(root.profilesVm && root.profilesVm.activeProfileHasToken)
                                implicitHeight: 22
                                implicitWidth: tokenText.implicitWidth + 12
                                color: "#1E3A8A"
                                radius: 4
                                border.color: "#3B82F6"
                                border.width: 1
                                Text {
                                    id: tokenText
                                    anchors.centerIn: parent
                                    text: "🔒 Token (Argon2id)"
                                    color: "#BFDBFE"
                                    font.pixelSize: 10
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
                                    readonly property bool isSelected: root.profilesVm ? (root.profilesVm.selectedProtocol === "ssh") : false
                                    color: isSelected ? "#0284C7" : "#0F172A"
                                    border.color: isSelected ? "#38BDF8" : "#334155"
                                    Text {
                                        anchors.centerIn: parent
                                        text: "SSH"
                                        color: parent.isSelected ? "#FFFFFF" : "#94A3B8"
                                        font.pixelSize: 11
                                        font.bold: true
                                    }
                                    MouseArea {
                                        anchors.fill: parent
                                        cursorShape: Qt.PointingHandCursor
                                        onClicked: {
                                            if (root.profilesVm) {
                                                root.profilesVm.setSelectedProtocol("ssh");
                                                if (root.selectedRepoFullName.length > 0) {
                                                    // refresh active clone url in field
                                                    for (var i = 0; i < repoListView.count; ++i) {
                                                        var r = root.profilesVm.remoteReposModel.getRepoAt(i);
                                                        if (r.fullName === root.selectedRepoFullName) {
                                                            urlField.text = r.activeCloneUrl;
                                                            break;
                                                        }
                                                    }
                                                }
                                            }
                                        }
                                    }
                                }

                                Rectangle {
                                    width: 52
                                    height: 26
                                    radius: 4
                                    readonly property bool isSelected: root.profilesVm ? (root.profilesVm.selectedProtocol === "https") : false
                                    color: isSelected ? "#0284C7" : "#0F172A"
                                    border.color: isSelected ? "#38BDF8" : "#334155"
                                    Text {
                                        anchors.centerIn: parent
                                        text: "HTTPS"
                                        color: parent.isSelected ? "#FFFFFF" : "#94A3B8"
                                        font.pixelSize: 11
                                        font.bold: true
                                    }
                                    MouseArea {
                                        anchors.fill: parent
                                        cursorShape: Qt.PointingHandCursor
                                        onClicked: {
                                            if (root.profilesVm) {
                                                root.profilesVm.setSelectedProtocol("https");
                                                if (root.selectedRepoFullName.length > 0) {
                                                    for (var i = 0; i < repoListView.count; ++i) {
                                                        var r = root.profilesVm.remoteReposModel.getRepoAt(i);
                                                        if (r.fullName === root.selectedRepoFullName) {
                                                            urlField.text = r.activeCloneUrl;
                                                            break;
                                                        }
                                                    }
                                                }
                                            }
                                        }
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
                        visible: root.showTokenConfig && root.profilesVm && (root.profilesVm.activeProfileId !== "url")
                        implicitHeight: tokenConfigCol.implicitHeight + 16
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
                                text: root.profilesVm && root.profilesVm.activeProfileId === "bitbucket"
                                      ? "Bitbucket ha deprecato le App Password a Luglio 2026. Crea un 'API token with scopes' su id.atlassian.com selezionando l'app Bitbucket e lo scope 'read:repository:bitbucket'."
                                      : "Il token viene crittografato sul database locale con Argon2id (KDF) e AES-256-GCM. Sblocca tutti i repository privati e delle organizzazioni."
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
                                    placeholderText: root.profilesVm && root.profilesVm.activeProfileId === "bitbucket"
                                                     ? "Email Atlassian"
                                                     : "Username (es. Dokjolly0)"
                                    placeholderTextColor: "#475569"
                                    selectByMouse: true
                                    text: root.profilesVm ? root.profilesVm.activeProfileUsername : ""
                                    color: "#F8FAFC"
                                    font.pixelSize: 12
                                    background: Rectangle {
                                        color: "#0B1120"
                                        radius: 4
                                        border.color: "#334155"
                                    }
                                    Keys.onReturnPressed: tokenSecretInput.forceActiveFocus()
                                    onAccepted: tokenSecretInput.forceActiveFocus()
                                }

                                TextField {
                                    id: tokenSecretInput
                                    Layout.fillWidth: true
                                    Layout.preferredHeight: 32
                                    placeholderText: root.profilesVm && root.profilesVm.activeProfileId === "bitbucket"
                                                     ? "Atlassian Scoped API Token (ATATT...)"
                                                     : "Personal Access Token (ghp_... o API token)"
                                    placeholderTextColor: "#475569"
                                    selectByMouse: true
                                    echoMode: root.showTokenText ? TextInput.Normal : TextInput.Password
                                    color: "#F8FAFC"
                                    font.pixelSize: 12
                                    background: Rectangle {
                                        color: "#0B1120"
                                        radius: 4
                                        border.color: "#334155"
                                    }
                                    Keys.onReturnPressed: saveTokenBtn.clicked()
                                    onAccepted: saveTokenBtn.clicked()
                                }

                                Button {
                                    text: root.showTokenText ? "🙈" : "👁️"
                                    Layout.preferredWidth: 32
                                    Layout.preferredHeight: 32
                                    onClicked: root.showTokenText = !root.showTokenText
                                    background: Rectangle { color: "#1E293B"; radius: 4 }
                                }

                                Button {
                                    id: saveTokenBtn
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
                                    visible: Boolean(root.profilesVm && root.profilesVm.activeProfileHasToken)
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

                    // Informative Banner: Public consultation mode (no token configured)
                    Rectangle {
                        Layout.fillWidth: true
                        visible: Boolean(root.profilesVm && root.profilesVm.activeProfileId !== "url" && !root.profilesVm.activeProfileHasToken && !root.showTokenConfig)
                        implicitHeight: tokenTipLayout.implicitHeight + 14
                        color: "#0F2942"
                        radius: 6
                        border.color: "#0284C7"
                        border.width: 1

                        RowLayout {
                            id: tokenTipLayout
                            anchors.fill: parent
                            anchors.margins: 8
                            spacing: 10
                            Text {
                                text: "💡"
                                font.pixelSize: 15
                            }
                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 2
                                Text {
                                    text: "Consultazione pubblica: visualizzazione limitata ai soli repository pubblici"
                                    color: "#38BDF8"
                                    font.bold: true
                                    font.pixelSize: 11
                                }
                                Text {
                                    text: "Git Flux è collegato al tuo account, ma senza un Personal Access Token (PAT) GitHub restituisce solo le repo pubbliche. Per mostrare anche i repository privati e aziendali, inserisci un token."
                                    color: "#BAE6FD"
                                    font.pixelSize: 11
                                    wrapMode: Text.Wrap
                                    Layout.fillWidth: true
                                }
                            }
                            Button {
                                text: "🔑 Inserisci Token"
                                Layout.preferredHeight: 28
                                onClicked: root.showTokenConfig = true
                                background: Rectangle { color: parent.hovered ? "#0284C7" : "#0369A1"; radius: 4 }
                                contentItem: Text { text: "🔑 Inserisci Token"; color: "#FFFFFF"; font.pixelSize: 11; font.bold: true }
                            }
                        }
                    }

                    // Success Banner: Token is active (public + private repos unlocked)
                    Rectangle {
                        Layout.fillWidth: true
                        visible: Boolean(root.profilesVm && root.profilesVm.activeProfileId !== "url" && root.profilesVm.activeProfileHasToken && !root.showTokenConfig)
                        implicitHeight: authTipLayout.implicitHeight + 14
                        color: "#06281D"
                        radius: 6
                        border.color: "#059669"
                        border.width: 1

                        RowLayout {
                            id: authTipLayout
                            anchors.fill: parent
                            anchors.margins: 8
                            spacing: 8
                            Text {
                                text: "🔒"
                                font.pixelSize: 14
                            }
                            Text {
                                text: "Token PAT attivo (Argon2id + AES-256): accesso a tutti i repository pubblici e privati abilitato."
                                color: "#A7F3D0"
                                font.bold: true
                                font.pixelSize: 11
                                Layout.fillWidth: true
                            }
                            Button {
                                text: "Modifica"
                                Layout.preferredHeight: 24
                                onClicked: root.showTokenConfig = true
                                background: Rectangle { color: parent.hovered ? "#047857" : "#065F46"; radius: 4 }
                                contentItem: Text { text: "Modifica"; color: "#ECFDF5"; font.pixelSize: 10; font.bold: true }
                            }
                        }
                    }

                    // ---------------------------------------------------------
                    // C. REPOSITORY SELECTOR & EXPANDABLE LIST (GitKraken Clean)
                    // ---------------------------------------------------------
                    ColumnLayout {
                        Layout.fillWidth: true
                        visible: root.profilesVm ? (root.profilesVm.activeProfileId !== "url") : false
                        spacing: 6

                        Text {
                            text: "Repository da Clonare:"
                            color: "#94A3B8"
                            font.pixelSize: 12
                            font.bold: true
                        }

                        // Compact Clickable Selector Box
                        Rectangle {
                            id: repoSelectorBox
                            Layout.fillWidth: true
                            height: 42
                            radius: 6
                            color: repoSelectorMouse.containsMouse ? "#1E293B" : "#0B1120"
                            border.color: (root.cloneAttempted && root.selectedRepoFullName.length === 0)
                                          ? "#F59E0B"
                                          : (root.showRepoPicker ? "#38BDF8" : (root.selectedRepoFullName.length > 0 ? "#0284C7" : "#334155"))
                            border.width: (root.showRepoPicker || (root.cloneAttempted && root.selectedRepoFullName.length === 0)) ? 2 : 1

                            RowLayout {
                                anchors.fill: parent
                                anchors.leftMargin: 12
                                anchors.rightMargin: 12
                                spacing: 10

                                Text {
                                    text: root.selectedRepoFullName.length > 0 ? (root.selectedRepoIsPrivate ? "🔒" : "🌐") : "📂"
                                    font.pixelSize: 16
                                }

                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 1

                                    Text {
                                        text: root.selectedRepoFullName.length > 0
                                              ? root.selectedRepoFullName
                                              : "Clicca per selezionare un repository da " + (root.profilesVm ? root.profilesVm.activeProfileDisplayName : "server") + "..."
                                        color: root.selectedRepoFullName.length > 0 ? "#F8FAFC" : "#64748B"
                                        font.pixelSize: 12
                                        font.bold: root.selectedRepoFullName.length > 0
                                        elide: Text.ElideRight
                                        Layout.fillWidth: true
                                    }

                                    Text {
                                        visible: root.selectedRepoFullName.length > 0
                                        text: urlField.text
                                        color: "#38BDF8"
                                        font.pixelSize: 10
                                        elide: Text.ElideRight
                                        Layout.fillWidth: true
                                    }
                                }

                                Rectangle {
                                    implicitHeight: 24
                                    implicitWidth: actionBtnText.implicitWidth + 14
                                    radius: 4
                                    color: root.showRepoPicker ? "#0284C7" : (repoSelectorMouse.containsMouse ? "#334155" : "#1E293B")
                                    border.color: root.showRepoPicker ? "#38BDF8" : "#475569"
                                    Text {
                                        id: actionBtnText
                                        anchors.centerIn: parent
                                        text: root.showRepoPicker
                                              ? "Chiudi ▲"
                                              : (root.selectedRepoFullName.length > 0 ? "Cambia ▼" : "Scegli ▼")
                                        color: root.showRepoPicker ? "#FFFFFF" : "#94A3B8"
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
                                    root.showRepoPicker = !root.showRepoPicker;
                                    if (root.showRepoPicker) {
                                        searchField.forceActiveFocus();
                                    }
                                }
                            }
                        }

                        // Expandable Dropdown Drawer for Repository Selection
                        Rectangle {
                            id: repoPickerContainer
                            Layout.fillWidth: true
                            visible: root.showRepoPicker
                            implicitHeight: repoPickerLayout.implicitHeight + 16
                            color: "#080D1A"
                            radius: 6
                            border.color: "#38BDF8"
                            border.width: 1

                            onVisibleChanged: {
                                if (visible && typeof searchField !== "undefined") {
                                    searchField.forceActiveFocus();
                                }
                            }

                            ColumnLayout {
                                id: repoPickerLayout
                                anchors.fill: parent
                                anchors.margins: 8
                                spacing: 6

                                // Search Bar & Counter & Clear
                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: 8

                                    TextField {
                                        id: searchField
                                        Layout.fillWidth: true
                                        Layout.preferredHeight: 32
                                        placeholderText: "🔍 Cerca tra i tuoi repository (es. FinancialManager, FileHub)..."
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
                                    height: 36
                                    visible: Boolean(root.profilesVm && root.profilesVm.isFetching)
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
                                    visible: Boolean(root.profilesVm && root.profilesVm.fetchError.length > 0 && !root.profilesVm.isFetching)
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

                                // Scrollable Remote Repository List View (height 190)
                                Rectangle {
                                    Layout.fillWidth: true
                                    height: 190
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
                                                    root.selectedRepoIsPrivate = model.isPrivate;
                                                    var targetUrl = model.activeCloneUrl;
                                                    if (!targetUrl || targetUrl.length === 0) {
                                                        targetUrl = (root.profilesVm && root.profilesVm.selectedProtocol === "ssh") ? model.sshUrl : model.httpsUrl;
                                                    }
                                                    if (!targetUrl || targetUrl.length === 0) {
                                                        targetUrl = model.httpsUrl ? model.httpsUrl : model.sshUrl;
                                                    }
                                                    urlField.text = targetUrl;

                                                    var defaultName = model.fullName ? model.fullName.split("/").pop() : model.name;
                                                    dirNameField.text = defaultName;
                                                    dirNameField.propertyModified = true;
                                                    root.showRepoPicker = false;
                                                    root.cloneAttempted = false;
                                                    console.log("[CloneModal] Repo selezionato:", model.fullName, "URL:", targetUrl, "Cartella:", defaultName);
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
                        }
                    }

                    // ---------------------------------------------------------
                    // D. DIRECT URL INPUT (Only in "URL Diretto" mode)
                    // ---------------------------------------------------------
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 4
                        visible: root.profilesVm ? (root.profilesVm.activeProfileId === "url") : false

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
                            enabled: !(root.effectiveHubVm && root.effectiveHubVm.isCloning)
                            placeholderText: "es. https://github.com/utente/progetto.git o git@github.com:..."
                            placeholderTextColor: "#475569"
                            color: "#F8FAFC"
                            font.pixelSize: 13
                            selectByMouse: true
                            background: Rectangle {
                                color: "#0B1120"
                                radius: 6
                                border.color: (root.cloneAttempted && urlField.text.trim().length === 0)
                                              ? "#F59E0B"
                                              : (urlField.activeFocus ? "#38BDF8" : "#334155")
                                border.width: 1
                            }
                            onTextChanged: {
                                if (root.cloneAttempted && text.trim().length > 0) root.cloneAttempted = false;
                                if (root.effectiveHubVm && text.length > 0 && root.profilesVm && root.profilesVm.activeProfileId === "url") {
                                    var extracted = root.effectiveHubVm.extractRepoNameFromUrl(text);
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
                                enabled: !(root.effectiveHubVm && root.effectiveHubVm.isCloning)
                                placeholderText: "Seleziona cartella in cui scaricare il progetto..."
                                placeholderTextColor: "#475569"
                                color: "#F8FAFC"
                                font.pixelSize: 13
                                selectByMouse: true
                                background: Rectangle {
                                    color: "#0B1120"
                                    radius: 6
                                    border.color: (root.cloneAttempted && destFolderField.text.trim().length === 0)
                                                  ? "#F59E0B"
                                                  : (destFolderField.activeFocus ? "#38BDF8" : "#334155")
                                    border.width: 1
                                }
                                onTextChanged: {
                                    if (root.cloneAttempted && text.trim().length > 0) root.cloneAttempted = false;
                                }
                                onAccepted: {
                                    if (cloneButton.enabled) cloneButton.clicked();
                                }
                            }

                            Button {
                                text: "Sfoglia..."
                                Layout.preferredHeight: 34
                                Layout.preferredWidth: 84
                                enabled: !(root.effectiveHubVm && root.effectiveHubVm.isCloning)
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

                        Text {
                            visible: root.cloneAttempted && destFolderField.text.trim().length === 0
                            text: "⚠️ Specifica una cartella di destinazione prima di avviare la clonazione"
                            color: "#FCA5A5"
                            font.pixelSize: 11
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
                                enabled: !(root.effectiveHubVm && root.effectiveHubVm.isCloning)
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
                                enabled: !(root.effectiveHubVm && root.effectiveHubVm.isCloning)
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

                    // Warning when no repo is selected in provider mode (only shown after clone attempt)
                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 38
                        visible: root.cloneAttempted && root.profilesVm && root.profilesVm.activeProfileId !== "url" && root.selectedRepoFullName.length === 0
                        color: "#451A03"
                        radius: 6
                        border.color: "#D97706"
                        border.width: 1

                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 12
                            anchors.rightMargin: 12
                            spacing: 8

                            Text {
                                text: "⚠️ Seleziona prima un repository dal menu a tendina sopra!"
                                color: "#FDE68A"
                                font.pixelSize: 11
                                font.bold: true
                                Layout.fillWidth: true
                            }

                            Button {
                                text: root.showRepoPicker ? "Chiudi ▲" : "Scegli Repository ▼"
                                Layout.preferredHeight: 26
                                onClicked: {
                                    root.showRepoPicker = !root.showRepoPicker;
                                    if (root.showRepoPicker && typeof searchField !== "undefined") {
                                        searchField.forceActiveFocus();
                                    }
                                }
                                contentItem: Text {
                                    text: root.showRepoPicker ? "Chiudi ▲" : "Scegli Repository ▼"
                                    color: "#FDE68A"
                                    font.pixelSize: 11
                                    font.bold: true
                                    horizontalAlignment: Text.AlignHCenter
                                    verticalAlignment: Text.AlignVCenter
                                }
                                background: Rectangle {
                                    color: "#78350F"
                                    radius: 4
                                }
                            }
                        }
                    }

                    // Destination Path Preview
                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 38
                        visible: destFolderField.text.trim().length > 0 && dirNameField.text.trim().length > 0 && !(root.effectiveHubVm && root.effectiveHubVm.isCloning)
                        color: "#0F172A"
                        radius: 6
                        border.color: "#1E293B"
                        border.width: 1

                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 10
                            anchors.rightMargin: 10
                            spacing: 8

                            Text {
                                text: "📍 Cartella finale:"
                                color: "#94A3B8"
                                font.pixelSize: 11
                                font.bold: true
                            }

                            Text {
                                text: destFolderField.text.trim() + "/" + dirNameField.text.trim()
                                color: "#38BDF8"
                                font.pixelSize: 11
                                font.bold: true
                                elide: Text.ElideMiddle
                                Layout.fillWidth: true
                            }

                            Text {
                                text: "(Premi 'Avvia Clonazione' per iniziare)"
                                color: "#64748B"
                                font.pixelSize: 10
                                font.italic: true
                            }
                        }
                    }

                    // Progress Indicator with Real-Time Percentage
                    Rectangle {
                        Layout.fillWidth: true
                        visible: Boolean(root.effectiveHubVm && root.effectiveHubVm.isCloning)
                        color: "#0B132B"
                        radius: 8
                        border.color: "#0284C7"
                        border.width: 1
                        implicitHeight: progressCol.implicitHeight + 24

                        ColumnLayout {
                            id: progressCol
                            anchors.fill: parent
                            anchors.margins: 12
                            spacing: 8

                            RowLayout {
                                Layout.fillWidth: true
                                spacing: 8

                                Text {
                                    text: "⬇ Clonazione repository in corso..."
                                    color: "#F8FAFC"
                                    font.pixelSize: 12
                                    font.bold: true
                                    Layout.fillWidth: true
                                }

                                Text {
                                    visible: Boolean(root.effectiveHubVm && root.effectiveHubVm.cloneProgressPercent > 0)
                                    text: root.effectiveHubVm ? (root.effectiveHubVm.cloneProgressPercent + "%") : ""
                                    color: "#38BDF8"
                                    font.pixelSize: 13
                                    font.bold: true
                                }
                            }

                            ProgressBar {
                                id: cloneProgressBar
                                Layout.fillWidth: true
                                Layout.preferredHeight: 8
                                from: 0
                                to: 100
                                value: (root.effectiveHubVm && root.effectiveHubVm.cloneProgressPercent > 0)
                                       ? root.effectiveHubVm.cloneProgressPercent
                                       : 0
                                indeterminate: Boolean(root.effectiveHubVm && root.effectiveHubVm.cloneProgressPercent <= 0)
                                background: Rectangle {
                                    implicitHeight: 8
                                    color: "#1E293B"
                                    radius: 4
                                }
                                contentItem: Item {
                                    implicitHeight: 8
                                    clip: true
                                    Rectangle {
                                        id: barFill
                                        width: cloneProgressBar.indeterminate
                                               ? parent.width * 0.35
                                               : Math.max(8, parent.width * cloneProgressBar.visualPosition)
                                        height: parent.height
                                        radius: 4
                                        x: cloneProgressBar.indeterminate ? indetAnim.animX : 0
                                        gradient: Gradient {
                                            orientation: Gradient.Horizontal
                                            GradientStop { position: 0.0; color: "#0284C7" }
                                            GradientStop { position: 1.0; color: "#38BDF8" }
                                        }
                                    }

                                    NumberAnimation {
                                        id: indetAnim
                                        property real animX: 0
                                        target: indetAnim
                                        property: "animX"
                                        from: -cloneProgressBar.width * 0.35
                                        to: cloneProgressBar.width
                                        duration: 1200
                                        loops: Animation.Infinite
                                        running: cloneProgressBar.indeterminate && root.visible
                                    }
                                }
                            }

                            Text {
                                text: root.effectiveHubVm ? root.effectiveHubVm.cloneStatusMessage : ""
                                color: "#94A3B8"
                                font.pixelSize: 11
                                font.family: "monospace"
                                elide: Text.ElideRight
                                Layout.fillWidth: true
                            }
                        }
                    }

                    // Error Banner
                    Rectangle {
                        Layout.fillWidth: true
                        visible: Boolean(root.effectiveHubVm && root.effectiveHubVm.lastError.length > 0 && !root.effectiveHubVm.isCloning)
                        color: "#450A0A"
                        radius: 6
                        border.color: "#DC2626"
                        border.width: 1
                        implicitHeight: errorText.implicitHeight + 16

                        Text {
                            id: errorText
                            anchors.fill: parent
                            anchors.margins: 8
                            text: root.effectiveHubVm ? root.effectiveHubVm.lastError : ""
                            color: "#FECACA"
                            font.pixelSize: 12
                            wrapMode: Text.Wrap
                        }
                    }
                }
            }
        }

        // ==========================================
        // 3. MODAL FOOTER
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
                    enabled: !(root.effectiveHubVm && root.effectiveHubVm.isCloning)
                    onClicked: {
                        root.resetFormAndCache();
                        root.close();
                    }
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
                    Layout.preferredHeight: 36
                    enabled: !(root.effectiveHubVm && root.effectiveHubVm.isCloning)
                    onClicked: {
                        console.log("[CloneModal] Avvia Clonazione cliccato. URL=" + urlField.text + " DestFolder=" + destFolderField.text + " Dir=" + dirNameField.text);
                        if (urlField.text.trim().length === 0) {
                            console.warn("[CloneModal] Nessun repository selezionato! Apertura automatica cassetto...");
                            root.showRepoPicker = true;
                            if (typeof searchField !== "undefined") searchField.forceActiveFocus();
                            return;
                        }
                        if (destFolderField.text.trim().length === 0) {
                            console.warn("[CloneModal] Cartella di destinazione non specificata!");
                            destFolderField.forceActiveFocus();
                            return;
                        }
                        var vm = root.effectiveHubVm;
                        if (vm) {
                            console.log("[CloneModal] Invocazione cloneRepositoryAsync su ViewModel per: " + urlField.text.trim());
                            vm.cloneRepositoryAsync(
                                urlField.text.trim(),
                                destFolderField.text.trim(),
                                dirNameField.text.trim(),
                                displayNameField.text.trim()
                            );
                        } else {
                            console.error("[CloneModal] ERRORE CRITICO: ViewModel HomeHub non disponibile!");
                        }
                    }
                    contentItem: Text {
                        text: (root.effectiveHubVm && root.effectiveHubVm.isCloning)
                              ? (root.effectiveHubVm.cloneProgressPercent > 0
                                 ? ("⏳ " + root.effectiveHubVm.cloneProgressPercent + "% In corso...")
                                 : "⏳ Clonazione in corso...")
                              : (root.selectedRepoFullName.length > 0
                                 ? "⬇ Avvia Clonazione (" + (dirNameField.text.length > 0 ? dirNameField.text : root.selectedRepoFullName) + ")"
                                 : "⬇ Avvia Clonazione")
                        color: (root.effectiveHubVm && root.effectiveHubVm.isCloning)
                               ? "#38BDF8"
                               : (urlField.text.trim().length > 0 ? "#FFFFFF" : "#94A3B8")
                        font.pixelSize: 13
                        font.bold: true
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                    background: Rectangle {
                        color: (root.effectiveHubVm && root.effectiveHubVm.isCloning)
                               ? "#0C4A6E"
                               : (urlField.text.trim().length > 0
                                  ? (cloneButton.hovered ? "#0284C7" : "#0369A1")
                                  : (cloneButton.hovered ? "#1E293B" : "#0F172A"))
                        radius: 6
                        border.color: (root.effectiveHubVm && root.effectiveHubVm.isCloning)
                                      ? "#38BDF8"
                                      : (urlField.text.trim().length > 0 ? "#38BDF8" : "#334155")
                        border.width: 1
                    }
                }
            }
        }
    }
}
