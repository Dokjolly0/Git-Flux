import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../themes"

Item {
    id: dialogsRoot
    readonly property FluxTheme fluxTheme: FluxTheme {}

    property var terminalVm: null
    property color defaultTabAccent: fluxTheme.defaultTabAccent
    property var defaultColorPalette: fluxTheme.accentPalette

    signal focusInputRequested()

    function openCloseRunning(index, title) {
        closeRunningTabDialog.pendingTabIndex = index;
        closeRunningTabDialog.pendingTabTitle = title || "Terminal";
        closeRunningTabDialog.open();
    }

    function openRename(index, title, color) {
        renameTabDialog.targetTabIndex = index;
        renameTabDialog.initialTitle = title || "Terminal";
        renameTabDialog.initialColor = color || "";
        renameTabDialog.open();
    }

    function openColorPicker(index, title, color) {
        tabColorDialog.targetTabIndex = index;
        tabColorDialog.targetTabTitle = title || "Terminal";
        tabColorDialog.selectedColor = color || "";
        tabColorDialog.open();
    }

    Dialog {
        id: closeRunningTabDialog
        objectName: "closeRunningTabDialog"
        property int pendingTabIndex: -1
        property string pendingTabTitle: ""

        anchors.centerIn: Overlay.overlay
        width: 430
        modal: true
        padding: 18
        header: null
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

        onOpened: cancelCloseTabBtn.forceActiveFocus()
        onClosed: {
            pendingTabIndex = -1;
            if (dialogsRoot.terminalVm && dialogsRoot.terminalVm.isVisible) {
                dialogsRoot.focusInputRequested();
            }
        }

        background: FluxModalBackground {}

        contentItem: ColumnLayout {
            spacing: 14

            RowLayout {
                Layout.fillWidth: true
                spacing: 12

                Rectangle {
                    width: 36
                    height: 36
                    radius: 10
                    color: dialogsRoot.fluxTheme.warningBg
                    border.color: dialogsRoot.fluxTheme.warning
                    border.width: 1

                    Text {
                        anchors.centerIn: parent
                        text: "⚠"
                        color: dialogsRoot.fluxTheme.warningText
                        font.pixelSize: 16
                    }
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 3

                    Text {
                        text: "Comando in esecuzione"
                        color: dialogsRoot.fluxTheme.textPrimary
                        font.pixelSize: 14
                        font.bold: true
                    }

                    Text {
                        text: "Nella scheda \"" + closeRunningTabDialog.pendingTabTitle + "\" c'è ancora un processo attivo.\nVuoi terminarlo e chiudere la scheda?"
                        color: dialogsRoot.fluxTheme.textSecondary
                        font.pixelSize: 12
                        wrapMode: Text.WordWrap
                        Layout.fillWidth: true
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 8

                Item { Layout.fillWidth: true }

                FluxActionButton {
                    id: cancelCloseTabBtn
                    objectName: "cancelCloseTabBtn"
                    text: "Annulla"
                    Layout.preferredHeight: 30
                    onClicked: closeRunningTabDialog.close()
                }

                FluxActionButton {
                    id: confirmCloseTabBtn
                    objectName: "confirmCloseTabBtn"
                    text: "Termina e chiudi"
                    Layout.preferredHeight: 30
                    baseColor: dialogsRoot.fluxTheme.dangerBg
                    hoverColor: dialogsRoot.fluxTheme.danger
                    baseBorderColor: dialogsRoot.fluxTheme.danger
                    textColor: dialogsRoot.fluxTheme.dangerText
                    hoverTextColor: dialogsRoot.fluxTheme.selectionText
                    fontBold: true
                    onClicked: {
                        const idx = closeRunningTabDialog.pendingTabIndex;
                        closeRunningTabDialog.close();
                        if (dialogsRoot.terminalVm && idx >= 0 && idx < dialogsRoot.terminalVm.tabs.length) {
                            dialogsRoot.terminalVm.closeTab(idx);
                            if (dialogsRoot.terminalVm.isVisible) {
                                dialogsRoot.focusInputRequested();
                            }
                        }
                    }
                }
            }
        }
    }

    Dialog {
        id: renameTabDialog
        objectName: "renameTabDialog"
        property int targetTabIndex: -1
        property string initialTitle: ""
        property string initialColor: ""

        anchors.centerIn: Overlay.overlay
        width: 410
        modal: true
        padding: 18
        header: null
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

        function submitRename() {
            const trimmed = renameTabInput.text.trim();
            if (trimmed.length === 0) return;
            const idx = targetTabIndex;
            const nextColor = renameColorSwatches.selectedColor;
            close();
            if (dialogsRoot.terminalVm && idx >= 0) {
                dialogsRoot.terminalVm.renameTab(idx, trimmed);
                dialogsRoot.terminalVm.setTabColor(idx, nextColor);
            }
        }

        onOpened: {
            renameTabInput.text = initialTitle;
            renameColorSwatches.selectedColor = initialColor;
            renameTabInput.forceActiveFocus();
            renameTabInput.selectAll();
        }
        onClosed: {
            targetTabIndex = -1;
            if (dialogsRoot.terminalVm && dialogsRoot.terminalVm.isVisible) {
                dialogsRoot.focusInputRequested();
            }
        }

        background: FluxModalBackground {}

        contentItem: ColumnLayout {
            spacing: 12

            Text {
                text: "✏ Rinomina e personalizza scheda"
                color: dialogsRoot.fluxTheme.textPrimary
                font.pixelSize: 13
                font.bold: true
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 34
                radius: 7
                color: dialogsRoot.fluxTheme.inputBg
                border.color: renameTabInput.activeFocus ? dialogsRoot.fluxTheme.borderFocus : dialogsRoot.fluxTheme.borderHighlight
                border.width: 1

                TextInput {
                    id: renameTabInput
                    objectName: "renameTabInput"
                    anchors.fill: parent
                    anchors.leftMargin: 10
                    anchors.rightMargin: 10
                    verticalAlignment: TextInput.AlignVCenter
                    color: dialogsRoot.fluxTheme.textPrimary
                    selectionColor: dialogsRoot.fluxTheme.selectionBg
                    selectedTextColor: dialogsRoot.fluxTheme.selectionText
                    font.pixelSize: 12
                    selectByMouse: true
                    clip: true

                    Keys.onReturnPressed: renameTabDialog.submitRename()
                    Keys.onEnterPressed: renameTabDialog.submitRename()
                    Keys.onEscapePressed: renameTabDialog.close()
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 6

                Text {
                    text: "Colore scheda"
                    color: dialogsRoot.fluxTheme.textSecondary
                    font.pixelSize: 11
                }

                FluxColorSwatchRow {
                    id: renameColorSwatches
                    showDefaultOption: true
                    swatchSize: 22
                    defaultAccentColor: dialogsRoot.defaultTabAccent
                    paletteColors: (dialogsRoot.terminalVm && dialogsRoot.terminalVm.tabColorPalette && dialogsRoot.terminalVm.tabColorPalette.length > 0)
                        ? dialogsRoot.terminalVm.tabColorPalette
                        : dialogsRoot.defaultColorPalette
                    objectNamePrefix: "renameTabSwatch_"
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 8

                Item { Layout.fillWidth: true }

                FluxActionButton {
                    id: cancelRenameTabBtn
                    objectName: "cancelRenameTabBtn"
                    text: "Annulla"
                    Layout.preferredHeight: 30
                    onClicked: renameTabDialog.close()
                }

                FluxActionButton {
                    id: confirmRenameTabBtn
                    objectName: "confirmRenameTabBtn"
                    text: "Salva"
                    enabled: renameTabInput.text.trim().length > 0
                    Layout.preferredHeight: 30
                    baseColor: dialogsRoot.fluxTheme.infoBg
                    hoverColor: dialogsRoot.fluxTheme.surfaceGlass
                    baseBorderColor: dialogsRoot.fluxTheme.borderFocus
                    textColor: dialogsRoot.fluxTheme.textPrimary
                    fontBold: true
                    onClicked: renameTabDialog.submitRename()
                }
            }
        }
    }

    Dialog {
        id: tabColorDialog
        objectName: "tabColorDialog"
        property int targetTabIndex: -1
        property string targetTabTitle: ""
        property string selectedColor: ""

        anchors.centerIn: Overlay.overlay
        width: 400
        modal: true
        padding: 18
        header: null
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

        onOpened: colorPickerSwatches.selectedColor = selectedColor
        onClosed: {
            targetTabIndex = -1;
            if (dialogsRoot.terminalVm && dialogsRoot.terminalVm.isVisible) {
                dialogsRoot.focusInputRequested();
            }
        }

        background: FluxModalBackground {}

        contentItem: ColumnLayout {
            spacing: 12

            Text {
                text: "🎨 Colore scheda: " + tabColorDialog.targetTabTitle
                color: dialogsRoot.fluxTheme.textPrimary
                font.pixelSize: 13
                font.bold: true
                elide: Text.ElideRight
                Layout.fillWidth: true
            }

            FluxColorSwatchRow {
                id: colorPickerSwatches
                showDefaultOption: true
                swatchSize: 22
                defaultAccentColor: dialogsRoot.defaultTabAccent
                paletteColors: (dialogsRoot.terminalVm && dialogsRoot.terminalVm.tabColorPalette && dialogsRoot.terminalVm.tabColorPalette.length > 0)
                    ? dialogsRoot.terminalVm.tabColorPalette
                    : dialogsRoot.defaultColorPalette
                objectNamePrefix: "quickTabSwatch_"
                onColorPicked: function (hexColor) {
                    const idx = tabColorDialog.targetTabIndex;
                    tabColorDialog.close();
                    if (dialogsRoot.terminalVm && idx >= 0) {
                        dialogsRoot.terminalVm.setTabColor(idx, hexColor);
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true
                Item { Layout.fillWidth: true }

                FluxActionButton {
                    text: "Chiudi"
                    Layout.preferredHeight: 28
                    onClicked: tabColorDialog.close()
                }
            }
        }
    }
}
