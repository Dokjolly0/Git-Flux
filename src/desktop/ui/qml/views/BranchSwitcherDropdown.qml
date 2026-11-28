import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../themes"

Rectangle {
    id: root
    readonly property FluxTheme theme: FluxTheme {}

    property var branchManager: null
    property string currentBranch: branchManager ? branchManager.currentBranch : ""
    property bool isDetached: branchManager ? branchManager.isDetached : false

    implicitWidth: branchBtn.implicitWidth
    implicitHeight: 32
    color: "transparent"

    Button {
        id: branchBtn
        anchors.fill: parent
        hoverEnabled: true
        leftPadding: 10
        rightPadding: 10

        background: Rectangle {
            color: parent.down
                ? root.theme.cardHover
                : (parent.hovered || branchPopup.opened ? root.theme.card : root.theme.surfaceElevated)
            radius: 9
            border.color: root.branchManager && root.branchManager.hasConflicts
                ? root.theme.danger
                : (branchPopup.opened || parent.hovered ? root.theme.borderFocus : root.theme.borderHighlight)
            border.width: 1

            Behavior on color {
                ColorAnimation {
                    duration: root.theme.animFast
                }
            }
            Behavior on border.color {
                ColorAnimation {
                    duration: root.theme.animFast
                }
            }
        }

        contentItem: RowLayout {
            spacing: 7

            Rectangle {
                Layout.preferredWidth: 20
                Layout.preferredHeight: 20
                radius: root.theme.radiusXs
                color: root.branchManager && root.branchManager.hasConflicts
                    ? root.theme.dangerBg
                    : (root.isDetached ? root.theme.warningBg : root.theme.infoBg)

                Text {
                    anchors.centerIn: parent
                    text: root.isDetached ? "⚠" : "⎇"
                    color: root.branchManager && root.branchManager.hasConflicts
                        ? root.theme.danger
                        : (root.isDetached ? root.theme.warning : root.theme.borderFocus)
                    font.bold: true
                    font.pixelSize: 12
                }
            }

            Text {
                text: (root.isDetached || root.currentBranch.length === 0) ? "HEAD detached" : root.currentBranch
                color: root.isDetached ? root.theme.warningText : root.theme.textPrimary
                font.bold: true
                font.pixelSize: 12
                elide: Text.ElideRight
                Layout.maximumWidth: 170
            }

            Text {
                text: "▾"
                color: branchPopup.opened ? root.theme.borderFocus : root.theme.textSecondary
                font.pixelSize: 10
                rotation: branchPopup.opened ? 180 : 0

                Behavior on rotation {
                    NumberAnimation {
                        duration: 140
                        easing.type: Easing.OutCubic
                    }
                }
            }
        }

        onClicked: {
            if (branchPopup.opened) {
                branchPopup.close();
            } else {
                branchPopup.open();
            }
        }
    }

    Popup {
        id: branchPopup
        y: parent.height + 6
        width: 300
        height: 360
        padding: 10
        modal: true
        focus: true
        transformOrigin: Item.TopLeft

        enter: Transition {
            ParallelAnimation {
                NumberAnimation {
                    property: "opacity"
                    from: 0.0
                    to: 1.0
                    duration: 140
                    easing.type: Easing.OutCubic
                }
                NumberAnimation {
                    property: "scale"
                    from: 0.96
                    to: 1.0
                    duration: 150
                    easing.type: Easing.OutCubic
                }
            }
        }

        exit: Transition {
            ParallelAnimation {
                NumberAnimation {
                    property: "opacity"
                    from: 1.0
                    to: 0.0
                    duration: 95
                    easing.type: Easing.InCubic
                }
                NumberAnimation {
                    property: "scale"
                    from: 1.0
                    to: 0.97
                    duration: 95
                    easing.type: Easing.InCubic
                }
            }
        }

        background: FluxModalBackground {}

        ColumnLayout {
            anchors.fill: parent
            spacing: 8

            FluxTextField {
                id: searchField
                Layout.fillWidth: true
                Layout.preferredHeight: 34
                placeholderText: "Cerca branch..."
                showClearButton: true
            }

            // Branch list
            ListView {
                id: branchListView
                SmoothWheelScroller {}
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                spacing: 2
                boundsBehavior: Flickable.StopAtBounds
                maximumFlickVelocity: 6000
                flickDeceleration: 1000
                ScrollBar.vertical: FluxScrollBar {
                }
                model: root.branchManager ? root.branchManager.branchList : null

                delegate: Rectangle {
                    readonly property bool matchesFilter: searchField.text.length === 0
                        || model.name.toLowerCase().includes(searchField.text.toLowerCase())
                    width: branchListView.width
                    height: matchesFilter ? 34 : 0
                    radius: 8
                    color: itemMouseArea.containsMouse
                        ? root.theme.cardHover
                        : (model.isCurrent ? root.theme.infoBg : "transparent")
                    border.color: model.isCurrent
                        ? root.theme.accentBlue
                        : (itemMouseArea.containsMouse ? root.theme.borderHighlight : "transparent")
                    border.width: 1
                    visible: matchesFilter
                    clip: true

                    Behavior on color {
                        ColorAnimation {
                            duration: 110
                        }
                    }

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 10
                        anchors.rightMargin: 10
                        spacing: 8

                        Rectangle {
                            Layout.preferredWidth: 18
                            Layout.preferredHeight: 18
                            radius: root.theme.radiusXs
                            color: model.isCurrent ? root.theme.successBg : "transparent"

                            Text {
                                anchors.centerIn: parent
                                text: model.isCurrent ? "✓" : "⎇"
                                color: model.isCurrent
                                    ? root.theme.successText
                                    : (model.isRemote ? root.theme.textMuted : root.theme.textSecondary)
                                font.bold: true
                                font.pixelSize: 11
                            }
                        }

                        Text {
                            text: model.shortName
                            color: model.isCurrent
                                ? root.theme.borderFocus
                                : (model.isRemote ? root.theme.textSecondary : root.theme.textPrimary)
                            font.bold: model.isCurrent
                            font.pixelSize: 12
                            elide: Text.ElideRight
                            Layout.fillWidth: true
                        }

                        // Remote badge pill
                        Rectangle {
                            visible: model.isRemote
                            Layout.preferredHeight: 18
                            implicitWidth: remoteBadgeLabel.implicitWidth + 12
                            radius: root.theme.radiusXs
                            color: root.theme.surfaceElevated
                            border.color: root.theme.borderHighlight
                            border.width: 1

                            Text {
                                id: remoteBadgeLabel
                                anchors.centerIn: parent
                                text: "remote"
                                color: root.theme.textSecondary
                                font.pixelSize: 9
                                font.bold: true
                            }
                        }
                    }

                    MouseArea {
                        id: itemMouseArea
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            if (!model.isCurrent && root.branchManager) {
                                root.branchManager.checkoutBranch(model.name);
                            }
                            branchPopup.close();
                        }
                    }
                }
            }

            // Separator
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 1
                color: root.theme.border
            }

            // New Branch button
            FluxActionButton {
                Layout.fillWidth: true
                Layout.preferredHeight: 34
                text: "+  Nuovo Branch..."
                fontPixelSize: 12
                fontBold: true
                baseColor: root.theme.accentBlue
                hoverColor: root.theme.accentBlueHover
                baseBorderColor: root.theme.borderFocus
                hoverBorderColor: root.theme.infoText
                textColor: root.theme.selectionText
                onClicked: {
                    branchPopup.close();
                    newBranchDialog.openWithStartPoint("");
                }
            }
        }
    }

    CreateBranchModal {
        id: newBranchDialog
        branchManager: root.branchManager
    }
}
