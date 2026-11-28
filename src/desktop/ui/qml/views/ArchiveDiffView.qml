import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../themes"

ColumnLayout {
    id: archiveRoot
    readonly property FluxTheme theme: FluxTheme {}

    property var diffViewer: null
    property string monoFont: "monospace"
    property string archiveFilterText: ""
    property bool archiveOnlyChanged: false

    readonly property bool hasViewer: diffViewer !== null

    spacing: 10

    // Summary + search bar
    RowLayout {
        Layout.fillWidth: true
        spacing: 8

        Rectangle {
            visible: archiveRoot.hasViewer && archiveRoot.diffViewer.archiveAddedCount > 0
            implicitHeight: 24; implicitWidth: arAddTxt.implicitWidth + 12; radius: 5; color: archiveRoot.theme.successBg; border.color: archiveRoot.theme.success
            Text { id: arAddTxt; anchors.centerIn: parent; text: "+" + (archiveRoot.hasViewer ? archiveRoot.diffViewer.archiveAddedCount : 0) + " aggiunti"; color: archiveRoot.theme.successText; font.pixelSize: 11; font.bold: true }
        }
        Rectangle {
            visible: archiveRoot.hasViewer && archiveRoot.diffViewer.archiveModifiedCount > 0
            implicitHeight: 24; implicitWidth: arModTxt.implicitWidth + 12; radius: 5; color: archiveRoot.theme.warningBg; border.color: archiveRoot.theme.warning
            Text { id: arModTxt; anchors.centerIn: parent; text: "~" + (archiveRoot.hasViewer ? archiveRoot.diffViewer.archiveModifiedCount : 0) + " modificati"; color: archiveRoot.theme.warningText; font.pixelSize: 11; font.bold: true }
        }
        Rectangle {
            visible: archiveRoot.hasViewer && archiveRoot.diffViewer.archiveRemovedCount > 0
            implicitHeight: 24; implicitWidth: arRemTxt.implicitWidth + 12; radius: 5; color: archiveRoot.theme.dangerBg; border.color: archiveRoot.theme.danger
            Text { id: arRemTxt; anchors.centerIn: parent; text: "-" + (archiveRoot.hasViewer ? archiveRoot.diffViewer.archiveRemovedCount : 0) + " rimossi"; color: archiveRoot.theme.dangerText; font.pixelSize: 11; font.bold: true }
        }
        Rectangle {
            visible: archiveRoot.hasViewer && archiveRoot.diffViewer.archiveUnchangedCount > 0
            implicitHeight: 24; implicitWidth: arUnchTxt.implicitWidth + 12; radius: 5; color: archiveRoot.theme.surfaceElevated; border.color: archiveRoot.theme.borderHighlight
            Text { id: arUnchTxt; anchors.centerIn: parent; text: (archiveRoot.hasViewer ? archiveRoot.diffViewer.archiveUnchangedCount : 0) + " invariati"; color: archiveRoot.theme.textMuted; font.pixelSize: 11 }
        }

        Item { Layout.fillWidth: true }

        RichPreviewPillButton {
            text: "Solo modifiche"
            active: archiveRoot.archiveOnlyChanged
            onClicked: archiveRoot.archiveOnlyChanged = !archiveRoot.archiveOnlyChanged
        }

        FluxTextField {
            Layout.preferredWidth: 220
            Layout.preferredHeight: 26
            placeholderText: "Filtra file nell'archivio..."
            font.pixelSize: 11
            text: archiveRoot.archiveFilterText
            onTextChanged: archiveRoot.archiveFilterText = text
        }
    }

    Rectangle {
        Layout.fillWidth: true
        Layout.fillHeight: true
        radius: 8
        color: archiveRoot.theme.inputBg
        border.color: archiveRoot.theme.borderSubtle
        border.width: 1
        clip: true

        ListView {
            id: archiveListView
            anchors.fill: parent
            anchors.margins: 4
            spacing: 2
            boundsBehavior: Flickable.StopAtBounds
            ScrollBar.vertical: FluxScrollBar {}
            model: {
                if (!archiveRoot.hasViewer || !archiveRoot.diffViewer.archiveEntries) return [];
                const q = archiveRoot.archiveFilterText.trim().toLowerCase();
                return archiveRoot.diffViewer.archiveEntries.filter((e) => {
                    if (archiveRoot.archiveOnlyChanged && e.status === "unchanged") return false;
                    return q.length === 0 || e.path.toLowerCase().indexOf(q) >= 0;
                });
            }

            delegate: Rectangle {
                width: archiveListView.width - 8
                height: 28
                radius: 4
                color: modelData.status === "added" ? archiveRoot.theme.successBg
                     : (modelData.status === "removed" ? archiveRoot.theme.dangerBg
                        : (modelData.status === "modified" ? archiveRoot.theme.warningBg : "transparent"))

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 10
                    anchors.rightMargin: 10
                    spacing: 8

                    Text {
                        Layout.preferredWidth: 16
                        horizontalAlignment: Text.AlignHCenter
                        text: modelData.status === "added" ? "+"
                            : (modelData.status === "removed" ? "−"
                               : (modelData.status === "modified" ? "~" : "•"))
                        color: modelData.status === "added" ? archiveRoot.theme.success
                             : (modelData.status === "removed" ? archiveRoot.theme.danger
                                : (modelData.status === "modified" ? archiveRoot.theme.warning : archiveRoot.theme.textMuted))
                        font.family: archiveRoot.monoFont
                        font.bold: true
                        font.pixelSize: 12
                    }

                    Text {
                        Layout.fillWidth: true
                        text: modelData.path
                        color: modelData.status === "removed" ? archiveRoot.theme.dangerText : archiveRoot.theme.textSecondary
                        font.family: archiveRoot.monoFont
                        font.pixelSize: 11
                        elide: Text.ElideMiddle
                    }

                    Text {
                        text: (modelData.oldSizeText && modelData.newSizeText && modelData.oldSizeText !== modelData.newSizeText)
                            ? (modelData.oldSizeText + " → " + modelData.newSizeText)
                            : (modelData.newSizeText || modelData.oldSizeText || "")
                        color: archiveRoot.theme.textMuted
                        font.family: archiveRoot.monoFont
                        font.pixelSize: 11
                    }
                }
            }
        }
    }
}
