import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Dialog {
    id: root
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

    background: Rectangle {
        color: "#111824"
        radius: 14
        border.color: root.requiresUnconditionalForce ? "#7F1D1D" : "#78350F"
        border.width: 1

        Rectangle {
            z: -2
            anchors.fill: parent
            anchors.margins: -8
            radius: 20
            color: "#000000"
            opacity: 0.30
        }
        Rectangle {
            z: -1
            anchors.fill: parent
            anchors.margins: -2
            radius: 16
            color: "#000000"
            opacity: 0.40
        }
    }

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
                text: root.requiresUnconditionalForce
                    ? "⚠ Force Push Richiesto (--force)"
                    : "⚠ Force Push Richiesto"
                color: root.requiresUnconditionalForce ? "#F87171" : "#F59E0B"
                font.bold: true
                font.pixelSize: 13
            }

            Item {
                Layout.fillWidth: true
            }

            Button {
                id: closeBtn
                text: "✕"
                Layout.preferredWidth: 26
                Layout.preferredHeight: 26
                hoverEnabled: true
                background: Rectangle {
                    radius: 7
                    color: closeBtn.hovered ? "#243247" : "transparent"
                }
                contentItem: Text {
                    text: "✕"
                    color: closeBtn.hovered ? "#F8FAFC" : "#94A3B8"
                    font.bold: true
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
                onClicked: root.close()
            }
        }
    }

    contentItem: ColumnLayout {
        spacing: 12

        // Target branch & remote summary pill
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 36
            color: "#0D131D"
            radius: 9
            border.color: "#222F42"
            border.width: 1

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 12
                anchors.rightMargin: 12
                spacing: 8

                Text {
                    text: "Destinazione:"
                    color: "#94A3B8"
                    font.pixelSize: 11
                }

                Text {
                    text: (root.branchName.length > 0 ? (root.branchName + "  →  ") : "") + root.remoteName
                    color: "#38BDF8"
                    font.bold: true
                    font.pixelSize: 11
                    elide: Text.ElideRight
                    Layout.fillWidth: true
                }

                Rectangle {
                    Layout.preferredHeight: 22
                    implicitWidth: modeBadgeText.implicitWidth + 14
                    radius: 6
                    color: root.requiresUnconditionalForce ? "#3B1219" : "#451A03"
                    border.color: root.requiresUnconditionalForce ? "#EF4444" : "#D97706"
                    border.width: 1

                    Text {
                        id: modeBadgeText
                        anchors.centerIn: parent
                        text: (root.requiresUnconditionalForce ? "--force" : "--force-with-lease")
                              + (root.tags ? " --tags" : "")
                        color: root.requiresUnconditionalForce ? "#FCA5A5" : "#FCD34D"
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
            color: "#F1F5F9"
            font.pixelSize: 12
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
        }

        // Explicit confirmation prompt
        Rectangle {
            Layout.fillWidth: true
            implicitHeight: promptText.implicitHeight + 16
            radius: 9
            color: root.requiresUnconditionalForce ? "#2A1215" : "#261B0D"
            border.color: root.requiresUnconditionalForce ? "#7F1D1D" : "#78350F"
            border.width: 1

            Text {
                id: promptText
                anchors.fill: parent
                anchors.margins: 8
                text: root.requiresUnconditionalForce
                    ? "Vuoi forzare comunque il push con --force? Questa operazione può sovrascrivere commit o tag sul remote non presenti in locale."
                    : "Vuoi procedere con il force push sicuro (--force-with-lease)? La cronologia remota verrà aggiornata alla tua versione locale solo se il remote non è cambiato nel frattempo."
                color: root.requiresUnconditionalForce ? "#FECACA" : "#FDE68A"
                font.pixelSize: 11
                wrapMode: Text.WordWrap
            }
        }

        // Git raw output details
        Rectangle {
            visible: root.details.length > 0
            Layout.fillWidth: true
            Layout.preferredHeight: 92
            color: "#0A0E15"
            radius: 8
            border.color: "#1E293B"
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
                    color: "#94A3B8"
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

            Button {
                id: cancelBtn
                text: "Annulla"
                Layout.preferredHeight: 34
                leftPadding: 14
                rightPadding: 14
                hoverEnabled: true
                background: Rectangle {
                    color: cancelBtn.hovered ? "#1E2B3E" : "#151E2C"
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
                    text: cancelBtn.text
                    color: "#CBD5E1"
                    font.pixelSize: 11
                    font.bold: true
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
                onClicked: root.close()
            }

            Button {
                id: confirmForceBtn
                text: root.requiresUnconditionalForce
                    ? "⚠ Force Push (--force)"
                    : "⬆ Push con Force-With-Lease"
                Layout.preferredHeight: 34
                leftPadding: 16
                rightPadding: 16
                hoverEnabled: true
                background: Rectangle {
                    color: root.requiresUnconditionalForce
                        ? (confirmForceBtn.hovered ? "#DC2626" : "#B91C1C")
                        : (confirmForceBtn.hovered ? "#D97706" : "#B45309")
                    radius: 8
                    border.color: root.requiresUnconditionalForce ? "#EF4444" : "#F59E0B"
                    border.width: 1

                    Behavior on color {
                        ColorAnimation {
                            duration: 120
                        }
                    }
                }
                contentItem: Text {
                    text: confirmForceBtn.text
                    color: "#FFFFFF"
                    font.pixelSize: 11
                    font.bold: true
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
                onClicked: {
                    var targetRemote = root.remoteName;
                    var upstream = root.setUpstream;
                    var pushTags = root.tags;
                    var unconditional = root.requiresUnconditionalForce;
                    root.close();
                    if (root.branchManager) {
                        root.branchManager.pushRemote(targetRemote, upstream, true, pushTags, unconditional);
                    }
                }
            }
        }
    }
}
