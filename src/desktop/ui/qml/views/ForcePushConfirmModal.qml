import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../themes"

Dialog {
    id: root
    readonly property FluxTheme theme: FluxTheme {}
    title: "Conferma Force Push"
    modal: true
    width: 520
    padding: 18
    topPadding: 14
    bottomPadding: 16
    anchors.centerIn: Overlay.overlay

    property var branchManager: null
    property string remoteName: "origin"
    property string branchName: ""
    property bool setUpstream: false
    property bool tags: false
    property bool requiresUnconditionalForce: false
    property string reasonSummary: ""
    property string details: ""

    function openForRequest(remote, branch, upstream, pushTags, unconditional, summary, rawDetails) {
        root.remoteName = remote && remote.length > 0 ? remote : "origin";
        root.branchName = branch ? branch : "";
        root.setUpstream = Boolean(upstream);
        root.tags = Boolean(pushTags);
        root.requiresUnconditionalForce = Boolean(unconditional);
        root.reasonSummary = summary ? summary : "";
        root.details = rawDetails ? rawDetails : "";
        root.open();
    }

    background: FluxModalBackground {
        border.color: root.requiresUnconditionalForce ? root.theme.danger : root.theme.warning
    }

    header: FluxModalHeader {
        titleText: root.requiresUnconditionalForce
            ? "⚠ Force Push Richiesto (--force)"
            : "⚠ Force Push Richiesto"
        titleColor: root.requiresUnconditionalForce ? root.theme.dangerText : root.theme.warning
        onCloseRequested: root.close()
    }

    contentItem: ColumnLayout {
        spacing: 12

        // Target branch & remote summary pill
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 36
            color: root.theme.inputBg
            radius: 9
            border.color: root.theme.border
            border.width: 1

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 12
                anchors.rightMargin: 12
                spacing: 8

                Text {
                    text: "Destinazione:"
                    color: root.theme.textSecondary
                    font.pixelSize: 11
                }

                Text {
                    text: (root.branchName.length > 0 ? (root.branchName + "  →  ") : "") + root.remoteName
                    color: root.theme.borderFocus
                    font.bold: true
                    font.pixelSize: 11
                    elide: Text.ElideRight
                    Layout.fillWidth: true
                }

                Rectangle {
                    Layout.preferredHeight: 22
                    implicitWidth: modeBadgeText.implicitWidth + 14
                    radius: root.theme.radiusXs
                    color: root.requiresUnconditionalForce ? root.theme.dangerBg : root.theme.warningBg
                    border.color: root.requiresUnconditionalForce ? root.theme.danger : root.theme.warning
                    border.width: 1

                    Text {
                        id: modeBadgeText
                        anchors.centerIn: parent
                        text: (root.requiresUnconditionalForce ? "--force" : "--force-with-lease")
                              + (root.tags ? " --tags" : "")
                        color: root.requiresUnconditionalForce ? root.theme.dangerText : root.theme.warningText
                        font.family: "monospace"
                        font.bold: true
                        font.pixelSize: 10
                    }
                }
            }
        }

        // Human-readable explanation
        Text {
            text: root.reasonSummary
            color: root.theme.textPrimary
            font.pixelSize: 12
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
        }

        // Explicit confirmation prompt
        Rectangle {
            Layout.fillWidth: true
            implicitHeight: promptText.implicitHeight + 16
            radius: 9
            color: root.requiresUnconditionalForce ? root.theme.dangerBg : root.theme.warningBg
            border.color: root.requiresUnconditionalForce ? root.theme.danger : root.theme.warning
            border.width: 1

            Text {
                id: promptText
                anchors.fill: parent
                anchors.margins: 8
                text: root.requiresUnconditionalForce
                    ? "Vuoi forzare comunque il push con --force? Questa operazione può sovrascrivere commit o tag sul remote non presenti in locale."
                    : "Vuoi procedere con il force push sicuro (--force-with-lease)? La cronologia remota verrà aggiornata alla tua versione locale solo se il remote non è cambiato nel frattempo."
                color: root.requiresUnconditionalForce ? root.theme.dangerText : root.theme.warningText
                font.pixelSize: 11
                wrapMode: Text.WordWrap
            }
        }

        // Git raw output details
        Rectangle {
            visible: root.details.length > 0
            Layout.fillWidth: true
            Layout.preferredHeight: 92
            color: root.theme.inputBg
            radius: root.theme.radiusSm
            border.color: root.theme.borderSubtle
            border.width: 1
            clip: true

            ScrollView {
                anchors.fill: parent
                anchors.margins: 8
                clip: true

                TextArea {
                    text: root.details
                    readOnly: true
                    wrapMode: TextEdit.Wrap
                    selectByMouse: true
                    color: root.theme.textSecondary
                    font.family: "monospace"
                    font.pixelSize: 10
                    padding: 0
                    background: null
                }
            }
        }

        // Action Buttons
        RowLayout {
            Layout.fillWidth: true
            Layout.topMargin: 2
            spacing: 10

            Item {
                Layout.fillWidth: true
            }

            FluxActionButton {
                text: "Annulla"
                Layout.preferredHeight: 34
                fontBold: true
                onClicked: root.close()
            }

            FluxActionButton {
                text: root.requiresUnconditionalForce
                    ? "⚠ Force Push (--force)"
                    : "⬆ Push con Force-With-Lease"
                Layout.preferredHeight: 34
                leftPadding: 16
                rightPadding: 16
                fontBold: true
                baseColor: root.requiresUnconditionalForce ? root.theme.dangerBg : root.theme.warningBg
                hoverColor: root.requiresUnconditionalForce ? root.theme.danger : root.theme.warning
                baseBorderColor: root.requiresUnconditionalForce ? root.theme.danger : root.theme.warning
                textColor: root.requiresUnconditionalForce ? root.theme.dangerText : root.theme.warningText
                hoverTextColor: root.theme.selectionText
                onClicked: {
                    const targetRemote = root.remoteName;
                    const upstream = root.setUpstream;
                    const pushTags = root.tags;
                    const unconditional = root.requiresUnconditionalForce;
                    root.close();
                    if (root.branchManager) {
                        root.branchManager.pushRemote(targetRemote, upstream, true, pushTags, unconditional);
                    }
                }
            }
        }
    }
}
