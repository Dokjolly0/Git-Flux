import QtQuick
import QtQuick.Layouts
import "../themes"

ColumnLayout {
    id: tokenRoot
    readonly property FluxTheme theme: FluxTheme {}

    property var profilesVm: null
    property bool showTokenConfig: false
    property bool showTokenText: false

    signal requestShowTokenConfig()
    signal requestHideTokenConfig()

    function clearTokenInput() {
        tokenSecretInput.text = "";
    }

    spacing: 8

    // Expandable Token Configuration Card
    Rectangle {
        Layout.fillWidth: true
        visible: Boolean(tokenRoot.showTokenConfig && tokenRoot.profilesVm && tokenRoot.profilesVm.activeProfileId !== "url")
        implicitHeight: tokenConfigCol.implicitHeight + 16
        color: tokenRoot.theme.canvas
        radius: 6
        border.color: tokenRoot.theme.accentCyan
        border.width: 1

        ColumnLayout {
            id: tokenConfigCol
            anchors.fill: parent
            anchors.margins: 10
            spacing: 8

            Text {
                text: "🔒 Configura Personal Access Token (PAT) per " + (tokenRoot.profilesVm ? tokenRoot.profilesVm.activeProfileDisplayName : "")
                color: tokenRoot.theme.textPrimary
                font.bold: true
                font.pixelSize: 12
            }

            Text {
                text: tokenRoot.profilesVm && tokenRoot.profilesVm.activeProfileId === "bitbucket"
                    ? "Bitbucket ha deprecato le App Password a Luglio 2026. Crea un 'API token with scopes' su id.atlassian.com selezionando l'app Bitbucket e lo scope 'read:repository:bitbucket'."
                    : "Il token viene crittografato sul database locale con Argon2id (KDF) e AES-256-GCM. Sblocca tutti i repository privati e delle organizzazioni."
                color: tokenRoot.theme.textMuted
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
                    placeholderText: tokenRoot.profilesVm && tokenRoot.profilesVm.activeProfileId === "bitbucket"
                        ? "Email Atlassian"
                        : "Username (es. Dokjolly0)"
                    text: tokenRoot.profilesVm ? tokenRoot.profilesVm.activeProfileUsername : ""
                    Keys.onReturnPressed: tokenSecretInput.forceActiveFocus()
                    onAccepted: tokenSecretInput.forceActiveFocus()
                }

                FluxTextField {
                    id: tokenSecretInput
                    Layout.fillWidth: true
                    Layout.preferredHeight: 32
                    placeholderText: tokenRoot.profilesVm && tokenRoot.profilesVm.activeProfileId === "bitbucket"
                        ? "Atlassian Scoped API Token (ATATT...)"
                        : "Personal Access Token (ghp_... o API token)"
                    echoMode: tokenRoot.showTokenText ? TextInput.Normal : TextInput.Password
                    Keys.onReturnPressed: saveTokenBtn.clicked()
                    onAccepted: saveTokenBtn.clicked()
                }

                FluxActionButton {
                    text: tokenRoot.showTokenText ? "🙈" : "👁️"
                    Layout.preferredWidth: 32
                    Layout.preferredHeight: 32
                    onClicked: tokenRoot.showTokenText = !tokenRoot.showTokenText
                }

                FluxActionButton {
                    id: saveTokenBtn
                    text: "💾 Salva e Carica"
                    variant: "primary"
                    Layout.preferredHeight: 32
                    enabled: tokenSecretInput.text.trim().length > 0 || tokenUsernameInput.text.trim().length > 0
                    onClicked: {
                        if (tokenRoot.profilesVm) {
                            tokenRoot.profilesVm.saveProfileToken(
                                tokenRoot.profilesVm.activeProfileId,
                                tokenSecretInput.text,
                                tokenUsernameInput.text
                            );
                            tokenSecretInput.text = "";
                            tokenRoot.requestHideTokenConfig();
                        }
                    }
                }

                FluxActionButton {
                    visible: Boolean(tokenRoot.profilesVm && tokenRoot.profilesVm.activeProfileHasToken)
                    text: "🗑 Rimuovi"
                    variant: "danger"
                    Layout.preferredHeight: 32
                    onClicked: {
                        if (tokenRoot.profilesVm) {
                            tokenRoot.profilesVm.clearProfileToken(tokenRoot.profilesVm.activeProfileId);
                            tokenRoot.requestHideTokenConfig();
                        }
                    }
                }
            }
        }
    }

    // Informative Banner: Public consultation mode
    Rectangle {
        Layout.fillWidth: true
        visible: Boolean(tokenRoot.profilesVm && tokenRoot.profilesVm.activeProfileId !== "url" && !tokenRoot.profilesVm.activeProfileHasToken && !tokenRoot.showTokenConfig)
        implicitHeight: tokenTipLayout.implicitHeight + 14
        color: tokenRoot.theme.infoBg
        radius: 6
        border.color: tokenRoot.theme.info
        border.width: 1

        RowLayout {
            id: tokenTipLayout
            anchors.fill: parent
            anchors.margins: 8
            spacing: 10
            Text { text: "💡"; font.pixelSize: 15 }
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 2
                Text {
                    text: "Consultazione pubblica: visualizzazione limitata ai soli repository pubblici"
                    color: tokenRoot.theme.accentCyan
                    font.bold: true
                    font.pixelSize: 11
                }
                Text {
                    text: "Git Flux è collegato al tuo account, ma senza un Personal Access Token (PAT) GitHub restituisce solo le repo pubbliche. Per mostrare anche i repository privati e aziendali, inserisci un token."
                    color: tokenRoot.theme.infoText
                    font.pixelSize: 11
                    wrapMode: Text.Wrap
                    Layout.fillWidth: true
                }
            }
            FluxActionButton {
                text: "🔑 Inserisci Token"
                variant: "primary"
                Layout.preferredHeight: 28
                onClicked: tokenRoot.requestShowTokenConfig()
            }
        }
    }

    // Success Banner: Token is active
    Rectangle {
        Layout.fillWidth: true
        visible: Boolean(tokenRoot.profilesVm && tokenRoot.profilesVm.activeProfileId !== "url" && tokenRoot.profilesVm.activeProfileHasToken && !tokenRoot.showTokenConfig)
        implicitHeight: authTipLayout.implicitHeight + 14
        color: tokenRoot.theme.successBg
        radius: 6
        border.color: tokenRoot.theme.success
        border.width: 1

        RowLayout {
            id: authTipLayout
            anchors.fill: parent
            anchors.margins: 8
            spacing: 8
            Text { text: "🔒"; font.pixelSize: 14 }
            Text {
                text: "Token PAT attivo (Argon2id + AES-256): accesso a tutti i repository pubblici e privati abilitato."
                color: tokenRoot.theme.successText
                font.bold: true
                font.pixelSize: 11
                Layout.fillWidth: true
            }
            FluxActionButton {
                text: "Modifica"
                variant: "success"
                Layout.preferredHeight: 24
                onClicked: tokenRoot.requestShowTokenConfig()
            }
        }
    }
}
