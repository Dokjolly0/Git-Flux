import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../themes"

// Step-by-step "how to fix it" card for a remote access problem (SSH key, host key, token...).
// `guide` is GitProfilesViewModel.troubleshootingGuide(); `logText` is the raw ssh/git output.
ColumnLayout {
    id: guideRoot
    readonly property FluxTheme theme: FluxTheme {}

    property var guide: ({})
    property string host: ""
    property string logText: ""
    property bool showSummary: true
    property string feedback: ""
    property bool showLog: false
    property var profilesVm: null
    property var actionHandler: null // function(action, host) -> feedback text

    spacing: 8

    component SelectableText: TextEdit {
        readOnly: true
        selectByMouse: true
        selectByKeyboard: true
        persistentSelection: false
        wrapMode: TextEdit.Wrap
        textFormat: TextEdit.PlainText
        selectionColor: guideRoot.theme.selectionBg
        selectedTextColor: guideRoot.theme.selectionText
        color: guideRoot.theme.textSecondary
        font.pixelSize: 11
    }

    component GuideButton: Button {
        id: guideBtn
        property color accent: guideRoot.theme.accentCyan
        Layout.preferredHeight: 24
        leftPadding: 10
        rightPadding: 10
        hoverEnabled: true
        contentItem: Text {
            text: guideBtn.text
            color: guideBtn.hovered ? guideRoot.theme.textPrimary : guideBtn.accent
            font.pixelSize: 11
            font.bold: true
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
        }
        background: Rectangle {
            radius: 4
            color: guideBtn.hovered ? guideRoot.theme.infoBg : guideRoot.theme.inputBg
            border.color: guideBtn.accent
            border.width: 1
        }
    }

    Timer {
        id: feedbackTimer
        interval: 2500
        onTriggered: guideRoot.feedback = ""
    }

    function act(action) {
        guideRoot.feedback = guideRoot.actionHandler ? guideRoot.actionHandler(action, guideRoot.host) : "";
        if (guideRoot.feedback.length > 0) feedbackTimer.restart();
    }

    Text {
        visible: guideRoot.showSummary && (guideRoot.guide.summary || "").length > 0
        text: guideRoot.guide.summary || ""
        color: guideRoot.theme.textSecondary
        font.pixelSize: 11
        wrapMode: Text.Wrap
        Layout.fillWidth: true
    }

    Repeater {
        model: guideRoot.guide.steps || []

        delegate: RowLayout {
            required property var modelData
            required property int index
            Layout.fillWidth: true
            spacing: 8

            Rectangle {
                Layout.alignment: Qt.AlignTop
                Layout.preferredWidth: 20
                Layout.preferredHeight: 20
                radius: 10
                color: guideRoot.theme.infoBg
                Text {
                    anchors.centerIn: parent
                    text: index + 1
                    color: guideRoot.theme.infoText
                    font.pixelSize: 10
                    font.bold: true
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 4

                Text {
                    text: modelData.title
                    color: guideRoot.theme.textPrimary
                    font.pixelSize: 12
                    font.bold: true
                    wrapMode: Text.Wrap
                    Layout.fillWidth: true
                }

                SelectableText {
                    visible: (modelData.detail || "").length > 0
                    text: modelData.detail || ""
                    color: guideRoot.theme.textMuted
                    Layout.fillWidth: true
                }

                // Copyable command / snippet
                Rectangle {
                    visible: (modelData.command || "").length > 0
                    Layout.fillWidth: true
                    implicitHeight: cmdRow.implicitHeight + 10
                    radius: 4
                    color: guideRoot.theme.canvas
                    border.color: guideRoot.theme.borderSubtle

                    RowLayout {
                        id: cmdRow
                        anchors.fill: parent
                        anchors.margins: 5
                        spacing: 6

                        SelectableText {
                            text: modelData.command || ""
                            font.family: "monospace"
                            color: guideRoot.theme.infoText
                            Layout.fillWidth: true
                        }

                        GuideButton {
                            text: "Copia"
                            Layout.alignment: Qt.AlignTop
                            onClicked: {
                                if (guideRoot.profilesVm) guideRoot.profilesVm.copyToClipboard(modelData.command);
                                guideRoot.feedback = "Comando copiato negli appunti";
                                feedbackTimer.restart();
                            }
                        }
                    }
                }

                RowLayout {
                    visible: (modelData.url || "").length > 0 || (modelData.action || "").length > 0
                    spacing: 6

                    GuideButton {
                        visible: (modelData.action || "").length > 0
                        text: modelData.actionLabel || ""
                        accent: guideRoot.theme.accentTeal
                        onClicked: guideRoot.act(modelData.action)
                    }

                    GuideButton {
                        visible: (modelData.url || "").length > 0
                        text: (modelData.urlLabel || "Apri") + " ↗"
                        onClicked: Qt.openUrlExternally(modelData.url)
                    }
                }
            }
        }
    }

    // Raw output, selectable and copyable
    RowLayout {
        visible: guideRoot.logText.length > 0
        spacing: 6

        GuideButton {
            text: guideRoot.showLog ? "Nascondi log" : "Mostra log"
            accent: guideRoot.theme.textMuted
            onClicked: guideRoot.showLog = !guideRoot.showLog
        }

        GuideButton {
            text: "Copia log"
            accent: guideRoot.theme.textMuted
            onClicked: {
                if (guideRoot.profilesVm) guideRoot.profilesVm.copyToClipboard(guideRoot.logText);
                guideRoot.feedback = "Log copiato negli appunti";
                feedbackTimer.restart();
            }
        }

        Text {
            visible: guideRoot.feedback.length > 0
            text: "✔ " + guideRoot.feedback
            color: guideRoot.theme.successText
            font.pixelSize: 11
        }
    }

    Text {
        visible: guideRoot.logText.length === 0 && guideRoot.feedback.length > 0
        text: "✔ " + guideRoot.feedback
        color: guideRoot.theme.successText
        font.pixelSize: 11
    }

    Rectangle {
        visible: guideRoot.showLog && guideRoot.logText.length > 0
        Layout.fillWidth: true
        Layout.preferredHeight: Math.min(logEdit.implicitHeight + 12, 140)
        radius: 4
        color: guideRoot.theme.canvas
        border.color: guideRoot.theme.borderSubtle

        Flickable {
            id: logFlick
            anchors.fill: parent
            anchors.margins: 6
            clip: true
            contentWidth: width
            contentHeight: logEdit.implicitHeight
            boundsBehavior: Flickable.StopAtBounds
            ScrollBar.vertical: FluxScrollBar {}

            SelectableText {
                id: logEdit
                width: logFlick.width
                text: guideRoot.logText
                font.family: "monospace"
                color: guideRoot.theme.dangerText
            }
        }
    }
}
