import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Dialog {
    id: root
    title: "Crea Pull Request"
    modal: true
    width: 540
    height: 460
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
        color: "#0F172A"
        radius: 8
        border.color: "#334155"
        border.width: 1
    }

    header: Rectangle {
        color: "#1E293B"
        height: 44
        radius: 8

        Rectangle {
            anchors.bottom: parent.bottom
            width: parent.width
            height: 8
            color: "#1E293B"
        }

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 16
            anchors.rightMargin: 16

            Text {
                text: "🚀 Crea Pull Request"
                color: "#F8FAFC"
                font.bold: true
                font.pixelSize: 13
            }

            Item { Layout.fillWidth: true }

            Button {
                text: "✕"
                Layout.preferredWidth: 24
                Layout.preferredHeight: 24
                background: Rectangle { color: "transparent" }
                contentItem: Text { text: "✕"; color: "#94A3B8"; font.bold: true; horizontalAlignment: Text.AlignHCenter }
                onClicked: root.close()
            }
        }
    }

    contentItem: ColumnLayout {
        spacing: 12

        // Branch comparison banner
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 40
            color: "#161B22"
            radius: 6
            border.color: "#30363D"

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 12
                anchors.rightMargin: 12
                spacing: 8

                Rectangle {
                    color: "#0E7490"
                    radius: 4
                    Layout.preferredHeight: 22
                    Layout.preferredWidth: Math.min(180, srcText.implicitWidth + 14)
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
                    color: "#1E293B"
                    radius: 4
                    border.color: "#38BDF8"
                    border.width: 1
                    Layout.preferredHeight: 22
                    Layout.preferredWidth: Math.min(180, tgtText.implicitWidth + 14)
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

                Item { Layout.fillWidth: true }

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
            spacing: 4

            Text {
                text: "Titolo della Pull Request:"
                color: "#94A3B8"
                font.pixelSize: 11
                font.bold: true
            }

            TextField {
                id: titleField
                Layout.fillWidth: true
                Layout.preferredHeight: 34
                color: "#F8FAFC"
                font.pixelSize: 12
                placeholderText: "Inserisci titolo della Pull Request..."
                placeholderTextColor: "#475569"
                selectByMouse: true

                background: Rectangle {
                    color: "#020617"
                    radius: 4
                    border.color: titleField.activeFocus ? "#38BDF8" : "#334155"
                    border.width: 1
                }
            }
        }

        // Description
        ColumnLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 4

            Text {
                text: "Descrizione / Note (opzionale):"
                color: "#94A3B8"
                font.pixelSize: 11
                font.bold: true
            }

            ScrollView {
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true

                TextArea {
                    id: descField
                    color: "#F8FAFC"
                    font.pixelSize: 11
                    placeholderText: "Descrivi le modifiche incluse in questa Pull Request..."
                    placeholderTextColor: "#475569"
                    selectByMouse: true
                    wrapMode: TextEdit.Wrap

                    background: Rectangle {
                        color: "#020617"
                        radius: 4
                        border.color: descField.activeFocus ? "#38BDF8" : "#334155"
                        border.width: 1
                    }
                }
            }
        }

        // URL Preview
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 28
            color: "#0D1117"
            radius: 4
            border.color: "#21262D"
            visible: root.calculatedPrUrl.length > 0

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 8
                anchors.rightMargin: 8
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
        color: "#1E293B"
        height: 52
        radius: 8

        Rectangle {
            anchors.top: parent.top
            width: parent.width
            height: 8
            color: "#1E293B"
        }

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 16
            anchors.rightMargin: 16
            spacing: 10

            Item { Layout.fillWidth: true }

            Button {
                text: "Annulla"
                Layout.preferredHeight: 32
                background: Rectangle {
                    color: parent.hovered ? "#334155" : "transparent"
                    radius: 4
                    border.color: "#475569"
                    border.width: 1
                }
                contentItem: Text {
                    text: "Annulla"
                    color: "#94A3B8"
                    font.pixelSize: 12
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
                onClicked: root.close()
            }

            Button {
                id: openBrowserBtn
                text: "🌐 Apri nel Browser"
                Layout.preferredHeight: 32
                Layout.preferredWidth: 150
                enabled: root.calculatedPrUrl.length > 0

                background: Rectangle {
                    color: openBrowserBtn.enabled ? (openBrowserBtn.hovered ? "#0284C7" : "#0284C7") : "#334155"
                    radius: 4
                }
                contentItem: Text {
                    text: "🌐 Apri nel Browser"
                    color: openBrowserBtn.enabled ? "#FFFFFF" : "#64748B"
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
