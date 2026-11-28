import QtQuick
import "../themes"

TextEdit {
    id: codeEdit
    readonly property FluxTheme theme: FluxTheme {}

    property var panel: null
    property string selTargetName: ""
    property int rowIndex: -1
    property int hunkIdx: -1
    property bool rowDisabled: false
    property bool _syncingSelection: false

    readOnly: true
    selectByMouse: false
    activeFocusOnPress: false
    persistentSelection: true
    textFormat: TextEdit.RichText
    selectionColor: theme.selectionBg
    selectedTextColor: theme.selectionText
    leftPadding: 0
    rightPadding: 0
    topPadding: 0
    bottomPadding: 0
    font.family: panel ? panel.monoFont : "Consolas"

    function syncSelection() {
        if (!panel || _syncingSelection) return;
        if (!panel.hasSelection && codeEdit.selectionStart === codeEdit.selectionEnd) return;
        _syncingSelection = true;
        try {
            panel.syncCodeSelection(codeEdit, selTargetName, rowIndex, rowDisabled, hunkIdx);
        } finally {
            _syncingSelection = false;
        }
    }

    Component.onCompleted: { syncSelection(); }
    onRowIndexChanged: { syncSelection(); }
    onTextChanged: { syncSelection(); }

    Rectangle {
        visible: Boolean(
            codeEdit.panel
            && !codeEdit.rowDisabled
            && codeEdit.panel.hasSelection
            && codeEdit.panel.selTarget === codeEdit.selTargetName
            && (codeEdit.hunkIdx < 0 || codeEdit.panel.selHunkIndex === codeEdit.hunkIdx)
            && codeEdit.rowIndex >= codeEdit.panel.selStartRow
            && codeEdit.rowIndex < codeEdit.panel.selEndRow
            && codeEdit.length === 0
        )
        x: 0
        y: 0
        width: 6
        height: Math.max(14, codeEdit.implicitHeight)
        color: codeEdit.theme.selectionBg
    }

    Connections {
        target: codeEdit.panel
        function onSelectionStateChanged() { codeEdit.syncSelection(); }
    }
}
