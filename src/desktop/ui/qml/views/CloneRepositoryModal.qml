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
    property bool showTokenText: false
    property bool showRepoPicker: false
    property string selectedRepoFullName: ""
    property bool selectedRepoIsPrivate: false
    property bool repoTouched: false
    property bool destFolderMissing: false
    property bool urlMissing: false
    property bool sshGuideExpanded: true

    // Re-points the URL field at the selected repository after a protocol change (SSH <-> HTTPS).
    function refreshSelectedRepoUrl() {
        if (!root.profilesVm || root.selectedRepoFullName.length === 0) return;
        for (let i = 0; i < repoListView.count; ++i) {
            const r = root.profilesVm.remoteReposModel.getRepoAt(i);
            if (r.fullName === root.selectedRepoFullName) {
                urlField.text = r.activeCloneUrl;
                break;
            }
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

    // Step-by-step "how to fix it" card for a remote access problem (SSH key, host key, token...).
    // `guide` is GitProfilesViewModel.troubleshootingGuide(); `logText` is the raw ssh/git output.
    component RemoteAccessGuide: ColumnLayout {
        id: guideRoot
        readonly property FluxTheme theme: FluxTheme {}
        property var guide: ({})
        property string host: ""
        property string logText: ""
        property bool showSummary: true
        property string feedback: ""
        property bool showLog: false
        // Injected by the user: inline components can't see the ids of the enclosing file.
        property var profilesVm: null
        property var actionHandler: null // function(action, host) -> feedback text
        spacing: 8

        Timer {
            id: feedbackTimer
            interval: 2500
            onTriggered: guideRoot.feedback = ""
        }

        function act(action) {
            guideRoot.feedback = guideRoot.actionHandler ? guideRoot.actionHandler(action, guideRoot.host) : "";
            if (guideRoot.feedback.length > 0) feedbackTimer.restart();
        }

        Text {
            visible: guideRoot.showSummary && (guideRoot.guide.summary || "").length > 0
            text: guideRoot.guide.summary || ""
            color: guideRoot.theme.textSecondary
            font.pixelSize: 11
            wrapMode: Text.Wrap
            Layout.fillWidth: true
        }

        Repeater {
            model: guideRoot.guide.steps || []

            delegate: RowLayout {
                required property var modelData
                required property int index
                Layout.fillWidth: true
                spacing: 8

                Rectangle {
                    Layout.alignment: Qt.AlignTop
                    Layout.preferredWidth: 20
                    Layout.preferredHeight: 20
                    radius: 10
                    color: guideRoot.theme.infoBg
                    Text {
                        anchors.centerIn: parent
                        text: index + 1
                        color: guideRoot.theme.infoText
                        font.pixelSize: 10
                        font.bold: true
                    }
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 4

                    Text {
                        text: modelData.title
                        color: guideRoot.theme.textPrimary
                        font.pixelSize: 12
                        font.bold: true
                        wrapMode: Text.Wrap
                        Layout.fillWidth: true
                    }

                    SelectableText {
                        visible: (modelData.detail || "").length > 0
                        text: modelData.detail || ""
                        color: guideRoot.theme.textMuted
                        Layout.fillWidth: true
                    }

                    // Copyable command / snippet
                    Rectangle {
                        visible: (modelData.command || "").length > 0
                        Layout.fillWidth: true
                        implicitHeight: cmdRow.implicitHeight + 10
                        radius: 4
                        color: guideRoot.theme.canvas
                        border.color: guideRoot.theme.borderSubtle

                        RowLayout {
                            id: cmdRow
                            anchors.fill: parent
                            anchors.margins: 5
                            spacing: 6

                            SelectableText {
                                text: modelData.command || ""
                                font.family: "monospace"
                                color: guideRoot.theme.infoText
                                Layout.fillWidth: true
                            }

                            GuideButton {
                                text: "Copia"
                                Layout.alignment: Qt.AlignTop
                                onClicked: {
                                    if (guideRoot.profilesVm) guideRoot.profilesVm.copyToClipboard(modelData.command);
                                    guideRoot.feedback = "Comando copiato negli appunti";
                                    feedbackTimer.restart();
                                }
                            }
                        }
                    }

                    RowLayout {
                        visible: (modelData.url || "").length > 0 || (modelData.action || "").length > 0
                        spacing: 6

                        GuideButton {
                            visible: (modelData.action || "").length > 0
                            text: modelData.actionLabel || ""
                            accent: guideRoot.theme.accentTeal
                            onClicked: guideRoot.act(modelData.action)
                        }

                        GuideButton {
                            visible: (modelData.url || "").length > 0
                            text: (modelData.urlLabel || "Apri") + " ↗"
                            onClicked: Qt.openUrlExternally(modelData.url)
                        }
                    }
                }
            }
        }

        // Raw output, selectable and copyable
        RowLayout {
            visible: guideRoot.logText.length > 0
            spacing: 6

            GuideButton {
                text: guideRoot.showLog ? "Nascondi log" : "Mostra log"
                accent: guideRoot.theme.textMuted
                onClicked: guideRoot.showLog = !guideRoot.showLog
            }

            GuideButton {
                text: "Copia log"
                accent: guideRoot.theme.textMuted
                onClicked: {
                    if (guideRoot.profilesVm) guideRoot.profilesVm.copyToClipboard(guideRoot.logText);
                    guideRoot.feedback = "Log copiato negli appunti";
                    feedbackTimer.restart();
                }
            }

            Text {
                visible: guideRoot.feedback.length > 0
                text: "✔ " + guideRoot.feedback
                color: guideRoot.theme.successText
                font.pixelSize: 11
            }
        }

        Text {
            visible: guideRoot.logText.length === 0 && guideRoot.feedback.length > 0
            text: "✔ " + guideRoot.feedback
            color: guideRoot.theme.successText
            font.pixelSize: 11
        }

        Rectangle {
            visible: guideRoot.showLog && guideRoot.logText.length > 0
            Layout.fillWidth: true
            Layout.preferredHeight: Math.min(logEdit.implicitHeight + 12, 140)
            radius: 4
            color: guideRoot.theme.canvas
            border.color: guideRoot.theme.borderSubtle

            Flickable {
                id: logFlick
                anchors.fill: parent
                anchors.margins: 6
                clip: true
                contentWidth: width
                contentHeight: logEdit.implicitHeight
                boundsBehavior: Flickable.StopAtBounds
                ScrollBar.vertical: FluxScrollBar {}

                SelectableText {
                    id: logEdit
                    width: logFlick.width
                    text: guideRoot.logText
                    font.family: "monospace"
                    color: guideRoot.theme.dangerText
                }
            }
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

    // -------------------------------------------------------------
    // ADD CUSTOM SERVER POPUP (Gitea / Forgejo / Codeberg / self-hosted)
    // -------------------------------------------------------------
    Popup {
        id: customServerPopup
        parent: Overlay.overlay
        anchors.centerIn: parent
        width: 460
        modal: true
        focus: true
        padding: 0
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

        property bool showToken: false

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
            if (!root.profilesVm || customHostField.text.trim().length === 0) return;
            root.profilesVm.addCustomProfile(
                customHostField.text,
                customNameField.text,
                customTokenField.text,
                customApiField.text,
                customUserField.text
            );
            customTokenField.text = "";
            customServerPopup.close();
            root.selectedRepoFullName = "";
            root.selectedRepoIsPrivate = false;
            urlField.text = "";
            root.showTokenConfig = false;
            root.showRepoPicker = true;
        }

        background: Rectangle {
            color: root.theme.surfaceElevated
            radius: root.theme.radiusMd
            border.color: root.theme.borderFocus
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
                    color: root.theme.textPrimary
                    font.bold: true
                    font.pixelSize: 14
                }
            }

            Text {
                Layout.leftMargin: 16
                Layout.rightMargin: 16
                Layout.fillWidth: true
                text: "Gitea, Forgejo, Codeberg o qualsiasi istanza compatibile con l'API /api/v1. Il token è opzionale: senza token vengono elencati solo i repository pubblici."
                color: root.theme.textMuted
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

                Text { text: "Host *"; color: root.theme.textSecondary; font.pixelSize: 11; font.bold: true }
                FluxTextField {
                    id: customHostField
                    Layout.fillWidth: true
                    Layout.preferredHeight: 32
                    placeholderText: "gitea.example.com o https://192.168.1.30:3000"
                    onAccepted: customNameField.forceActiveFocus()
                    // A full URL with a port/path is most likely the API base: reuse it.
                    onEditingFinished: {
                        const t = text.trim();
                        if (customApiField.text.trim().length === 0 && t.indexOf("://") > 0) {
                            customApiField.text = t;
                        }
                    }
                }

                Text { text: "Nome"; color: root.theme.textSecondary; font.pixelSize: 11; font.bold: true }
                FluxTextField {
                    id: customNameField
                    Layout.fillWidth: true
                    Layout.preferredHeight: 32
                    placeholderText: "Es. Gitea di casa (default: host)"
                    onAccepted: customApiField.forceActiveFocus()
                }

                Text { text: "URL API"; color: root.theme.textSecondary; font.pixelSize: 11; font.bold: true }
                FluxTextField {
                    id: customApiField
                    Layout.fillWidth: true
                    Layout.preferredHeight: 32
                    placeholderText: "Default: https://<host> (+ /api/v1)"
                    onAccepted: customUserField.forceActiveFocus()
                }

                Text { text: "Username"; color: root.theme.textSecondary; font.pixelSize: 11; font.bold: true }
                FluxTextField {
                    id: customUserField
                    Layout.fillWidth: true
                    Layout.preferredHeight: 32
                    placeholderText: "Opzionale"
                    onAccepted: customTokenField.forceActiveFocus()
                }

                Text { text: "Token"; color: root.theme.textSecondary; font.pixelSize: 11; font.bold: true }
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

            // -------------------------------------------------------------
            // LEFT SIDEBAR: Stacked list of Git Servers/Providers (GitKraken)
            // -------------------------------------------------------------
            Rectangle {
                Layout.preferredWidth: 210
                Layout.fillHeight: true
                color: root.theme.canvas
                border.color: root.theme.borderSubtle
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
                            color: root.theme.textMuted
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
                        spacing: 3
                        model: root.profilesVm ? root.profilesVm.profiles : []

                        delegate: Rectangle {
                            id: providerItem
                            width: providerListView.width - 12
                            x: 6
                            height: 40
                            radius: 8

                            readonly property bool isSelected: root.profilesVm && (root.profilesVm.activeProfileId === modelData.id)

                            color: isSelected ? root.theme.infoBg : (providerMouse.containsMouse ? root.theme.cardHover : "transparent")
                            border.color: isSelected ? root.theme.borderFocus : (providerMouse.containsMouse ? root.theme.border : "transparent")
                            border.width: 1

                            Behavior on color {
                                ColorAnimation {
                                    duration: root.theme.animFast
                                }
                            }

                            // Active Left Indicator Pill
                            Rectangle {
                                x: 3
                                anchors.verticalCenter: parent.verticalCenter
                                width: 3
                                height: 18
                                radius: 1.5
                                color: root.theme.accentCyan
                                visible: providerItem.isSelected
                            }

                            RowLayout {
                                anchors.fill: parent
                                anchors.leftMargin: 16
                                anchors.rightMargin: 12
                                spacing: 10

                                Image {
                                    source: modelData.iconSource ? modelData.iconSource : ""
                                    Layout.preferredWidth: 18
                                    Layout.preferredHeight: 18
                                    Layout.alignment: Qt.AlignVCenter
                                    fillMode: Image.PreserveAspectFit
                                    smooth: true
                                    mipmap: true
                                    opacity: providerItem.isSelected ? 1.0 : (providerMouse.containsMouse ? 0.95 : 0.85)
                                }

                                Text {
                                    text: modelData.displayName
                                    color: providerItem.isSelected ? root.theme.textPrimary : (providerMouse.containsMouse ? root.theme.textPrimary : root.theme.textMuted)
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
                                    color: (modelData.hasSsh || modelData.hasToken) ? root.theme.success : root.theme.borderHighlight
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

                    // Add a self-hosted server (Gitea / Forgejo / Codeberg...)
                    Rectangle {
                        Layout.fillWidth: true
                        Layout.leftMargin: 6
                        Layout.rightMargin: 6
                        Layout.bottomMargin: 8
                        Layout.preferredHeight: 36
                        radius: 8
                        color: addServerMouse.containsMouse ? root.theme.cardHover : "transparent"
                        border.color: addServerMouse.containsMouse ? root.theme.accentCyan : root.theme.border
                        border.width: 1

                        Text {
                            anchors.centerIn: parent
                            text: "＋ Server personalizzato"
                            color: addServerMouse.containsMouse ? root.theme.accentCyan : root.theme.textMuted
                            font.pixelSize: 11
                            font.bold: true
                        }

                        MouseArea {
                            id: addServerMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: customServerPopup.openForNew()
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

                    // ---------------------------------------------------------
                    // B. EXPANDABLE TOKEN CONFIGURATION CARD (Argon2id + AES)
                    // ---------------------------------------------------------
                    Rectangle {
                        Layout.fillWidth: true
                        visible: root.showTokenConfig && root.profilesVm && (root.profilesVm.activeProfileId !== "url")
                        implicitHeight: tokenConfigCol.implicitHeight + 16
                        color: root.theme.canvas
                        radius: 6
                        border.color: root.theme.accentCyan
                        border.width: 1

                        ColumnLayout {
                            id: tokenConfigCol
                            anchors.fill: parent
                            anchors.margins: 10
                            spacing: 8

                            Text {
                                text: "🔒 Configura Personal Access Token (PAT) per " + (root.profilesVm ? root.profilesVm.activeProfileDisplayName : "")
                                color: root.theme.textPrimary
                                font.bold: true
                                font.pixelSize: 12
                            }

                            Text {
                                text: root.profilesVm && root.profilesVm.activeProfileId === "bitbucket"
                                    ? "Bitbucket ha deprecato le App Password a Luglio 2026. Crea un 'API token with scopes' su id.atlassian.com selezionando l'app Bitbucket e lo scope 'read:repository:bitbucket'."
                                    : "Il token viene crittografato sul database locale con Argon2id (KDF) e AES-256-GCM. Sblocca tutti i repository privati e delle organizzazioni."
                                color: root.theme.textMuted
                                font.pixelSize: 11
                                wrapMode: Text.Wrap
                                Layout.fillWidth: true
                            }

                            RowLayout {
                                Layout.fillWidth: true
                                spacing: 8

                                FluxTextField {
                                    id: tokenUsernameInput
                                    Layout.preferredWidth: 160
                                    Layout.preferredHeight: 32
                                    placeholderText: root.profilesVm && root.profilesVm.activeProfileId === "bitbucket"
                                        ? "Email Atlassian"
                                        : "Username (es. Dokjolly0)"
                                    text: root.profilesVm ? root.profilesVm.activeProfileUsername : ""
                                    Keys.onReturnPressed: tokenSecretInput.forceActiveFocus()
                                    onAccepted: tokenSecretInput.forceActiveFocus()
                                }

                                FluxTextField {
                                    id: tokenSecretInput
                                    Layout.fillWidth: true
                                    Layout.preferredHeight: 32
                                    placeholderText: root.profilesVm && root.profilesVm.activeProfileId === "bitbucket"
                                        ? "Atlassian Scoped API Token (ATATT...)"
                                        : "Personal Access Token (ghp_... o API token)"
                                    echoMode: root.showTokenText ? TextInput.Normal : TextInput.Password
                                    Keys.onReturnPressed: saveTokenBtn.clicked()
                                    onAccepted: saveTokenBtn.clicked()
                                }

                                FluxActionButton {
                                    text: root.showTokenText ? "🙈" : "👁️"
                                    Layout.preferredWidth: 32
                                    Layout.preferredHeight: 32
                                    onClicked: root.showTokenText = !root.showTokenText
                                }

                                FluxActionButton {
                                    id: saveTokenBtn
                                    text: "💾 Salva e Carica"
                                    variant: "primary"
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
                                }

                                FluxActionButton {
                                    visible: Boolean(root.profilesVm && root.profilesVm.activeProfileHasToken)
                                    text: "🗑 Rimuovi"
                                    variant: "danger"
                                    Layout.preferredHeight: 32
                                    onClicked: {
                                        if (root.profilesVm) {
                                            root.profilesVm.clearProfileToken(root.profilesVm.activeProfileId);
                                            root.showTokenConfig = false;
                                        }
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
                        color: root.theme.infoBg
                        radius: 6
                        border.color: root.theme.info
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
                                    color: root.theme.accentCyan
                                    font.bold: true
                                    font.pixelSize: 11
                                }
                                Text {
                                    text: "Git Flux è collegato al tuo account, ma senza un Personal Access Token (PAT) GitHub restituisce solo le repo pubbliche. Per mostrare anche i repository privati e aziendali, inserisci un token."
                                    color: root.theme.infoText
                                    font.pixelSize: 11
                                    wrapMode: Text.Wrap
                                    Layout.fillWidth: true
                                }
                            }
                            FluxActionButton {
                                text: "🔑 Inserisci Token"
                                variant: "primary"
                                Layout.preferredHeight: 28
                                onClicked: root.showTokenConfig = true
                            }
                        }
                    }

                    // Success Banner: Token is active (public + private repos unlocked)
                    Rectangle {
                        Layout.fillWidth: true
                        visible: Boolean(root.profilesVm && root.profilesVm.activeProfileId !== "url" && root.profilesVm.activeProfileHasToken && !root.showTokenConfig)
                        implicitHeight: authTipLayout.implicitHeight + 14
                        color: root.theme.successBg
                        radius: 6
                        border.color: root.theme.success
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
                                color: root.theme.successText
                                font.bold: true
                                font.pixelSize: 11
                                Layout.fillWidth: true
                            }
                            FluxActionButton {
                                text: "Modifica"
                                variant: "success"
                                Layout.preferredHeight: 24
                                onClicked: root.showTokenConfig = true
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
                            color: root.theme.textMuted
                            font.pixelSize: 12
                            font.bold: true
                        }

                        // Compact Clickable Selector Box
                        Rectangle {
                            Layout.fillWidth: true
                            height: 42
                            radius: 6
                            color: repoSelectorMouse.containsMouse ? root.theme.cardHover : root.theme.inputBg
                            border.color: (root.repoTouched && !root.showRepoPicker && root.selectedRepoFullName.length === 0)
                                ? root.theme.warning
                                : (root.showRepoPicker ? root.theme.accentCyan : (root.selectedRepoFullName.length > 0 ? root.theme.accentBlue : root.theme.border))
                            border.width: (root.showRepoPicker || (root.repoTouched && !root.showRepoPicker && root.selectedRepoFullName.length === 0)) ? 2 : 1

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
                                        color: root.selectedRepoFullName.length > 0 ? root.theme.textPrimary : root.theme.textMuted
                                        font.pixelSize: 12
                                        font.bold: root.selectedRepoFullName.length > 0
                                        elide: Text.ElideRight
                                        Layout.fillWidth: true
                                    }

                                    Text {
                                        visible: root.selectedRepoFullName.length > 0
                                        text: urlField.text
                                        color: root.theme.accentCyan
                                        font.pixelSize: 10
                                        elide: Text.ElideRight
                                        Layout.fillWidth: true
                                    }
                                }

                                Rectangle {
                                    implicitHeight: 24
                                    implicitWidth: actionBtnText.implicitWidth + 14
                                    radius: 4
                                    color: root.showRepoPicker ? root.theme.accentBlue : (repoSelectorMouse.containsMouse ? root.theme.borderHighlight : root.theme.surfaceElevated)
                                    border.color: root.showRepoPicker ? root.theme.accentCyan : root.theme.borderHighlight
                                    Text {
                                        id: actionBtnText
                                        anchors.centerIn: parent
                                        text: root.showRepoPicker
                                            ? "Chiudi ▲"
                                            : (root.selectedRepoFullName.length > 0 ? "Cambia ▼" : "Scegli ▼")
                                        color: root.showRepoPicker ? root.theme.textPrimary : root.theme.textMuted
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
                                    root.repoTouched = true;
                                    root.showRepoPicker = !root.showRepoPicker;
                                    if (root.showRepoPicker && typeof searchField !== "undefined") {
                                        searchField.forceActiveFocus();
                                    }
                                }
                            }
                        }

                        // Warning if user touched/opened repo selector and closed it without choosing
                        Rectangle {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 32
                            visible: root.repoTouched && !root.showRepoPicker && root.selectedRepoFullName.length === 0 && (root.profilesVm && root.profilesVm.activeProfileId !== "url")
                            color: root.theme.warningBg
                            radius: 6
                            border.color: root.theme.warning
                            border.width: 1

                            RowLayout {
                                anchors.fill: parent
                                anchors.leftMargin: 10
                                anchors.rightMargin: 10
                                spacing: 8

                                Text {
                                    text: "⚠️ Nessun repository selezionato. Clicca sul riquadro sopra per sceglierne uno."
                                    color: root.theme.warningText
                                    font.pixelSize: 11
                                    font.bold: true
                                    Layout.fillWidth: true
                                }

                                Button {
                                    Layout.preferredHeight: 22
                                    onClicked: {
                                        root.showRepoPicker = true;
                                        if (typeof searchField !== "undefined") searchField.forceActiveFocus();
                                    }
                                    contentItem: Text {
                                        text: "Scegli ▼"; color: root.theme.warningText; font.pixelSize: 10; font.bold: true
                                    }
                                    background: Rectangle {
                                        color: root.theme.warning; opacity: 0.35; radius: 4
                                    }
                                }
                            }
                        }

                        // Expandable Dropdown Drawer for Repository Selection
                        Rectangle {
                            Layout.fillWidth: true
                            visible: root.showRepoPicker
                            implicitHeight: repoPickerLayout.implicitHeight + 16
                            color: root.theme.canvas
                            radius: 6
                            border.color: root.theme.accentCyan
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

                                    FluxTextField {
                                        id: searchField
                                        Layout.fillWidth: true
                                        Layout.preferredHeight: 32
                                        placeholderText: "🔍 Cerca tra i tuoi repository (es. FinancialManager, FileHub)..."
                                        showClearButton: true
                                        onTextChanged: {
                                            if (root.profilesVm && root.profilesVm.remoteReposModel) {
                                                root.profilesVm.remoteReposModel.searchFilter = text;
                                            }
                                        }
                                    }

                                    // Counter Badge
                                    Rectangle {
                                        implicitHeight: 32
                                        implicitWidth: countText.implicitWidth + 16
                                        color: root.theme.surfaceElevated
                                        radius: 6
                                        border.color: root.theme.border
                                        Text {
                                            id: countText
                                            anchors.centerIn: parent
                                            text: {
                                                if (!root.profilesVm || !root.profilesVm.remoteReposModel) return "0 repo";
                                                const filtered = root.profilesVm.remoteReposModel.filteredCount;
                                                const total = root.profilesVm.remoteReposModel.totalCount;
                                                return filtered + " di " + total + " repo";
                                            }
                                            color: root.theme.accentCyan
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
                                    color: root.theme.surface
                                    radius: 6
                                    RowLayout {
                                        anchors.centerIn: parent
                                        spacing: 8
                                        ProgressBar {
                                            indeterminate: true; Layout.preferredWidth: 100
                                        }
                                        Text {
                                            text: "Caricamento repository in corso..."
                                            color: root.theme.accentCyan
                                            font.pixelSize: 12
                                        }
                                    }
                                }

                                // Fetch Error Banner
                                Rectangle {
                                    Layout.fillWidth: true
                                    visible: Boolean(root.profilesVm && root.profilesVm.fetchError.length > 0 && !root.profilesVm.isFetching)
                                    implicitHeight: fetchErrText.implicitHeight + 16
                                    color: root.theme.dangerBg
                                    radius: 6
                                    border.color: root.theme.danger
                                    border.width: 1
                                    RowLayout {
                                        anchors.fill: parent
                                        anchors.margins: 8
                                        spacing: 8
                                        Text {
                                            id: fetchErrText
                                            text: root.profilesVm ? root.profilesVm.fetchError : ""
                                            color: root.theme.dangerText
                                            font.pixelSize: 11
                                            wrapMode: Text.Wrap
                                            Layout.fillWidth: true
                                        }
                                        FluxActionButton {
                                            text: "Inserisci Token"
                                            variant: "danger"
                                            Layout.preferredHeight: 24
                                            onClicked: root.showTokenConfig = true
                                        }
                                    }
                                }

                                // Scrollable Remote Repository List View (height 190)
                                Rectangle {
                                    Layout.fillWidth: true
                                    height: 190
                                    color: root.theme.inputBg
                                    radius: 6
                                    border.color: root.theme.borderSubtle
                                    border.width: 1
                                    clip: true

                                    ListView {
                                        id: repoListView
                                        SmoothWheelScroller {}
                                        anchors.fill: parent
                                        anchors.margins: 4
                                        spacing: 4
                                        model: root.profilesVm ? root.profilesVm.remoteReposModel : null
                                        boundsBehavior: Flickable.StopAtBounds
                                        maximumFlickVelocity: 6000
                                        flickDeceleration: 1000
                                        ScrollBar.vertical: FluxScrollBar {
                                        }

                                        delegate: Rectangle {
                                            id: repoCard
                                            width: repoListView.width - 8
                                            height: 50
                                            radius: 5

                                            readonly property bool isSelected: root.selectedRepoFullName === model.fullName

                                            color: isSelected ? root.theme.infoBg : (cardMouse.containsMouse ? root.theme.cardHover : root.theme.surface)
                                            border.color: isSelected ? root.theme.accentCyan : (cardMouse.containsMouse ? root.theme.borderHighlight : root.theme.borderSubtle)
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
                                                            color: repoCard.isSelected ? root.theme.accentCyan : root.theme.textPrimary
                                                            font.pixelSize: 12
                                                            font.bold: true
                                                            elide: Text.ElideRight
                                                        }

                                                        Text {
                                                            visible: model.starsCount > 0
                                                            text: "⭐ " + model.starsCount
                                                            color: root.theme.warning
                                                            font.pixelSize: 10
                                                        }
                                                    }

                                                    Text {
                                                        text: model.description.length > 0 ? model.description : (model.activeCloneUrl)
                                                        color: root.theme.textMuted
                                                        font.pixelSize: 11
                                                        elide: Text.ElideRight
                                                        Layout.fillWidth: true
                                                    }
                                                }

                                                Text {
                                                    visible: repoCard.isSelected
                                                    text: "✔"
                                                    color: root.theme.accentCyan
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
                                                    let targetUrl = model.activeCloneUrl;
                                                    if (!targetUrl || targetUrl.length === 0) {
                                                        targetUrl = (root.profilesVm && root.profilesVm.selectedProtocol === "ssh") ? model.sshUrl : model.httpsUrl;
                                                    }
                                                    if (!targetUrl || targetUrl.length === 0) {
                                                        targetUrl = model.httpsUrl ? model.httpsUrl : model.sshUrl;
                                                    }
                                                    urlField.text = targetUrl;

                                                    const defaultName = model.fullName ? model.fullName.split("/").pop() : model.name;
                                                    dirNameField.text = defaultName;
                                                    dirNameField.propertyModified = true;
                                                    root.showRepoPicker = false;
                                                    root.repoTouched = false;
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
                                            color: root.theme.textMuted
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
                            if (typeof searchField !== "undefined") searchField.forceActiveFocus();
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
