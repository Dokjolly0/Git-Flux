import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../themes"

ColumnLayout {
    id: hexRoot
    readonly property FluxTheme theme: FluxTheme {}

    property var diffViewer: null
    property string monoFont: "monospace"
    property bool hexOnlyChanged: false

    readonly property bool hasViewer: diffViewer !== null

    spacing: 8

    RowLayout {
        Layout.fillWidth: true
        spacing: 10

        Text {
            text: hexRoot.hasViewer && hexRoot.diffViewer.hexTruncated
                ? "Anteprima esadecimale (primi 4 KB)"
                : "Dump esadecimale completo"
            color: hexRoot.theme.textMuted
            font.pixelSize: 11
            font.bold: true
        }

        Rectangle {
            visible: hexRoot.hasViewer && hexRoot.diffViewer.hexChangedRows > 0
            implicitHeight: 20
            implicitWidth: hexDiffBadge.implicitWidth + 10
            radius: 5
            color: hexRoot.theme.warningBg
            border.color: hexRoot.theme.warning
            Text {
                id: hexDiffBadge
                anchors.centerIn: parent
                text: (hexRoot.hasViewer ? hexRoot.diffViewer.hexChangedRows : 0) + " righe modificate"
                color: hexRoot.theme.warningText
                font.pixelSize: 10
                font.bold: true
            }
        }

        Item { Layout.fillWidth: true }

        RichPreviewPillButton {
            visible: hexRoot.hasViewer && hexRoot.diffViewer.hasOldBlob && hexRoot.diffViewer.hasNewBlob
            text: "Solo righe modificate"
            active: hexRoot.hexOnlyChanged
            onClicked: hexRoot.hexOnlyChanged = !hexRoot.hexOnlyChanged
        }
    }

    Rectangle {
        Layout.fillWidth: true
        Layout.fillHeight: true
        radius: 8
        color: hexRoot.theme.inputBg
        border.color: hexRoot.theme.borderSubtle
        border.width: 1
        clip: true

        ListView {
            id: hexListView
            anchors.fill: parent
            anchors.margins: 6
            spacing: 2
            boundsBehavior: Flickable.StopAtBounds
            ScrollBar.vertical: FluxScrollBar {}
            model: {
                if (!hexRoot.hasViewer || !hexRoot.diffViewer.hexRows) return [];
                if (!hexRoot.hexOnlyChanged) return hexRoot.diffViewer.hexRows;
                return hexRoot.diffViewer.hexRows.filter((row) => row.hasDiff);
            }

            delegate: Rectangle {
                width: hexListView.width - 8
                height: (hexRoot.hasViewer && hexRoot.diffViewer.hasOldBlob && hexRoot.diffViewer.hasNewBlob && modelData.hasDiff) ? 44 : 22
                radius: 3
                color: modelData.hasDiff ? hexRoot.theme.surface : "transparent"

                ColumnLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 8
                    anchors.rightMargin: 8
                    spacing: 1

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 12
                        visible: !(hexRoot.hasViewer && hexRoot.diffViewer.hasOldBlob && hexRoot.diffViewer.hasNewBlob && modelData.hasDiff)

                        Text {
                            text: modelData.offset
                            color: hexRoot.theme.textMuted
                            font.family: hexRoot.monoFont
                            font.pixelSize: 11
                        }
                        Text {
                            Layout.fillWidth: true
                            text: (hexRoot.hasViewer && hexRoot.diffViewer.hasNewBlob) ? modelData.newHex : modelData.oldHex
                            color: hexRoot.theme.textSecondary
                            font.family: hexRoot.monoFont
                            font.pixelSize: 11
                        }
                        Text {
                            Layout.preferredWidth: 140
                            text: (hexRoot.hasViewer && hexRoot.diffViewer.hasNewBlob) ? modelData.newAscii : modelData.oldAscii
                            color: hexRoot.theme.textMuted
                            font.family: hexRoot.monoFont
                            font.pixelSize: 11
                        }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 12
                        visible: hexRoot.hasViewer && hexRoot.diffViewer.hasOldBlob && hexRoot.diffViewer.hasNewBlob && modelData.hasDiff

                        Text { text: modelData.offset + " -"; color: hexRoot.theme.danger; font.family: hexRoot.monoFont; font.pixelSize: 11; font.bold: true }
                        Text {
                            Layout.fillWidth: true
                            textFormat: Text.RichText
                            text: modelData.oldRichHex
                            color: hexRoot.theme.textMuted
                            font.family: hexRoot.monoFont
                            font.pixelSize: 11
                        }
                        Text { Layout.preferredWidth: 140; text: modelData.oldAscii; color: hexRoot.theme.dangerText; font.family: hexRoot.monoFont; font.pixelSize: 11 }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 12
                        visible: hexRoot.hasViewer && hexRoot.diffViewer.hasOldBlob && hexRoot.diffViewer.hasNewBlob && modelData.hasDiff

                        Text { text: modelData.offset + " +"; color: hexRoot.theme.success; font.family: hexRoot.monoFont; font.pixelSize: 11; font.bold: true }
                        Text {
                            Layout.fillWidth: true
                            textFormat: Text.RichText
                            text: modelData.newRichHex
                            color: hexRoot.theme.textSecondary
                            font.family: hexRoot.monoFont
                            font.pixelSize: 11
                        }
                        Text { Layout.preferredWidth: 140; text: modelData.newAscii; color: hexRoot.theme.successText; font.family: hexRoot.monoFont; font.pixelSize: 11 }
                    }
                }
            }
        }
    }
}
