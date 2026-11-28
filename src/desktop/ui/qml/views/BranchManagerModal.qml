import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../themes"

Dialog {
    id: root
    readonly property FluxTheme theme: FluxTheme {}
    title: "Gestione Branch & Sincronizzazione Remota"
    modal: true
    width: 660
    height: 520
    padding: 18
    topPadding: 14
    bottomPadding: 14
    anchors.centerIn: Overlay.overlay

    property var branchManager: null

    background: FluxModalBackground {}

    header: FluxModalHeader {
        titleText: "⚙ Gestione Branch & Sincronizzazione Remota"
        onCloseRequested: root.close()
    }

    contentItem: ColumnLayout {
        spacing: 12

        // Top Bar: Quick Remote Actions (Fetch, Pull, Push)
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 46
            color: root.theme.inputBg
            radius: 10
            border.color: root.theme.border
            border.width: 1

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 12
                anchors.rightMargin: 12
                spacing: 8

                Text {
                    text: (root.branchManager && root.branchManager.remoteUrl) ? ("Remote: " + root.branchManager.remoteUrl) : "Remote origin:"
                    color: root.theme.textSecondary
                    font.bold: true
                    font.pixelSize: 11
                    elide: Text.ElideMiddle
                    Layout.maximumWidth: 250
                }

                FluxActionButton {
                    visible: root.branchManager && root.branchManager.remoteUrl && root.branchManager.remoteUrl.startsWith("http")
                    text: "🔑 Passa a SSH"
                    Layout.preferredHeight: 30
                    leftPadding: 10
                    rightPadding: 10
                    hoverBorderColor: root.theme.borderFocus
                    textColor: root.theme.borderFocus
                    fontBold: true
                    ToolTip.text: "Converti l'URL in formato SSH (git@...) per autenticarti senza prompt di password"
                    ToolTip.visible: hovered
                    onClicked: {
                        if (root.branchManager) root.branchManager.convertRemoteToSsh("origin");
                    }
                }

                FluxActionButton {
                    text: "⬇ Fetch"
                    Layout.preferredHeight: 30
                    leftPadding: 12
                    rightPadding: 12
                    fontBold: true
                    onClicked: {
                        if (root.branchManager) root.branchManager.fetchRemote("origin");
                    }
                }

                FluxActionButton {
                    text: "⬇ Pull"
                    Layout.preferredHeight: 30
                    leftPadding: 12
                    rightPadding: 12
                    fontBold: true
                    onClicked: {
                        if (root.branchManager) root.branchManager.pullRemote("origin");
                    }
                }

                FluxActionButton {
                    text: "⬆ Push"
                    Layout.preferredHeight: 30
                    leftPadding: 12
                    rightPadding: 12
                    baseColor: root.theme.accentBlue
                    hoverColor: root.theme.accentBlueHover
                    baseBorderColor: root.theme.borderFocus
                    textColor: root.theme.selectionText
                    fontBold: true
                    onClicked: {
                        if (root.branchManager) root.branchManager.pushRemote("origin", false);
                    }
                }

                Item {
                    Layout.fillWidth: true
                }

                // If merge conflicts are present, show Abort button
                FluxActionButton {
                    visible: root.branchManager && root.branchManager.hasConflicts
                    text: "⚠ Annulla Merge (Abort)"
                    Layout.preferredHeight: 30
                    leftPadding: 12
                    rightPadding: 12
                    baseColor: root.theme.dangerBg
                    hoverColor: root.theme.danger
                    baseBorderColor: root.theme.danger
                    textColor: root.theme.dangerText
                    hoverTextColor: root.theme.selectionText
                    fontBold: true
                    onClicked: {
                        if (root.branchManager) root.branchManager.abortMerge();
                    }
                }
            }
        }

        // Branch List with Details
        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            color: root.theme.inputBg
            radius: 10
            border.color: root.theme.border
            border.width: 1
            clip: true

            ListView {
                id: branchList
                SmoothWheelScroller {}
                anchors.fill: parent
                anchors.margins: 6
                spacing: 4
                model: root.branchManager ? root.branchManager.branchList : null
                boundsBehavior: Flickable.StopAtBounds
                maximumFlickVelocity: 6000
                flickDeceleration: 1000
                ScrollBar.vertical: FluxScrollBar {
                }

                delegate: Rectangle {
                    width: branchList.width
                    height: 46
                    radius: root.theme.radiusSm
                    color: model.isCurrent
                        ? root.theme.infoBg
                        : (listMouse.containsMouse ? root.theme.cardHover : root.theme.surfaceElevated)
                    border.color: model.isCurrent
                        ? root.theme.accentBlue
                        : (listMouse.containsMouse ? root.theme.borderHighlight : root.theme.borderSubtle)
                    border.width: 1

                    Behavior on color {
                        ColorAnimation {
                            duration: 110
                        }
                    }

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 12
                        anchors.rightMargin: 12
                        spacing: 10

                        Rectangle {
                            Layout.preferredWidth: 22
                            Layout.preferredHeight: 22
                            radius: 7
                            color: model.isCurrent ? root.theme.successBg : root.theme.card

                            Text {
                                anchors.centerIn: parent
                                text: model.isCurrent ? "✓" : "⎇"
                                color: model.isCurrent ? root.theme.successText : root.theme.textSecondary
                                font.bold: true
                                font.pixelSize: 12
                            }
                        }

                        ColumnLayout {
                            spacing: 2
                            Layout.fillWidth: true

                            Text {
                                text: model.shortName
                                color: model.isCurrent ? root.theme.borderFocus : root.theme.textPrimary
                                font.bold: model.isCurrent
                                font.pixelSize: 12
                                elide: Text.ElideRight
                                Layout.fillWidth: true
                            }

                            Text {
                                text: (model.upstream.length > 0 ? "upstream: " + model.upstream : "locale") +
                                    (model.ahead > 0 ? " ↑" + model.ahead : "") +
                                    (model.behind > 0 ? " ↓" + model.behind : "")
                                color: root.theme.textMuted
                                font.pixelSize: 10
                            }
                        }

                        // Checkout button
                        FluxActionButton {
                            visible: !model.isCurrent
                            text: "Switch"
                            Layout.preferredHeight: 26
                            leftPadding: 10
                            rightPadding: 10
                            cornerRadius: 7
                            baseColor: root.theme.surfaceGlass
                            hoverColor: root.theme.accentBlue
                            baseBorderColor: root.theme.borderHighlight
                            hoverBorderColor: root.theme.borderFocus
                            textColor: root.theme.textPrimary
                            hoverTextColor: root.theme.selectionText
                            fontBold: true
                            animDuration: 110
                            onClicked: {
                                if (root.branchManager) root.branchManager.checkoutBranch(model.name);
                            }
                        }

                        // Merge button
                        FluxActionButton {
                            visible: !model.isCurrent
                            text: "Merge"
                            Layout.preferredHeight: 26
                            leftPadding: 10
                            rightPadding: 10
                            cornerRadius: 7
                            baseColor: root.theme.surfaceGlass
                            hoverColor: root.theme.success
                            baseBorderColor: root.theme.borderHighlight
                            hoverBorderColor: root.theme.successText
                            textColor: root.theme.textPrimary
                            hoverTextColor: root.theme.selectionText
                            fontBold: true
                            animDuration: 110
                            onClicked: {
                                if (root.branchManager) root.branchManager.mergeBranch(model.name, true);
                            }
                        }

                        // Delete button
                        FluxActionButton {
                            visible: !model.isCurrent
                            text: "🗑"
                            Layout.preferredHeight: 26
                            Layout.preferredWidth: 28
                            leftPadding: 0
                            rightPadding: 0
                            cornerRadius: 7
                            baseColor: root.theme.surfaceGlass
                            hoverColor: root.theme.dangerBg
                            baseBorderColor: root.theme.borderHighlight
                            hoverBorderColor: root.theme.danger
                            animDuration: 110
                            onClicked: {
                                if (root.branchManager) root.branchManager.deleteBranch(model.name, false);
                            }
                        }
                    }

                    MouseArea {
                        id: listMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        propagateComposedEvents: true
                    }
                }
            }
        }

        // Bottom status bar & Close button
        RowLayout {
            Layout.fillWidth: true
            spacing: 10

            Text {
                Layout.fillWidth: true
                text: root.branchManager ? root.branchManager.statusMessage : ""
                color: root.theme.textSecondary
                font.pixelSize: 11
                elide: Text.ElideRight
            }

            FluxActionButton {
                text: "Chiudi"
                Layout.preferredHeight: 32
                leftPadding: 16
                rightPadding: 16
                fontBold: true
                onClicked: root.close()
            }
        }
    }
}
