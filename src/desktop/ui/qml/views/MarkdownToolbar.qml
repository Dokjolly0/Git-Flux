import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../themes"

Rectangle {
    id: toolbarRoot
    readonly property FluxTheme theme: FluxTheme {}

    property var diffViewer: null
    property bool showToc: false
    property bool fullWidth: false
    property real zoomScale: 1.0
    property bool copiedWholeFile: false
    property int changedBlocksCount: 0

    signal toggleTocRequested()
    signal jumpToNextChangeRequested()
    signal toggleFullWidthRequested()
    signal zoomDeltaRequested(real delta)
    signal copyWholeRequested()

    implicitHeight: 36
    color: theme.surface
    border.color: theme.border
    border.width: 1

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 12
        anchors.rightMargin: 12
        spacing: 8

        // TOC Drawer Toggle
        Button {
            id: tocBtn
            Layout.preferredHeight: 26
            leftPadding: 9
            rightPadding: 9
            hoverEnabled: true
            enabled: Boolean(toolbarRoot.diffViewer && toolbarRoot.diffViewer.markdownToc && toolbarRoot.diffViewer.markdownToc.length > 0)
            opacity: enabled ? 1.0 : 0.45
            background: Rectangle {
                color: toolbarRoot.showToc ? toolbarRoot.theme.infoBg : (tocBtn.hovered ? toolbarRoot.theme.cardHover : toolbarRoot.theme.surface)
                radius: 7
                border.color: toolbarRoot.showToc ? toolbarRoot.theme.accentCyan : toolbarRoot.theme.border
                border.width: 1
                Behavior on color {
                    ColorAnimation {
                        duration: toolbarRoot.theme.animFast
                    }
                }
            }
            contentItem: RowLayout {
                spacing: 5
                Text {
                    text: "📑"
                    font.pixelSize: 10
                }
                Text {
                    text: {
                        const tocCount = (toolbarRoot.diffViewer && toolbarRoot.diffViewer.markdownToc) ? toolbarRoot.diffViewer.markdownToc.length : 0;
                        return "Indice" + (tocCount > 0 ? (" (" + tocCount + ")") : "");
                    }
                    color: toolbarRoot.showToc ? toolbarRoot.theme.accentCyan : toolbarRoot.theme.textSecondary
                    font.pixelSize: 11
                    font.bold: toolbarRoot.showToc
                }
            }
            onClicked: toolbarRoot.toggleTocRequested()
        }

        // Rich Diff Toggle & Jump to Next Change
        Button {
            id: richDiffBtn
            Layout.preferredHeight: 26
            leftPadding: 9
            rightPadding: 9
            hoverEnabled: true
            readonly property bool isActive: toolbarRoot.diffViewer ? toolbarRoot.diffViewer.showMarkdownRichDiff : true
            background: Rectangle {
                color: richDiffBtn.isActive ? toolbarRoot.theme.successBg : (richDiffBtn.hovered ? toolbarRoot.theme.cardHover : toolbarRoot.theme.surface)
                radius: 7
                border.color: richDiffBtn.isActive ? toolbarRoot.theme.accentTeal : toolbarRoot.theme.border
                border.width: 1
                Behavior on color {
                    ColorAnimation {
                        duration: toolbarRoot.theme.animFast
                    }
                }
            }
            contentItem: RowLayout {
                spacing: 5
                Rectangle {
                    Layout.preferredWidth: 7
                    Layout.preferredHeight: 7
                    radius: 3.5
                    color: richDiffBtn.isActive ? toolbarRoot.theme.accentTeal : toolbarRoot.theme.textMuted
                }
                Text {
                    text: "Rich Diff" + (toolbarRoot.changedBlocksCount > 0 ? (" (" + toolbarRoot.changedBlocksCount + ")") : "")
                    color: richDiffBtn.isActive ? toolbarRoot.theme.successText : toolbarRoot.theme.textMuted
                    font.pixelSize: 11
                    font.bold: richDiffBtn.isActive
                }
            }
            onClicked: {
                if (toolbarRoot.diffViewer) {
                    toolbarRoot.diffViewer.setShowMarkdownRichDiff(!toolbarRoot.diffViewer.showMarkdownRichDiff);
                }
            }
        }

        Button {
            id: nextChangeBtn
            visible: toolbarRoot.changedBlocksCount > 0
            Layout.preferredHeight: 26
            leftPadding: 9
            rightPadding: 9
            hoverEnabled: true
            background: Rectangle {
                color: nextChangeBtn.hovered ? toolbarRoot.theme.cardHover : toolbarRoot.theme.surface
                radius: 7
                border.color: toolbarRoot.theme.border
                border.width: 1
                Behavior on color {
                    ColorAnimation {
                        duration: toolbarRoot.theme.animFast
                    }
                }
            }
            contentItem: Text {
                text: "↓ Prossima modifica"
                color: toolbarRoot.theme.accentCyan
                font.pixelSize: 11
                font.bold: true
            }
            onClicked: toolbarRoot.jumpToNextChangeRequested()
        }

        Item {
            Layout.fillWidth: true
        }

        // Width Toggle (GitHub Centered vs Full Width)
        Button {
            id: widthToggleBtn
            Layout.preferredHeight: 26
            leftPadding: 9
            rightPadding: 9
            hoverEnabled: true
            background: Rectangle {
                color: widthToggleBtn.hovered ? toolbarRoot.theme.cardHover : toolbarRoot.theme.surface
                radius: 7
                border.color: toolbarRoot.theme.border
                border.width: 1
                Behavior on color {
                    ColorAnimation {
                        duration: toolbarRoot.theme.animFast
                    }
                }
            }
            contentItem: Text {
                text: toolbarRoot.fullWidth ? "↔ Larghezza lettore" : "↔ Espandi pagina"
                color: toolbarRoot.theme.textSecondary
                font.pixelSize: 11
            }
            onClicked: toolbarRoot.toggleFullWidthRequested()
        }

        // Zoom Controls
        Rectangle {
            Layout.preferredHeight: 26
            implicitWidth: zoomRow.implicitWidth + 8
            color: toolbarRoot.theme.inputBg
            radius: 7
            border.color: toolbarRoot.theme.border
            border.width: 1

            RowLayout {
                id: zoomRow
                anchors.centerIn: parent
                spacing: 2

                Button {
                    id: zoomOutBtn
                    Layout.preferredWidth: 22
                    Layout.preferredHeight: 20
                    background: Rectangle {
                        color: zoomOutBtn.hovered ? toolbarRoot.theme.cardHover : "transparent"
                        radius: 3
                    }
                    contentItem: Text {
                        text: "A-"
                        color: toolbarRoot.theme.textSecondary
                        font.pixelSize: 10
                        font.bold: true
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                    onClicked: toolbarRoot.zoomDeltaRequested(-0.1)
                }

                Text {
                    text: Math.round(toolbarRoot.zoomScale * 100) + "%"
                    color: toolbarRoot.theme.textMuted
                    font.pixelSize: 10
                    Layout.preferredWidth: 34
                    horizontalAlignment: Text.AlignHCenter
                }

                Button {
                    id: zoomInBtn
                    Layout.preferredWidth: 22
                    Layout.preferredHeight: 20
                    background: Rectangle {
                        color: zoomInBtn.hovered ? toolbarRoot.theme.cardHover : "transparent"
                        radius: 3
                    }
                    contentItem: Text {
                        text: "A+"
                        color: toolbarRoot.theme.textSecondary
                        font.pixelSize: 10
                        font.bold: true
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                    onClicked: toolbarRoot.zoomDeltaRequested(0.1)
                }
            }
        }

        // Copy Full Markdown Source
        Button {
            id: copyMdBtn
            Layout.preferredHeight: 24
            enabled: Boolean(toolbarRoot.diffViewer && toolbarRoot.diffViewer.markdownSource && toolbarRoot.diffViewer.markdownSource.length > 0)
            background: Rectangle {
                color: toolbarRoot.copiedWholeFile ? toolbarRoot.theme.successBg : (copyMdBtn.hovered ? toolbarRoot.theme.cardHover : toolbarRoot.theme.surfaceElevated)
                radius: 4
                border.color: toolbarRoot.copiedWholeFile ? toolbarRoot.theme.accentTeal : toolbarRoot.theme.borderHighlight
                border.width: 1
            }
            contentItem: Text {
                text: toolbarRoot.copiedWholeFile ? "✓ Copiato!" : "📋 Copia .md"
                color: toolbarRoot.copiedWholeFile ? toolbarRoot.theme.successText : toolbarRoot.theme.textSecondary
                font.pixelSize: 11
                font.bold: toolbarRoot.copiedWholeFile
            }
            onClicked: toolbarRoot.copyWholeRequested()
        }
    }
}
