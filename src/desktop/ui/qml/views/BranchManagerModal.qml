import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Dialog {
    id: root
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
            color: "#0D131D"
            radius: 10
            border.color: "#222F42"
            border.width: 1

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 12
                anchors.rightMargin: 12
                spacing: 8

                Text {
                    text: (root.branchManager && root.branchManager.remoteUrl) ? ("Remote: " + root.branchManager.remoteUrl) : "Remote origin:"
                    color: "#94A3B8"
                    font.bold: true
                    font.pixelSize: 11
                    elide: Text.ElideMiddle
                    Layout.maximumWidth: 250
                }

                Button {
                    visible: root.branchManager && root.branchManager.remoteUrl && root.branchManager.remoteUrl.startsWith("http")
                    text: "🔑 Passa a SSH"
                    Layout.preferredHeight: 30
                    leftPadding: 10
                    rightPadding: 10
                    hoverEnabled: true
                    ToolTip.text: "Converti l'URL in formato SSH (git@...) per autenticarti senza prompt di password"
                    ToolTip.visible: hovered
                    background: Rectangle {
                        color: parent.hovered ? "#1E2B3E" : "#151E2C"
                        radius: 8
                        border.color: parent.hovered ? "#38BDF8" : "#28374E"
                        border.width: 1
                        Behavior on color {
                            ColorAnimation {
                                duration: 120
                            }
                        }
                    }
                    contentItem: Text {
                        text: parent.text
                        color: "#38BDF8"
                        font.pixelSize: 11
                        font.bold: true
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                    onClicked: {
                        if (root.branchManager) root.branchManager.convertRemoteToSsh("origin");
                    }
                }

                FluxActionButton {
                    text: "⬇ Fetch"
                    Layout.preferredHeight: 30
                    leftPadding: 12
                    rightPadding: 12
                    hoverBorderColor: "#344966"
                    textColor: "#E2E8F0"
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
                    hoverBorderColor: "#344966"
                    textColor: "#E2E8F0"
                    fontBold: true
                    onClicked: {
                        if (root.branchManager) root.branchManager.pullRemote("origin");
                    }
                }

                Button {
                    text: "⬆ Push"
                    Layout.preferredHeight: 30
                    leftPadding: 12
                    rightPadding: 12
                    hoverEnabled: true
                    background: Rectangle {
                        color: parent.hovered ? "#0EA5E9" : "#0284C7"
                        radius: 8
                        border.color: "#38BDF8"
                        border.width: 1
                        Behavior on color {
                            ColorAnimation {
                                duration: 120
                            }
                        }
                    }
                    contentItem: Text {
                        text: parent.text
                        color: "#FFFFFF"
                        font.pixelSize: 11
                        font.bold: true
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                    onClicked: {
                        if (root.branchManager) root.branchManager.pushRemote("origin", false);
                    }
                }

                Item {
                    Layout.fillWidth: true
                }

                // If merge conflicts are present, show Abort button
                Button {
                    visible: root.branchManager && root.branchManager.hasConflicts
                    text: "⚠ Annulla Merge (Abort)"
                    Layout.preferredHeight: 30
                    leftPadding: 12
                    rightPadding: 12
                    hoverEnabled: true
                    background: Rectangle {
                        color: parent.hovered ? "#EF4444" : "#DC2626"
                        radius: 8
                        Behavior on color {
                            ColorAnimation {
                                duration: 120
                            }
                        }
                    }
                    contentItem: Text {
                        text: parent.text
                        color: "#FFFFFF"
                        font.bold: true
                        font.pixelSize: 11
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
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
            color: "#0D131D"
            radius: 10
            border.color: "#222F42"
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
                    radius: 8
                    color: model.isCurrent
                        ? "#13263B"
                        : (listMouse.containsMouse ? "#172233" : "#101723")
                    border.color: model.isCurrent
                        ? "#1E4066"
                        : (listMouse.containsMouse ? "#283952" : "#1B2536")
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
                            color: model.isCurrent ? "#064E3B" : "#182232"

                            Text {
                                anchors.centerIn: parent
                                text: model.isCurrent ? "✓" : "⎇"
                                color: model.isCurrent ? "#34D399" : "#94A3B8"
                                font.bold: true
                                font.pixelSize: 12
                            }
                        }

                        ColumnLayout {
                            spacing: 2
                            Layout.fillWidth: true

                            Text {
                                text: model.shortName
                                color: model.isCurrent ? "#38BDF8" : "#F8FAFC"
                                font.bold: model.isCurrent
                                font.pixelSize: 12
                                elide: Text.ElideRight
                                Layout.fillWidth: true
                            }

                            Text {
                                text: (model.upstream.length > 0 ? "upstream: " + model.upstream : "locale") +
                                    (model.ahead > 0 ? " ↑" + model.ahead : "") +
                                    (model.behind > 0 ? " ↓" + model.behind : "")
                                color: "#64748B"
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
                            baseColor: "#182334"
                            hoverColor: "#0284C7"
                            baseBorderColor: "#29384D"
                            hoverBorderColor: "#38BDF8"
                            textColor: "#E2E8F0"
                            hoverTextColor: "#FFFFFF"
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
                            baseColor: "#182334"
                            hoverColor: "#10B981"
                            baseBorderColor: "#29384D"
                            hoverBorderColor: "#34D399"
                            textColor: "#E2E8F0"
                            hoverTextColor: "#FFFFFF"
                            fontBold: true
                            animDuration: 110
                            onClicked: {
                                if (root.branchManager) root.branchManager.mergeBranch(model.name, true);
                            }
                        }

                        // Delete button
                        Button {
                            visible: !model.isCurrent
                            text: "🗑"
                            Layout.preferredHeight: 26
                            Layout.preferredWidth: 28
                            hoverEnabled: true
                            background: Rectangle {
                                color: parent.hovered ? "#3B1219" : "#182334"
                                radius: 7
                                border.color: parent.hovered ? "#EF4444" : "#29384D"
                                border.width: 1
                                Behavior on color {
                                    ColorAnimation {
                                        duration: 110
                                    }
                                }
                            }
                            contentItem: Text {
                                text: parent.text
                                font.pixelSize: 11
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                            }
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
                color: "#94A3B8"
                font.pixelSize: 11
                elide: Text.ElideRight
            }

            FluxActionButton {
                text: "Chiudi"
                Layout.preferredHeight: 32
                leftPadding: 16
                rightPadding: 16
                textColor: "#E2E8F0"
                fontBold: true
                onClicked: root.close()
            }
        }
    }
}
