import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../themes"

Dialog {
    id: root
    readonly property FluxTheme theme: FluxTheme {}
    title: "Crea Pull Request"
    modal: true
    width: 550
    height: 470
    padding: 18
    topPadding: 14
    bottomPadding: 14
    anchors.centerIn: Overlay.overlay

    property var branchManager: null
    property string sourceBranch: ""
    property string targetBranch: ""

    function openWithBranches(src, tgt) {
        root.sourceBranch = src ? src : "";
        root.targetBranch = tgt ? tgt : ((root.branchManager && root.branchManager.currentBranch) ? root.branchManager.currentBranch : "main");
        if (root.sourceBranch.startsWith("origin/")) {
            root.sourceBranch = root.sourceBranch.substring(7);
        }
        if (root.targetBranch.startsWith("origin/")) {
            root.targetBranch = root.targetBranch.substring(7);
        }
        titleField.text = "Merge " + root.sourceBranch + " into " + root.targetBranch;
        descField.text = "";
        errorText.text = "";
        root.open();
        titleField.forceActiveFocus();
    }

    readonly property string calculatedPrUrl: {
        if (!root.branchManager) return "";
        return root.branchManager.buildPullRequestUrl(root.sourceBranch, root.targetBranch, titleField.text, descField.text);
    }

    background: FluxModalBackground {}

    header: FluxModalHeader {
        titleText: "🚀 Crea Pull Request"
        onCloseRequested: root.close()
    }

    contentItem: ColumnLayout {
        spacing: 12

        // Branch comparison banner
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 42
            color: root.theme.inputBg
            radius: 10
            border.color: root.theme.border
            border.width: 1

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 12
                anchors.rightMargin: 12
                spacing: 8

                Rectangle {
                    color: root.theme.accentTeal
                    radius: 7
                    Layout.preferredHeight: 24
                    Layout.preferredWidth: Math.min(180, srcText.implicitWidth + 16)
                    Text {
                        id: srcText
                        anchors.centerIn: parent
                        text: "🌿 " + root.sourceBranch
                        color: root.theme.selectionText
                        font.pixelSize: 11
                        font.bold: true
                        elide: Text.ElideMiddle
                    }
                }

                Text {
                    text: "➔"
                    color: root.theme.textSecondary
                    font.pixelSize: 14
                    font.bold: true
                }

                Rectangle {
                    color: root.theme.infoBg
                    radius: 7
                    border.color: root.theme.borderFocus
                    border.width: 1
                    Layout.preferredHeight: 24
                    Layout.preferredWidth: Math.min(180, tgtText.implicitWidth + 16)
                    Text {
                        id: tgtText
                        anchors.centerIn: parent
                        text: "🎯 " + root.targetBranch
                        color: root.theme.infoText
                        font.pixelSize: 11
                        font.bold: true
                        elide: Text.ElideMiddle
                    }
                }

                Item {
                    Layout.fillWidth: true
                }

                Text {
                    text: (root.branchManager && root.branchManager.remoteUrl) ? "Remote: origin" : "Nessun remote"
                    color: root.theme.textMuted
                    font.pixelSize: 10
                }
            }
        }

        // Title
        ColumnLayout {
            Layout.fillWidth: true
            spacing: 5

            Text {
                text: "Titolo della Pull Request:"
                color: root.theme.textSecondary
                font.pixelSize: 11
                font.bold: true
            }

            FluxTextField {
                id: titleField
                Layout.fillWidth: true
                placeholderText: "Inserisci titolo della Pull Request..."
            }
        }

        // Description
        ColumnLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 5

            Text {
                text: "Descrizione / Note (opzionale):"
                color: root.theme.textSecondary
                font.pixelSize: 11
                font.bold: true
            }

            ScrollView {
                SmoothWheelScroller {}
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true

                TextArea {
                    id: descField
                    color: root.theme.textPrimary
                    font.pixelSize: 12
                    placeholderText: "Descrivi le modifiche incluse in questa Pull Request..."
                    placeholderTextColor: root.theme.textMuted
                    selectionColor: root.theme.selectionBg
                    selectedTextColor: root.theme.selectionText
                    selectByMouse: true
                    wrapMode: TextEdit.Wrap

                    background: Rectangle {
                        color: root.theme.inputBg
                        radius: root.theme.radiusSm
                        border.color: descField.activeFocus ? root.theme.borderFocus : root.theme.border
                        border.width: 1

                        Behavior on border.color {
                            ColorAnimation {
                                duration: root.theme.animFast
                            }
                        }
                    }
                }
            }
        }

        // URL Preview
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 30
            color: root.theme.inputBg
            radius: root.theme.radiusSm
            border.color: root.theme.borderSubtle
            border.width: 1
            visible: root.calculatedPrUrl.length > 0

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 10
                anchors.rightMargin: 10
                spacing: 6

                Text {
                    text: "🔗 URL:"
                    color: root.theme.textMuted
                    font.pixelSize: 10
                }

                Text {
                    text: root.calculatedPrUrl
                    color: root.theme.borderFocus
                    font.pixelSize: 10
                    elide: Text.ElideMiddle
                    Layout.fillWidth: true
                }
            }
        }

        // Error message if any
        Text {
            id: errorText
            Layout.fillWidth: true
            color: root.theme.danger
            font.pixelSize: 11
            visible: text.length > 0
            wrapMode: Text.WordWrap
        }
    }

    footer: FluxModalFooter {
        footerHeight: 54
        horizontalMargin: 18
        buttonSpacing: 10

        Item {
            Layout.fillWidth: true
        }

        FluxActionButton {
            text: "Annulla"
            Layout.preferredHeight: 34
            fontPixelSize: 12
            onClicked: root.close()
        }

        FluxActionButton {
            text: "🌐 Apri nel Browser"
            Layout.preferredHeight: 34
            Layout.preferredWidth: 160
            enabled: root.calculatedPrUrl.length > 0
            baseColor: root.theme.accentBlue
            hoverColor: root.theme.accentBlueHover
            baseBorderColor: root.theme.borderFocus
            textColor: root.theme.selectionText
            fontPixelSize: 12
            fontBold: true
            onClicked: {
                const url = root.calculatedPrUrl;
                if (url && url.length > 0) {
                    Qt.openUrlExternally(url);
                    root.close();
                } else {
                    errorText.text = "Impossibile calcolare l'URL del remote repository";
                }
            }
        }
    }
}
