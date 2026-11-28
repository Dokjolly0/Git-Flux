import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../themes"

Flickable {
    id: metaRoot
    readonly property FluxTheme theme: FluxTheme {}

    property var diffViewer: null
    property string monoFont: "monospace"
    property string fileKindIcon: "📦"
    property bool hasHex: false

    readonly property bool hasViewer: diffViewer !== null

    signal switchToHexRequested()

    contentWidth: width
    contentHeight: metaCardCol.implicitHeight + 48
    boundsBehavior: Flickable.StopAtBounds
    ScrollBar.vertical: FluxScrollBar {}

    ColumnLayout {
        id: metaCardCol
        anchors.horizontalCenter: parent.horizontalCenter
        y: 24
        width: Math.min(parent.width - 48, 720)
        spacing: 14

        // Hero summary card
        Rectangle {
            Layout.fillWidth: true
            implicitHeight: heroRow.implicitHeight + 28
            radius: 10
            color: metaRoot.theme.surface
            border.color: metaRoot.theme.borderSubtle
            border.width: 1

            RowLayout {
                id: heroRow
                anchors.fill: parent
                anchors.margins: 14
                spacing: 14

                Rectangle {
                    width: 46
                    height: 46
                    radius: 10
                    color: metaRoot.theme.surfaceElevated
                    border.color: metaRoot.theme.border
                    Text {
                        anchors.centerIn: parent
                        text: metaRoot.fileKindIcon
                        font.pixelSize: 22
                    }
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 3
                    Text {
                        text: metaRoot.hasViewer ? (metaRoot.diffViewer.formatDescription || "File binario") : ""
                        color: metaRoot.theme.textPrimary
                        font.pixelSize: 15
                        font.bold: true
                    }
                    Text {
                        text: metaRoot.hasViewer ? metaRoot.diffViewer.filePath : ""
                        color: metaRoot.theme.textMuted
                        font.family: metaRoot.monoFont
                        font.pixelSize: 11
                        elide: Text.ElideMiddle
                        Layout.fillWidth: true
                    }
                }

                RowLayout {
                    spacing: 6
                    RichPreviewPillButton {
                        visible: metaRoot.hasViewer && metaRoot.diffViewer.hasOldBlob && metaRoot.diffViewer.hasNewBlob
                        text: "↗ Prima"
                        onClicked: if (metaRoot.diffViewer) metaRoot.diffViewer.openPreviewExternally("old")
                    }
                    RichPreviewPillButton {
                        visible: metaRoot.hasViewer && (metaRoot.diffViewer.hasNewBlob || metaRoot.diffViewer.hasOldBlob)
                        text: (metaRoot.hasViewer && metaRoot.diffViewer.hasOldBlob && metaRoot.diffViewer.hasNewBlob) ? "↗ Dopo" : "↗ Apri nel sistema"
                        active: true
                        onClicked: {
                            if (!metaRoot.diffViewer) return;
                            metaRoot.diffViewer.openPreviewExternally(metaRoot.diffViewer.hasNewBlob ? "new" : "old");
                        }
                    }
                }
            }
        }

        // Before vs After Metadata Table
        Rectangle {
            Layout.fillWidth: true
            visible: metaRoot.hasViewer && metaRoot.diffViewer.metadataRows && metaRoot.diffViewer.metadataRows.length > 0
            implicitHeight: metaTableCol.implicitHeight + 16
            radius: 10
            color: metaRoot.theme.inputBg
            border.color: metaRoot.theme.borderSubtle
            border.width: 1

            ColumnLayout {
                id: metaTableCol
                anchors.fill: parent
                anchors.margins: 8
                spacing: 4

                RowLayout {
                    Layout.fillWidth: true
                    Layout.leftMargin: 8
                    Layout.rightMargin: 8
                    Layout.preferredHeight: 24
                    spacing: 10
                    Text { Layout.preferredWidth: 140; text: "PROPRIETÀ"; color: metaRoot.theme.textMuted; font.pixelSize: 10; font.bold: true }
                    Text {
                        visible: metaRoot.hasViewer && metaRoot.diffViewer.hasOldBlob
                        Layout.fillWidth: true; text: "PRIMA"; color: metaRoot.theme.dangerText; font.pixelSize: 10; font.bold: true
                    }
                    Text {
                        visible: metaRoot.hasViewer && metaRoot.diffViewer.hasNewBlob
                        Layout.fillWidth: true; text: (metaRoot.hasViewer && metaRoot.diffViewer.hasOldBlob) ? "DOPO" : "VALORE"; color: metaRoot.theme.successText; font.pixelSize: 10; font.bold: true
                    }
                }

                Rectangle { Layout.fillWidth: true; height: 1; color: metaRoot.theme.borderSubtle }

                Repeater {
                    model: metaRoot.hasViewer ? metaRoot.diffViewer.metadataRows : []
                    delegate: Rectangle {
                        Layout.fillWidth: true
                        implicitHeight: 30
                        radius: 5
                        color: modelData.changed ? metaRoot.theme.surfaceElevated : "transparent"

                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 8
                            anchors.rightMargin: 8
                            spacing: 10

                            Text {
                                Layout.preferredWidth: 140
                                text: modelData.label
                                color: metaRoot.theme.textMuted
                                font.pixelSize: 11
                                font.bold: true
                                elide: Text.ElideRight
                            }
                            Text {
                                visible: metaRoot.hasViewer && metaRoot.diffViewer.hasOldBlob
                                Layout.fillWidth: true
                                text: modelData.oldValue || "—"
                                color: modelData.changed ? metaRoot.theme.dangerText : metaRoot.theme.textSecondary
                                font.family: modelData.label === "SHA-256" ? metaRoot.monoFont : ""
                                font.pixelSize: 11
                                elide: Text.ElideRight
                            }
                            Text {
                                visible: metaRoot.hasViewer && metaRoot.diffViewer.hasNewBlob
                                Layout.fillWidth: true
                                text: modelData.newValue || "—"
                                color: modelData.changed ? metaRoot.theme.successText : metaRoot.theme.textSecondary
                                font.family: modelData.label === "SHA-256" ? metaRoot.monoFont : ""
                                font.pixelSize: 11
                                elide: Text.ElideRight
                            }
                        }
                    }
                }
            }
        }

        // Hint to switch to Hex Inspector
        RowLayout {
            Layout.fillWidth: true
            visible: metaRoot.hasHex
            spacing: 8
            Item { Layout.fillWidth: true }
            RichPreviewPillButton {
                text: "🔍 Ispeziona byte in esadecimale (Hex)"
                onClicked: metaRoot.switchToHexRequested()
            }
            Item { Layout.fillWidth: true }
        }
    }
}
