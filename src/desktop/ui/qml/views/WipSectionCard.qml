import QtQuick
import QtQuick.Layouts
import "../themes"

Rectangle {
    id: wipCard
    readonly property FluxTheme theme: FluxTheme {}

    property string titleText: ""
    property color accentColor: theme.warning
    property int fileCount: 0
    property string extraBadgeText: ""
    property bool isExpanded: true
    property bool showActionButton: false
    property string actionText: ""
    property color actionBgColor: theme.successBg
    property color actionHoverColor: theme.success
    property color actionBorderColor: theme.accentTeal
    property color actionTextColor: theme.selectionText
    property var pathFiles: []
    property var treeModel: []
    property string treeScope: ""
    property string emptyText: ""
    property string fileViewMode: "path"
    property var selectedFile: null
    property bool isWipSelected: true
    property bool isMultiSelectMode: false
    property var sessionVm: null

    signal toggleExpanded()
    signal actionClicked()
    signal openFileDiffRequested(var file)
    signal fileContextMenuRequested(string filePath)
    signal toggleFolderRequested(string fullPath)

    radius: 8
    color: wipCard.theme.inputBg
    border.color: fileCount > 0 ? Qt.alpha(accentColor, 0.38) : wipCard.theme.border
    border.width: 1
    clip: true

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // Section Header
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 34
            implicitHeight: 34
            color: sectionHeaderMouse.containsMouse
                ? Qt.alpha(wipCard.accentColor, 0.14)
                : Qt.alpha(wipCard.accentColor, 0.08)

            MouseArea {
                id: sectionHeaderMouse
                z: -1
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: wipCard.toggleExpanded()
            }

            RowLayout {
                z: 1
                anchors.fill: parent
                anchors.leftMargin: 10
                anchors.rightMargin: 8
                spacing: 7

                Text {
                    text: wipCard.isExpanded ? "▾" : "▸"
                    color: wipCard.theme.textMuted
                    font.pixelSize: 11
                    font.bold: true
                }

                Rectangle {
                    width: 8
                    height: 8
                    radius: 4
                    color: wipCard.accentColor
                }

                Text {
                    text: wipCard.titleText
                    color: wipCard.theme.textPrimary
                    font.pixelSize: 11
                    font.bold: true
                    elide: Text.ElideRight
                }

                Rectangle {
                    height: 18
                    implicitWidth: secCountText.implicitWidth + 10
                    radius: 9
                    color: Qt.alpha(wipCard.accentColor, 0.18)
                    border.color: Qt.alpha(wipCard.accentColor, 0.50)
                    border.width: 1

                    Text {
                        id: secCountText
                        anchors.centerIn: parent
                        text: String(wipCard.fileCount)
                        color: wipCard.theme.textPrimary
                        font.pixelSize: 10
                        font.bold: true
                    }
                }

                Rectangle {
                    visible: wipCard.extraBadgeText.length > 0
                    height: 18
                    implicitWidth: extraBadgeLabel.implicitWidth + 10
                    radius: 9
                    color: Qt.alpha(wipCard.theme.accentTeal, 0.14)
                    border.color: Qt.alpha(wipCard.theme.accentTeal, 0.40)
                    border.width: 1

                    Text {
                        id: extraBadgeLabel
                        anchors.centerIn: parent
                        text: wipCard.extraBadgeText
                        color: wipCard.theme.accentTeal
                        font.pixelSize: 9
                        font.bold: true
                    }
                }

                Item {
                    Layout.fillWidth: true
                }

                Rectangle {
                    z: 2
                    visible: wipCard.showActionButton
                    Layout.preferredHeight: 22
                    implicitWidth: secActionText.implicitWidth + 12
                    radius: 4
                    color: secActionMouse.containsMouse ? wipCard.actionHoverColor : wipCard.actionBgColor
                    border.color: wipCard.actionBorderColor
                    border.width: 1

                    Text {
                        id: secActionText
                        anchors.centerIn: parent
                        text: wipCard.actionText
                        color: wipCard.actionTextColor
                        font.pixelSize: 10
                        font.bold: true
                    }

                    MouseArea {
                        id: secActionMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        preventStealing: true
                        onClicked: function (mouse) {
                            mouse.accepted = true;
                            wipCard.actionClicked();
                        }
                    }
                }
            }
        }

        Rectangle {
            visible: wipCard.isExpanded
            Layout.fillWidth: true
            Layout.preferredHeight: 1
            color: wipCard.theme.borderSubtle
        }

        CommitFilesPane {
            visible: wipCard.isExpanded
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.margins: 4
            pathFiles: wipCard.pathFiles
            treeModel: wipCard.treeModel
            treeScope: wipCard.treeScope
            emptyText: wipCard.emptyText
            fileViewMode: wipCard.fileViewMode
            selectedFile: wipCard.selectedFile
            isWipSelected: wipCard.isWipSelected
            isMultiSelectMode: wipCard.isMultiSelectMode
            sessionVm: wipCard.sessionVm
            onOpenFileDiffRequested: function (file) { wipCard.openFileDiffRequested(file); }
            onFileContextMenuRequested: function (path) { wipCard.fileContextMenuRequested(path); }
            onToggleFolderRequested: function (fullPath) { wipCard.toggleFolderRequested(fullPath); }
        }
    }
}
