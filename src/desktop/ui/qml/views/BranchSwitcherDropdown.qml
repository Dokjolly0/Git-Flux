import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root

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
                ? "#1E2D42"
                : (parent.hovered || branchPopup.opened ? "#1A2639" : "#121A27")
            radius: 9
            border.color: root.branchManager && root.branchManager.hasConflicts
                ? "#EF4444"
                : (branchPopup.opened || parent.hovered ? "#38BDF8" : "#243247")
            border.width: 1

            Behavior on color {
                ColorAnimation {
                    duration: 120
                }
            }
            Behavior on border.color {
                ColorAnimation {
                    duration: 120
                }
            }
        }

        contentItem: RowLayout {
            spacing: 7

            Rectangle {
                Layout.preferredWidth: 20
                Layout.preferredHeight: 20
                radius: 6
                color: root.branchManager && root.branchManager.hasConflicts
                    ? "#3B1219"
                    : (root.isDetached ? "#451A03" : "#0F2942")

                Text {
                    anchors.centerIn: parent
                    text: root.isDetached ? "⚠" : "⎇"
                    color: root.branchManager && root.branchManager.hasConflicts
                        ? "#EF4444"
                        : (root.isDetached ? "#F59E0B" : "#38BDF8")
                    font.bold: true
                    font.pixelSize: 12
                }
            }

            Text {
                text: (root.isDetached || root.currentBranch.length === 0) ? "HEAD detached" : root.currentBranch
                color: root.isDetached ? "#FBBF24" : "#F8FAFC"
                font.bold: true
                font.pixelSize: 12
                elide: Text.ElideRight
                Layout.maximumWidth: 170
            }

            Text {
                text: "▾"
                color: branchPopup.opened ? "#38BDF8" : "#94A3B8"
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

        background: Rectangle {
            color: "#111824"
            radius: 14
            border.color: "#28374E"
            border.width: 1

            Rectangle {
                z: -3
                anchors.fill: parent
                anchors.margins: -12
                anchors.topMargin: -8
                anchors.bottomMargin: -16
                radius: 24
                color: "#000000"
                opacity: 0.18
            }
            Rectangle {
                z: -2
                anchors.fill: parent
                anchors.margins: -6
                anchors.topMargin: -4
                anchors.bottomMargin: -9
                radius: 19
                color: "#000000"
                opacity: 0.26
            }
            Rectangle {
                z: -1
                anchors.fill: parent
                anchors.margins: -2
                radius: 16
                color: "#000000"
                opacity: 0.36
            }

            Rectangle {
                anchors.top: parent.top
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.topMargin: 1
                anchors.leftMargin: 12
                anchors.rightMargin: 12
                height: 1
                color: "#FFFFFF"
                opacity: 0.07
            }
        }

        ColumnLayout {
            anchors.fill: parent
            spacing: 8

            // Branch search input capsule
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 34
                radius: 8
                color: "#0A0E15"
                border.color: searchField.activeFocus ? "#38BDF8" : "#222F42"
                border.width: 1

                Behavior on border.color {
                    ColorAnimation {
                        duration: 120
                    }
                }

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 10
                    anchors.rightMargin: 8
                    spacing: 6

                    Text {
                        text: "🔍"
                        font.pixelSize: 11
                        color: "#64748B"
                    }

                    TextField {
                        id: searchField
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        placeholderText: "Cerca branch..."
                        placeholderTextColor: "#64748B"
                        color: "#F8FAFC"
                        font.pixelSize: 12
                        selectByMouse: true
                        background: Item {}
                    }

                    Button {
                        visible: searchField.text.length > 0
                        flat: true
                        Layout.preferredWidth: 20
                        Layout.preferredHeight: 20
                        onClicked: searchField.text = ""
                        background: Rectangle {
                            radius: 5
                            color: parent.hovered ? "#1E293B" : "transparent"
                        }
                        contentItem: Text {
                            text: "✕"
                            color: "#94A3B8"
                            font.pixelSize: 10
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }
                    }
                }
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
                    id: delegateRect
                    readonly property bool matchesFilter: searchField.text.length === 0
                        || model.name.toLowerCase().includes(searchField.text.toLowerCase())
                    width: branchListView.width
                    height: matchesFilter ? 34 : 0
                    radius: 8
                    color: itemMouseArea.containsMouse
                        ? "#1C2A40"
                        : (model.isCurrent ? "#13263B" : "transparent")
                    border.color: model.isCurrent
                        ? "#1E4066"
                        : (itemMouseArea.containsMouse ? "#283A54" : "transparent")
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
                            radius: 6
                            color: model.isCurrent ? "#064E3B" : "transparent"

                            Text {
                                anchors.centerIn: parent
                                text: model.isCurrent ? "✓" : "⎇"
                                color: model.isCurrent ? "#34D399" : (model.isRemote ? "#64748B" : "#94A3B8")
                                font.bold: true
                                font.pixelSize: 11
                            }
                        }

                        Text {
                            text: model.shortName
                            color: model.isCurrent ? "#38BDF8" : (model.isRemote ? "#94A3B8" : "#F1F5F9")
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
                            radius: 6
                            color: "#192334"
                            border.color: "#29384D"
                            border.width: 1

                            Text {
                                id: remoteBadgeLabel
                                anchors.centerIn: parent
                                text: "remote"
                                color: "#94A3B8"
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
                color: "#222F42"
            }

            // New Branch button
            Button {
                Layout.fillWidth: true
                Layout.preferredHeight: 34
                hoverEnabled: true
                background: Rectangle {
                    color: parent.down ? "#0369A1" : (parent.hovered ? "#0EA5E9" : "#0284C7")
                    radius: 8
                    border.color: parent.hovered ? "#7DD3FC" : "#38BDF8"
                    border.width: 1

                    Behavior on color {
                        ColorAnimation {
                            duration: 120
                        }
                    }
                }
                contentItem: RowLayout {
                    spacing: 6
                    anchors.centerIn: parent
                    Text {
                        text: "+"
                        color: "#FFFFFF"
                        font.bold: true
                        font.pixelSize: 14
                    }
                    Text {
                        text: "Nuovo Branch..."
                        color: "#FFFFFF"
                        font.bold: true
                        font.pixelSize: 12
                    }
                }
                onClicked: {
                    branchPopup.close();
                    newBranchNameField.text = "";
                    newBranchDialog.open();
                    newBranchNameField.forceActiveFocus();
                }
            }
        }
    }

    Dialog {
        id: newBranchDialog
        title: "Crea Nuovo Branch"
        modal: true
        width: 400
        padding: 18
        topPadding: 14
        bottomPadding: 16
        anchors.centerIn: Overlay.overlay

        background: FluxModalBackground {}

        header: Rectangle {
            color: "#161F2E"
            height: 46
            radius: 14

            Rectangle {
                anchors.bottom: parent.bottom
                width: parent.width
                height: 14
                color: "#161F2E"
            }

            Rectangle {
                anchors.bottom: parent.bottom
                width: parent.width
                height: 1
                color: "#222F42"
            }

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 16
                anchors.rightMargin: 14

                Text {
                    text: "🌿 Crea Nuovo Branch"
                    color: "#F8FAFC"
                    font.bold: true
                    font.pixelSize: 13
                }

                Item {
                    Layout.fillWidth: true
                }

                Button {
                    Layout.preferredWidth: 26
                    Layout.preferredHeight: 26
                    hoverEnabled: true
                    background: Rectangle {
                        radius: 7
                        color: parent.hovered ? "#243247" : "transparent"
                    }
                    contentItem: Text {
                        text: "✕"
                        color: parent.hovered ? "#F8FAFC" : "#94A3B8"
                        font.bold: true
                        font.pixelSize: 12
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                    onClicked: newBranchDialog.close()
                }
            }
        }

        contentItem: ColumnLayout {
            spacing: 12

            Text {
                text: "Nome del nuovo branch:"
                color: "#CBD5E1"
                font.bold: true
                font.pixelSize: 12
            }

            TextField {
                id: newBranchNameField
                Layout.fillWidth: true
                Layout.preferredHeight: 36
                placeholderText: "es. feature/my-feature"
                placeholderTextColor: "#64748B"
                color: "#F8FAFC"
                font.pixelSize: 12
                selectByMouse: true
                background: Rectangle {
                    color: "#0A0E15"
                    radius: 8
                    border.color: parent.activeFocus ? "#38BDF8" : "#222F42"
                    border.width: 1

                    Behavior on border.color {
                        ColorAnimation {
                            duration: 120
                        }
                    }
                }
                onAccepted: {
                    if (newBranchNameField.text.trim().length > 0) {
                        newBranchDialog.accept();
                    }
                }
            }

            FluxCheckBox {
                id: checkoutCheck
                text: "Esegui checkout immediato"
                checked: true
            }
        }

        footer: Rectangle {
            color: "transparent"
            height: 52

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 18
                anchors.rightMargin: 18
                anchors.bottomMargin: 10
                spacing: 10

                Item {
                    Layout.fillWidth: true
                }

                Button {
                    text: "Annulla"
                    Layout.preferredHeight: 34
                    leftPadding: 14
                    rightPadding: 14
                    hoverEnabled: true
                    background: Rectangle {
                        color: parent.hovered ? "#1E2B3E" : "#151E2C"
                        radius: 8
                        border.color: "#28374E"
                        border.width: 1

                        Behavior on color {
                            ColorAnimation {
                                duration: 120
                            }
                        }
                    }
                    contentItem: Text {
                        text: parent.text
                        color: "#CBD5E1"
                        font.pixelSize: 12
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                    onClicked: newBranchDialog.reject()
                }

                Button {
                    text: "✓ Crea Branch"
                    enabled: newBranchNameField.text.trim().length > 0
                    Layout.preferredHeight: 34
                    leftPadding: 16
                    rightPadding: 16
                    hoverEnabled: true
                    background: Rectangle {
                        color: !parent.enabled
                            ? "#161F2E"
                            : (parent.hovered ? "#0EA5E9" : "#0284C7")
                        radius: 8
                        border.color: !parent.enabled ? "#222F42" : "#38BDF8"
                        border.width: 1

                        Behavior on color {
                            ColorAnimation {
                                duration: 120
                            }
                        }
                    }
                    contentItem: Text {
                        text: parent.text
                        color: parent.enabled ? "#FFFFFF" : "#475569"
                        font.bold: true
                        font.pixelSize: 12
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                    onClicked: newBranchDialog.accept()
                }
            }
        }

        onAccepted: {
            if (newBranchNameField.text.trim().length > 0 && root.branchManager) {
                root.branchManager.createBranch(newBranchNameField.text.trim(), checkoutCheck.checked);
                newBranchNameField.text = "";
            }
        }
    }
}
