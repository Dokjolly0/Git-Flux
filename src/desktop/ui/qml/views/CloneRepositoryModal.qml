import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import "../themes"

Dialog {
    id: root
    readonly property FluxTheme theme: FluxTheme {}
    title: "Clona Repository"
    modal: true
    width: 820
    header: null
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
    property bool showRepoPicker: false
    property string selectedRepoFullName: ""
    property bool selectedRepoIsPrivate: false
    property bool repoTouched: false
    property bool destFolderMissing: false
    property bool urlMissing: false
    property bool sshGuideExpanded: true

    // Re-points the URL field at the selected repository after a protocol change (SSH <-> HTTPS).
    function refreshSelectedRepoUrl() {
        const nextUrl = repoPicker ? repoPicker.activeUrlForRepo(root.selectedRepoFullName) : "";
        if (nextUrl.length > 0) {
            urlField.text = nextUrl;
        }
    }

    // Runs an action button of the troubleshooting guide. Returns a short feedback text.
    function runGuideAction(action, host) {
        if (!root.profilesVm) return "";
        if (action === "copyPublicKey") {
            return root.profilesVm.copyPublicKey(host) ? "Chiave copiata negli appunti" : "Nessuna chiave pubblica trovata";
        }
        if (action === "recheckSsh") {
            root.profilesVm.recheckSsh();
            return "Verifica in corso…";
        }
        if (action === "useHttps") {
            root.profilesVm.setSelectedProtocol("https");
            root.refreshSelectedRepoUrl();
            if (root.effectiveHubVm && root.effectiveHubVm.cloneIssue.length > 0) {
                root.effectiveHubVm.resetCloneState();
            }
            return "Protocollo impostato su HTTPS";
        }
        if (action === "openTokenConfig") {
            root.showTokenConfig = true;
            return "";
        }
        return "";
    }

    // Read-only text that can be selected with the mouse and copied (logs, commands...).
    component SelectableText: TextEdit {
        readonly property FluxTheme theme: FluxTheme {}
        readOnly: true
        selectByMouse: true
        selectByKeyboard: true
        persistentSelection: false
        wrapMode: TextEdit.Wrap
        textFormat: TextEdit.PlainText
        selectionColor: theme.selectionBg
        selectedTextColor: theme.selectionText
        color: theme.textSecondary
        font.pixelSize: 11
    }

    // Small flat button used by the guide (copy, open link, actions).
    component GuideButton: Button {
        id: guideBtn
        readonly property FluxTheme theme: FluxTheme {}
        property color accent: theme.accentCyan
        Layout.preferredHeight: 24
        leftPadding: 10
        rightPadding: 10
        hoverEnabled: true
        contentItem: Text {
            text: guideBtn.text
            color: guideBtn.hovered ? guideBtn.theme.textPrimary : guideBtn.accent
            font.pixelSize: 11
            font.bold: true
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
        }
        background: Rectangle {
            radius: 4
            color: guideBtn.hovered ? guideBtn.theme.infoBg : guideBtn.theme.inputBg
            border.color: guideBtn.accent
            border.width: 1
        }
    }

    Connections {
        target: root.profilesVm
        function onSelectedProtocolChanged() {
            root.refreshSelectedRepoUrl();
        }
        function onSshCheckChanged() {
            // Key accepted now: an SSH error from the last clone attempt is no longer relevant.
            if (root.profilesVm.sshCheckState === "ok" && root.effectiveHubVm &&
                    root.effectiveHubVm.cloneIssue.indexOf("ssh") === 0 && !root.effectiveHubVm.isCloning) {
                root.effectiveHubVm.resetCloneState();
            }
        }
    }

    // While an SSH problem is shown, check again in the background: the user is likely fixing it
    // in the browser (adding the key) and shouldn't have to restart the app or press a button.
    readonly property bool sshIssueShown: Boolean(root.profilesVm && root.profilesVm.activeProfileId !== "url" &&
        root.profilesVm.sshCheckState.length > 0 && root.profilesVm.sshCheckState !== "checking" &&
        root.profilesVm.sshCheckState !== "ok")

    Timer {
        interval: 10000
        repeat: true
        running: root.visible && root.sshIssueShown
        onTriggered: root.profilesVm.recheckSshIfFailing()
    }

    Connections {
        target: Qt.application
        function onStateChanged() {
            if (Qt.application.state === Qt.ApplicationActive && root.visible && root.sshIssueShown) {
                root.profilesVm.recheckSshIfFailing();
            }
        }
    }

    function resetFormAndCache() {
        urlField.text = "";
        destFolderField.text = "";
        dirNameField.text = "";
        displayNameField.text = "";
        selectedRepoFullName = "";
        selectedRepoIsPrivate = false;
        showTokenConfig = false;
        showRepoPicker = false;
        repoTouched = false;
        destFolderMissing = false;
        urlMissing = false;
        dirNameField.propertyModified = false;

        if (repoPicker) {
            repoPicker.clearSearch();
        }
        if (tokenConfigCard) {
            tokenConfigCard.clearTokenInput();
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

    onAboutToShow: {
        if (destFolderField.text.trim().length === 0 && root.effectiveHubVm && root.effectiveHubVm.lastCloneDestinationPath && root.effectiveHubVm.lastCloneDestinationPath.length > 0) {
            destFolderField.text = root.effectiveHubVm.lastCloneDestinationPath;
        }
    }

    function openWithDefaultFolder(defaultParentFolder) {
        root.resetFormAndCache();

        let initialFolder = "";
        if (defaultParentFolder && defaultParentFolder.length > 0) {
            initialFolder = defaultParentFolder;
        } else if (root.effectiveHubVm && root.effectiveHubVm.lastCloneDestinationPath && root.effectiveHubVm.lastCloneDestinationPath.length > 0) {
            initialFolder = root.effectiveHubVm.lastCloneDestinationPath;
        }
        destFolderField.text = initialFolder;

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

        function onCloneCompleted(success) {
            if (success) {
                root.resetFormAndCache();
                root.close();
            }
        }
    }

    FolderDialog {
        id: cloneParentFolderDialog
        title: "Seleziona cartella di destinazione locale"
        currentFolder: destFolderField.text.length > 0
            ? ("file://" + destFolderField.text)
            : (root.effectiveHubVm && root.effectiveHubVm.lastCloneDestinationPath && root.effectiveHubVm.lastCloneDestinationPath.length > 0
                ? ("file://" + root.effectiveHubVm.lastCloneDestinationPath)
                : "")
        onAccepted: {
            let selected = cloneParentFolderDialog.selectedFolder.toString();
            if (selected.startsWith("file://")) {
                selected = selected.substring(7);
            }
            destFolderField.text = selected;
            root.destFolderMissing = false;
            if (root.effectiveHubVm) {
                root.effectiveHubVm.setLastCloneDestinationPath(selected);
            }
        }
    }

    // True when the selected provider is a self-hosted server added by hand (can be removed).
    readonly property bool activeProfileRemovable: {
        if (!root.profilesVm) return false;
        const list = root.profilesVm.profiles;
        for (let i = 0; i < list.length; ++i) {
            if (list[i].id === root.profilesVm.activeProfileId) return list[i].removable === true;
        }
        return false;
    }

    CustomServerPopup {
        id: customServerPopup
        profilesVm: root.profilesVm
        onServerAdded: {
            root.selectedRepoFullName = "";
            root.selectedRepoIsPrivate = false;
            urlField.text = "";
            root.showTokenConfig = false;
            root.showRepoPicker = true;
        }
    }

    background: FluxModalBackground {}

    contentItem: ColumnLayout {
        spacing: 0

        // ==========================================
        // 1. MODAL HEADER
        // ==========================================
        FluxModalHeader {
            Layout.fillWidth: true
            Layout.preferredHeight: 48
            titleText: "⬇ Clona Repository"
            enabled: !(root.effectiveHubVm && root.effectiveHubVm.isCloning)
            onCloseRequested: {
                root.resetFormAndCache();
                root.close();
            }
        }

        // ==========================================
        // 2. MAIN TWO-COLUMN BODY (Sidebar + Content)
        // ==========================================
        RowLayout {
            Layout.fillWidth: true
            spacing: 0

            // LEFT SIDEBAR: Stacked list of Git Servers/Providers
            ProviderSidebar {
                Layout.preferredWidth: 210
                Layout.fillHeight: true
                profilesVm: root.profilesVm
                onProfileSelected: function(profileId) {
                    if (root.profilesVm) {
                        root.profilesVm.selectProfile(profileId);
                        root.selectedRepoFullName = "";
                        root.selectedRepoIsPrivate = false;
                        urlField.text = "";
                        root.showRepoPicker = (profileId !== "url");
                    }
                }
                onAddCustomServerRequested: customServerPopup.openForNew()
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
                        color: root.theme.surfaceElevated
                        radius: 6
                        border.color: root.theme.borderSubtle
                        border.width: 1

                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 12
                            anchors.rightMargin: 12
                            spacing: 8

                            // Active Provider Icon & Profile Title
                            Image {
                                visible: Boolean(root.profilesVm && root.profilesVm.activeProfileIconSource && root.profilesVm.activeProfileIconSource.length > 0)
                                source: root.profilesVm ? root.profilesVm.activeProfileIconSource : ""
                                Layout.preferredWidth: 16
                                Layout.preferredHeight: 16
                                Layout.alignment: Qt.AlignVCenter
                                fillMode: Image.PreserveAspectFit
                                smooth: true
                                mipmap: true
                            }

                            Text {
                                text: (root.profilesVm ? root.profilesVm.activeProfileDisplayName : "") +
                                    (root.profilesVm && root.profilesVm.activeProfileUsername.length > 0
                                        ? " • @" + root.profilesVm.activeProfileUsername : "")
                                color: root.theme.textPrimary
                                font.bold: true
                                font.pixelSize: 12
                            }

                            // SSH Status Badge
                            Rectangle {
                                visible: Boolean(root.profilesVm && root.profilesVm.activeProfileHasSsh)
                                implicitHeight: 22
                                implicitWidth: sshText.implicitWidth + 12
                                color: root.theme.successBg
                                radius: 4
                                border.color: root.theme.success
                                border.width: 1
                                Text {
                                    id: sshText
                                    anchors.centerIn: parent
                                    text: "🔑 SSH"
                                    color: root.theme.successText
                                    font.pixelSize: 10
                                    font.bold: true
                                }
                            }

                            // Token Status Badge
                            Rectangle {
                                visible: Boolean(root.profilesVm && root.profilesVm.activeProfileHasToken)
                                implicitHeight: 22
                                implicitWidth: tokenText.implicitWidth + 12
                                color: root.theme.infoBg
                                radius: 4
                                border.color: root.theme.accentBlue
                                border.width: 1
                                Text {
                                    id: tokenText
                                    anchors.centerIn: parent
                                    text: "🔒 Token (Argon2id)"
                                    color: root.theme.infoText
                                    font.pixelSize: 10
                                    font.bold: true
                                }
                            }

                            Item {
                                Layout.fillWidth: true
                            }

                            // Protocol Switcher (SSH vs HTTPS)
                            RowLayout {
                                spacing: 2
                                Text {
                                    text: "Protocollo:"
                                    color: root.theme.textMuted
                                    font.pixelSize: 11
                                }

                                Rectangle {
                                    width: 44
                                    height: 26
                                    radius: 4
                                    readonly property bool isSelected: root.profilesVm ? (root.profilesVm.selectedProtocol === "ssh") : false
                                    color: isSelected ? root.theme.accentBlue : root.theme.inputBg
                                    border.color: isSelected ? root.theme.accentCyan : root.theme.border
                                    Text {
                                        anchors.centerIn: parent
                                        text: "SSH"
                                        color: parent.isSelected ? root.theme.textPrimary : root.theme.textMuted
                                        font.pixelSize: 11
                                        font.bold: true
                                    }
                                    MouseArea {
                                        anchors.fill: parent
                                        cursorShape: Qt.PointingHandCursor
                                        onClicked: {
                                            if (root.profilesVm) {
                                                root.profilesVm.setSelectedProtocol("ssh");
                                                root.refreshSelectedRepoUrl();
                                            }
                                        }
                                    }
                                }

                                Rectangle {
                                    width: 52
                                    height: 26
                                    radius: 4
                                    readonly property bool isSelected: root.profilesVm ? (root.profilesVm.selectedProtocol === "https") : false
                                    color: isSelected ? root.theme.accentBlue : root.theme.inputBg
                                    border.color: isSelected ? root.theme.accentCyan : root.theme.border
                                    Text {
                                        anchors.centerIn: parent
                                        text: "HTTPS"
                                        color: parent.isSelected ? root.theme.textPrimary : root.theme.textMuted
                                        font.pixelSize: 11
                                        font.bold: true
                                    }
                                    MouseArea {
                                        anchors.fill: parent
                                        cursorShape: Qt.PointingHandCursor
                                        onClicked: {
                                            if (root.profilesVm) {
                                                root.profilesVm.setSelectedProtocol("https");
                                                root.refreshSelectedRepoUrl();
                                            }
                                        }
                                    }
                                }
                            }

                            // Refresh Button
                            FluxActionButton {
                                text: "🔄"
                                Layout.preferredHeight: 26
                                Layout.preferredWidth: 28
                                onClicked: if (root.profilesVm) root.profilesVm.refreshRepositories()
                            }

                            // Remove Server Button (only for servers added by hand)
                            FluxActionButton {
                                visible: root.activeProfileRemovable
                                text: "🗑"
                                variant: "danger"
                                Layout.preferredHeight: 26
                                Layout.preferredWidth: 28
                                ToolTip.visible: hovered
                                ToolTip.text: "Rimuovi questo server"
                                onClicked: {
                                    if (root.profilesVm) {
                                        root.profilesVm.removeCustomProfile(root.profilesVm.activeProfileId);
                                        root.selectedRepoFullName = "";
                                        root.selectedRepoIsPrivate = false;
                                        root.showTokenConfig = false;
                                        root.showRepoPicker = false;
                                        urlField.text = "";
                                    }
                                }
                            }

                            // Config Token Button
                            FluxActionButton {
                                text: root.showTokenConfig ? "Nascondi" : (root.profilesVm && root.profilesVm.activeProfileHasToken ? "Modifica Token" : "+ Inserisci Token")
                                Layout.preferredHeight: 26
                                textColor: root.showTokenConfig ? root.theme.accentCyan : root.theme.textMuted
                                baseBorderColor: root.showTokenConfig ? root.theme.accentCyan : root.theme.borderHighlight
                                onClicked: root.showTokenConfig = !root.showTokenConfig
                            }
                        }
                    }

                    // ---------------------------------------------------------
                    // A2. SSH KEY CHECK ("ssh -T") + HOW TO FIX IT
                    // ---------------------------------------------------------
                    Rectangle {
                        id: sshCheckCard
                        readonly property string checkState: root.profilesVm ? root.profilesVm.sshCheckState : ""
                        readonly property bool isIssue: checkState.length > 0 && checkState !== "checking" && checkState !== "ok"
                        // A failed clone shows its own guide below: don't repeat it here.
                        readonly property bool cloneGuideShown: Boolean(root.effectiveHubVm && root.effectiveHubVm.cloneIssue.length > 0)

                        Layout.fillWidth: true
                        visible: root.profilesVm !== null && root.profilesVm.activeProfileId !== "url" &&
                                 checkState.length > 0 && !(isIssue && cloneGuideShown)
                        implicitHeight: sshCheckCol.implicitHeight + 14
                        radius: 6
                        color: isIssue ? root.theme.warningBg : (checkState === "ok" ? root.theme.successBg : root.theme.infoBg)
                        border.color: isIssue ? root.theme.warning : (checkState === "ok" ? root.theme.success : root.theme.info)
                        border.width: 1

                        ColumnLayout {
                            id: sshCheckCol
                            anchors.fill: parent
                            anchors.margins: 7
                            spacing: 6

                            RowLayout {
                                Layout.fillWidth: true
                                spacing: 8

                                Text {
                                    Layout.fillWidth: true
                                    wrapMode: Text.Wrap
                                    font.pixelSize: 11
                                    font.bold: true
                                    color: sshCheckCard.isIssue ? root.theme.warningText : (sshCheckCard.checkState === "ok" ? root.theme.successText : root.theme.infoText)
                                    text: {
                                        if (!root.profilesVm) return "";
                                        const host = root.profilesVm.activeProfileHost;
                                        if (sshCheckCard.checkState === "checking") return "⏳ Verifica della chiave SSH su " + host + "…";
                                        if (sshCheckCard.checkState === "ok") {
                                            const acc = root.profilesVm.sshCheckAccount;
                                            return "✔ Chiave SSH accettata da " + host + (acc.length > 0 ? " (account @" + acc + ")" : "");
                                        }
                                        const g = root.profilesVm.troubleshootingGuide(sshCheckCard.checkState, "");
                                        return "⚠️ " + g.title + (root.profilesVm.sshAutoSwitchedToHttps ? " — per ora uso HTTPS" : "");
                                    }
                                }

                                GuideButton {
                                    visible: sshCheckCard.isIssue
                                    text: root.sshGuideExpanded ? "Nascondi ▴" : "Come risolvere ▾"
                                    accent: root.theme.warning
                                    onClicked: root.sshGuideExpanded = !root.sshGuideExpanded
                                }

                                GuideButton {
                                    visible: sshCheckCard.checkState === "ok"
                                    text: "Verifica di nuovo"
                                    accent: root.theme.accentTeal
                                    onClicked: root.profilesVm.recheckSsh()
                                }
                            }

                            Flickable {
                                id: sshGuideFlick
                                visible: sshCheckCard.isIssue && root.sshGuideExpanded
                                Layout.fillWidth: true
                                Layout.preferredHeight: Math.min(sshGuide.implicitHeight, 260)
                                clip: true
                                contentWidth: width
                                contentHeight: sshGuide.implicitHeight
                                boundsBehavior: Flickable.StopAtBounds
                                ScrollBar.vertical: FluxScrollBar {}

                                RemoteAccessGuide {
                                    id: sshGuide
                                    width: sshGuideFlick.width - 10
                                    profilesVm: root.profilesVm
                                    actionHandler: root.runGuideAction
                                    guide: (root.profilesVm && sshCheckCard.isIssue)
                                        ? root.profilesVm.troubleshootingGuide(sshCheckCard.checkState, "") : ({})
                                    logText: root.profilesVm ? root.profilesVm.sshCheckOutput : ""
                                }
                            }
                        }
                    }

                    // B. EXPANDABLE TOKEN CONFIGURATION CARD & STATUS BANNERS
                    TokenConfigCard {
                        id: tokenConfigCard
                        Layout.fillWidth: true
                        profilesVm: root.profilesVm
                        showTokenConfig: root.showTokenConfig
                        onRequestShowTokenConfig: root.showTokenConfig = true
                        onRequestHideTokenConfig: root.showTokenConfig = false
                    }

                    // C. REPOSITORY SELECTOR & EXPANDABLE LIST
                    RemoteRepoPicker {
                        id: repoPicker
                        Layout.fillWidth: true
                        visible: root.profilesVm ? (root.profilesVm.activeProfileId !== "url") : false
                        profilesVm: root.profilesVm
                        selectedRepoFullName: root.selectedRepoFullName
                        selectedRepoIsPrivate: root.selectedRepoIsPrivate
                        selectedUrlText: urlField.text
                        showRepoPicker: root.showRepoPicker
                        repoTouched: root.repoTouched
                        onTogglePickerRequested: {
                            root.repoTouched = true;
                            root.showRepoPicker = !root.showRepoPicker;
                        }
                        onOpenPickerRequested: root.showRepoPicker = true
                        onInsertTokenRequested: root.showTokenConfig = true
                        onRepoChosen: function(fullName, isPrivate, cloneUrl, defaultDirName) {
                            root.selectedRepoFullName = fullName;
                            root.selectedRepoIsPrivate = isPrivate;
                            urlField.text = cloneUrl;
                            dirNameField.text = defaultDirName;
                            dirNameField.propertyModified = true;
                            root.showRepoPicker = false;
                            root.repoTouched = false;
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
                            color: root.theme.textMuted
                            font.pixelSize: 12
                            font.bold: true
                        }

                        FluxTextField {
                            id: urlField
                            Layout.fillWidth: true
                            Layout.preferredHeight: 34
                            enabled: !(root.effectiveHubVm && root.effectiveHubVm.isCloning)
                            placeholderText: "es. https://github.com/utente/progetto.git o git@github.com:..."
                            font.pixelSize: 13
                            backgroundColor: (root.urlMissing && urlField.text.trim().length === 0) ? root.theme.dangerBg : root.theme.inputBg
                            borderColor: (root.urlMissing && urlField.text.trim().length === 0) ? root.theme.danger : root.theme.border
                            onTextChanged: {
                                if (root.urlMissing && text.trim().length > 0) root.urlMissing = false;
                                if (root.effectiveHubVm && text.length > 0 && root.profilesVm && root.profilesVm.activeProfileId === "url") {
                                    const extracted = root.effectiveHubVm.extractRepoNameFromUrl(text);
                                    if (dirNameField.text.length === 0 || dirNameField.propertyModified !== true) {
                                        dirNameField.text = extracted;
                                    }
                                }
                            }
                            onAccepted: {
                                if (cloneButton.enabled) cloneButton.clicked();
                            }
                        }

                        // Warning when URL is missing in direct URL mode
                        Rectangle {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 32
                            visible: root.urlMissing && urlField.text.trim().length === 0 && (root.profilesVm && root.profilesVm.activeProfileId === "url")
                            color: root.theme.dangerBg
                            radius: 6
                            border.color: root.theme.danger
                            border.width: 1

                            RowLayout {
                                anchors.fill: parent
                                anchors.leftMargin: 10
                                anchors.rightMargin: 10
                                spacing: 8

                                Text {
                                    text: "⚠️ Inserisci l'URL del repository remoto da clonare."
                                    color: root.theme.dangerText
                                    font.pixelSize: 11
                                    font.bold: true
                                    Layout.fillWidth: true
                                }
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
                            color: root.theme.textMuted
                            font.pixelSize: 12
                            font.bold: true
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 8

                            FluxTextField {
                                id: destFolderField
                                Layout.fillWidth: true
                                Layout.preferredHeight: 34
                                enabled: !(root.effectiveHubVm && root.effectiveHubVm.isCloning)
                                placeholderText: "Seleziona cartella in cui scaricare il progetto..."
                                font.pixelSize: 13
                                backgroundColor: (root.destFolderMissing && destFolderField.text.trim().length === 0) ? root.theme.dangerBg : root.theme.inputBg
                                borderColor: (root.destFolderMissing && destFolderField.text.trim().length === 0) ? root.theme.danger : root.theme.border
                                onTextChanged: {
                                    if (root.destFolderMissing && text.trim().length > 0) root.destFolderMissing = false;
                                }
                                onAccepted: {
                                    if (cloneButton.enabled) cloneButton.clicked();
                                }
                            }

                            FluxActionButton {
                                text: "Sfoglia..."
                                Layout.preferredHeight: 34
                                Layout.preferredWidth: 84
                                enabled: !(root.effectiveHubVm && root.effectiveHubVm.isCloning)
                                textColor: root.theme.accentCyan
                                onClicked: cloneParentFolderDialog.open()
                            }
                        }

                        // Prominent alert when destination folder is missing on clone attempt
                        Rectangle {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 38
                            visible: root.destFolderMissing && destFolderField.text.trim().length === 0
                            color: root.theme.dangerBg
                            radius: 6
                            border.color: root.theme.danger
                            border.width: 1

                            RowLayout {
                                anchors.fill: parent
                                anchors.leftMargin: 12
                                anchors.rightMargin: 12
                                spacing: 8

                                Text {
                                    text: "⚠️ Cartella di destinazione mancante! Seleziona dove scaricare il repository:"
                                    color: root.theme.dangerText
                                    font.pixelSize: 11
                                    font.bold: true
                                    Layout.fillWidth: true
                                }

                                FluxActionButton {
                                    text: "📁 Sfoglia..."
                                    variant: "danger"
                                    Layout.preferredHeight: 26
                                    onClicked: {
                                        root.destFolderMissing = false;
                                        cloneParentFolderDialog.open();
                                    }
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
                                color: root.theme.textMuted
                                font.pixelSize: 12
                                font.bold: true
                            }

                            FluxTextField {
                                id: dirNameField
                                Layout.fillWidth: true
                                Layout.preferredHeight: 34
                                property bool propertyModified: false
                                enabled: !(root.effectiveHubVm && root.effectiveHubVm.isCloning)
                                placeholderText: "nome-cartella"
                                font.pixelSize: 13
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
                                color: root.theme.textMuted
                                font.pixelSize: 12
                            }

                            FluxTextField {
                                id: displayNameField
                                Layout.fillWidth: true
                                Layout.preferredHeight: 34
                                enabled: !(root.effectiveHubVm && root.effectiveHubVm.isCloning)
                                placeholderText: "Come mostrare in Git Flux"
                                font.pixelSize: 13
                                onAccepted: {
                                    if (cloneButton.enabled) cloneButton.clicked();
                                }
                            }
                        }
                    }

                    // Destination Path Preview
                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 38
                        visible: destFolderField.text.trim().length > 0 && dirNameField.text.trim().length > 0 && !(root.effectiveHubVm && root.effectiveHubVm.isCloning)
                        color: root.theme.surface
                        radius: 6
                        border.color: root.theme.borderSubtle
                        border.width: 1

                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 10
                            anchors.rightMargin: 10
                            spacing: 8

                            Text {
                                text: "📍 Cartella finale:"
                                color: root.theme.textMuted
                                font.pixelSize: 11
                                font.bold: true
                            }

                            Text {
                                text: destFolderField.text.trim() + "/" + dirNameField.text.trim()
                                color: root.theme.accentCyan
                                font.pixelSize: 11
                                font.bold: true
                                elide: Text.ElideMiddle
                                Layout.fillWidth: true
                            }

                            Text {
                                text: "(Premi 'Avvia Clonazione' per iniziare)"
                                color: root.theme.textMuted
                                font.pixelSize: 10
                                font.italic: true
                            }
                        }
                    }

                    // Progress Indicator with Real-Time Percentage
                    Rectangle {
                        Layout.fillWidth: true
                        visible: Boolean(root.effectiveHubVm && root.effectiveHubVm.isCloning)
                        color: root.theme.infoBg
                        radius: 8
                        border.color: root.theme.info
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
                                    color: root.theme.textPrimary
                                    font.pixelSize: 12
                                    font.bold: true
                                    Layout.fillWidth: true
                                }

                                Text {
                                    visible: Boolean(root.effectiveHubVm && root.effectiveHubVm.cloneProgressPercent > 0)
                                    text: root.effectiveHubVm ? (root.effectiveHubVm.cloneProgressPercent + "%") : ""
                                    color: root.theme.accentCyan
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
                                    color: root.theme.borderSubtle
                                    radius: 4
                                }
                                contentItem: Item {
                                    implicitHeight: 8
                                    clip: true
                                    Rectangle {
                                        width: cloneProgressBar.indeterminate
                                            ? parent.width * 0.35
                                            : Math.max(8, parent.width * cloneProgressBar.visualPosition)
                                        height: parent.height
                                        radius: 4
                                        x: cloneProgressBar.indeterminate ? indetAnim.animX : 0
                                        gradient: Gradient {
                                            orientation: Gradient.Horizontal
                                            GradientStop {
                                                position: 0.0; color: root.theme.accentBlue
                                            }
                                            GradientStop {
                                                position: 1.0; color: root.theme.accentCyan
                                            }
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

                            SelectableText {
                                text: root.effectiveHubVm ? root.effectiveHubVm.cloneStatusMessage : ""
                                color: root.theme.textMuted
                                font.pixelSize: 11
                                font.family: "monospace"
                                wrapMode: TextEdit.NoWrap
                                clip: true
                                Layout.fillWidth: true
                            }
                        }
                    }

                    // Error Banner
                    Rectangle {
                        id: errorBanner
                        Layout.fillWidth: true
                        visible: Boolean(root.effectiveHubVm && root.effectiveHubVm.lastError.length > 0 && !root.effectiveHubVm.isCloning)
                        color: root.theme.dangerBg
                        radius: 6
                        border.color: root.theme.danger
                        border.width: 1
                        implicitHeight: errorCol.implicitHeight + 16

                        readonly property string issue: root.effectiveHubVm ? root.effectiveHubVm.cloneIssue : ""
                        readonly property string rawLog: root.effectiveHubVm ? root.effectiveHubVm.cloneErrorLog : ""

                        ColumnLayout {
                            id: errorCol
                            anchors.fill: parent
                            anchors.margins: 8
                            spacing: 8

                            RowLayout {
                                Layout.fillWidth: true
                                spacing: 8

                                // Selectable: the message can be copied like any log.
                                SelectableText {
                                    id: errorText
                                    Layout.fillWidth: true
                                    text: root.effectiveHubVm ? root.effectiveHubVm.lastError : ""
                                    color: root.theme.dangerText
                                    font.pixelSize: 12
                                    font.bold: errorBanner.issue.length > 0
                                }

                                GuideButton {
                                    visible: errorBanner.issue.length === 0
                                    Layout.alignment: Qt.AlignTop
                                    text: "Copia"
                                    accent: root.theme.dangerText
                                    onClicked: if (root.profilesVm) root.profilesVm.copyToClipboard(
                                        errorBanner.rawLog.length > 0 ? errorBanner.rawLog : errorText.text)
                                }
                            }

                            // Recognised problem: guided fix + copyable git log
                            Flickable {
                                id: cloneGuideFlick
                                visible: errorBanner.issue.length > 0
                                Layout.fillWidth: true
                                Layout.preferredHeight: Math.min(cloneGuide.implicitHeight, 280)
                                clip: true
                                contentWidth: width
                                contentHeight: cloneGuide.implicitHeight
                                boundsBehavior: Flickable.StopAtBounds
                                ScrollBar.vertical: FluxScrollBar {}

                                RemoteAccessGuide {
                                    id: cloneGuide
                                    width: cloneGuideFlick.width - 10
                                    profilesVm: root.profilesVm
                                    actionHandler: root.runGuideAction
                                    host: root.effectiveHubVm ? root.effectiveHubVm.cloneIssueHost : ""
                                    guide: (root.profilesVm && errorBanner.issue.length > 0)
                                        ? root.profilesVm.troubleshootingGuide(errorBanner.issue, host) : ({})
                                    logText: errorBanner.rawLog
                                    showLog: true
                                }
                            }
                        }
                    }
                }
            }
        }

        // ==========================================
        // 3. MODAL FOOTER
        // ==========================================
        FluxModalFooter {
            Layout.fillWidth: true
            footerHeight: 54
            horizontalMargin: 18
            buttonSpacing: 10

            Item {
                Layout.fillWidth: true
            }

            FluxActionButton {
                text: "Annulla"
                Layout.preferredHeight: 36
                leftPadding: 16
                rightPadding: 16
                fontPixelSize: 12
                enabled: !(root.effectiveHubVm && root.effectiveHubVm.isCloning)
                onClicked: {
                    root.resetFormAndCache();
                    root.close();
                }
            }

            FluxActionButton {
                id: cloneButton
                Layout.preferredHeight: 36
                leftPadding: 18
                rightPadding: 18
                fontPixelSize: 12
                fontBold: true
                variant: (root.effectiveHubVm && root.effectiveHubVm.isCloning)
                    ? ""
                    : (urlField.text.trim().length > 0 ? "primary" : "")
                textColor: (root.effectiveHubVm && root.effectiveHubVm.isCloning)
                    ? root.theme.accentCyan
                    : (urlField.text.trim().length > 0 ? root.theme.selectionText : root.theme.textMuted)
                baseBorderColor: ((root.effectiveHubVm && root.effectiveHubVm.isCloning) || urlField.text.trim().length > 0)
                    ? root.theme.accentCyan
                    : root.theme.border
                text: (root.effectiveHubVm && root.effectiveHubVm.isCloning)
                    ? (root.effectiveHubVm.cloneProgressPercent > 0
                        ? ("⏳ " + root.effectiveHubVm.cloneProgressPercent + "% In corso...")
                        : "⏳ Clonazione in corso...")
                    : (root.selectedRepoFullName.length > 0
                        ? "⬇ Avvia Clonazione (" + (dirNameField.text.length > 0 ? dirNameField.text : root.selectedRepoFullName) + ")"
                        : "⬇ Avvia Clonazione")
                enabled: !(root.effectiveHubVm && root.effectiveHubVm.isCloning)
                onClicked: {
                    if (urlField.text.trim().length === 0) {
                        if (root.profilesVm && root.profilesVm.activeProfileId === "url") {
                            root.urlMissing = true;
                            urlField.forceActiveFocus();
                        } else {
                            root.repoTouched = true;
                            root.showRepoPicker = true;
                            repoPicker.focusSearch();
                        }
                        return;
                    }
                    if (destFolderField.text.trim().length === 0) {
                        root.destFolderMissing = true;
                        destFolderField.forceActiveFocus();
                        return;
                    }
                    root.repoTouched = false;
                    root.destFolderMissing = false;
                    root.urlMissing = false;
                    const vm = root.effectiveHubVm;
                    if (vm) {
                        vm.cloneRepositoryAsync(
                            urlField.text.trim(),
                            destFolderField.text.trim(),
                            dirNameField.text.trim(),
                            displayNameField.text.trim()
                        );
                    }
                }
            }
        }
    }
}
