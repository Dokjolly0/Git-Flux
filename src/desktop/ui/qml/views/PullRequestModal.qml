import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Dialog {
    id: root
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

    background: Rectangle {
        color: "#111824"
        radius: 14
        border.color: "#28374E"
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
                text: "🚀 Crea Pull Request"
                color: "#F8FAFC"
                font.bold: true
                font.pixelSize: 13
            }

            Item {
                Layout.fillWidth: true
            }

            Button {
                text: "✕"
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
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
                onClicked: root.close()
            }
        }
    }

    contentItem: ColumnLayout {
        spacing: 12

        // Branch comparison banner
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 42
            color: "#0D131D"
            radius: 10
            border.color: "#222F42"
            border.width: 1

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 12
                anchors.rightMargin: 12
                spacing: 8

                Rectangle {
                    color: "#0E7490"
                    radius: 7
                    Layout.preferredHeight: 24
                    Layout.preferredWidth: Math.min(180, srcText.implicitWidth + 16)
                    Text {
                        id: srcText
                        anchors.centerIn: parent
                        text: "🌿 " + root.sourceBranch
                        color: "#FFFFFF"
                        font.pixelSize: 11
                        font.bold: true
                        elide: Text.ElideMiddle
                    }
                }

                Text {
                    text: "➔"
                    color: "#94A3B8"
                    font.pixelSize: 14
                    font.bold: true
                }

                Rectangle {
                    color: "#152336"
                    radius: 7
                    border.color: "#38BDF8"
                    border.width: 1
                    Layout.preferredHeight: 24
                    Layout.preferredWidth: Math.min(180, tgtText.implicitWidth + 16)
                    Text {
                        id: tgtText
                        anchors.centerIn: parent
                        text: "🎯 " + root.targetBranch
                        color: "#93C5FD"
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
                    color: "#64748B"
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
                color: "#CBD5E1"
                font.pixelSize: 11
                font.bold: true
            }

            TextField {
                id: titleField
                Layout.fillWidth: true
                Layout.preferredHeight: 36
                color: "#F8FAFC"
                font.pixelSize: 12
                placeholderText: "Inserisci titolo della Pull Request..."
                placeholderTextColor: "#64748B"
                selectByMouse: true

                background: Rectangle {
                    color: "#0A0E15"
                    radius: 8
                    border.color: titleField.activeFocus ? "#38BDF8" : "#222F42"
                    border.width: 1

                    Behavior on border.color {
                        ColorAnimation {
                            duration: 120
                        }
                    }
                }
            }
        }

        // Description
        ColumnLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 5

            Text {
                text: "Descrizione / Note (opzionale):"
                color: "#CBD5E1"
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
                    color: "#F8FAFC"
                    font.pixelSize: 12
                    placeholderText: "Descrivi le modifiche incluse in questa Pull Request..."
                    placeholderTextColor: "#64748B"
                    selectByMouse: true
                    wrapMode: TextEdit.Wrap

                    background: Rectangle {
                        color: "#0A0E15"
                        radius: 8
                        border.color: descField.activeFocus ? "#38BDF8" : "#222F42"
                        border.width: 1

                        Behavior on border.color {
                            ColorAnimation {
                                duration: 120
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
            color: "#0D131D"
            radius: 8
            border.color: "#1E293B"
            border.width: 1
            visible: root.calculatedPrUrl.length > 0

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 10
                anchors.rightMargin: 10
                spacing: 6

                Text {
                    text: "🔗 URL:"
                    color: "#64748B"
                    font.pixelSize: 10
                }

                Text {
                    text: root.calculatedPrUrl
                    color: "#38BDF8"
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
            color: "#EF4444"
            font.pixelSize: 11
            visible: text.length > 0
            wrapMode: Text.WordWrap
        }
    }

    footer: Rectangle {
        color: "#161F2E"
        height: 54
        radius: 14

        Rectangle {
            anchors.top: parent.top
            width: parent.width
            height: 14
            color: "#161F2E"
        }

        Rectangle {
            anchors.top: parent.top
            width: parent.width
            height: 1
            color: "#222F42"
        }

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 18
            anchors.rightMargin: 18
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
                    text: "Annulla"
                    color: "#CBD5E1"
                    font.pixelSize: 12
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
                onClicked: root.close()
            }

            Button {
                id: openBrowserBtn
                text: "🌐 Apri nel Browser"
                Layout.preferredHeight: 34
                Layout.preferredWidth: 160
                enabled: root.calculatedPrUrl.length > 0
                hoverEnabled: true

                background: Rectangle {
                    color: openBrowserBtn.enabled ? (openBrowserBtn.hovered ? "#0EA5E9" : "#0284C7") : "#161F2E"
                    radius: 8
                    border.color: openBrowserBtn.enabled ? "#38BDF8" : "#222F42"
                    border.width: 1

                    Behavior on color {
                        ColorAnimation {
                            duration: 120
                        }
                    }
                }
                contentItem: Text {
                    text: "🌐 Apri nel Browser"
                    color: openBrowserBtn.enabled ? "#FFFFFF" : "#475569"
                    font.pixelSize: 12
                    font.bold: true
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
                onClicked: {
                    var url = root.calculatedPrUrl;
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
}
