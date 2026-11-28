import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../themes"

Rectangle {
    id: popupRoot
    FluxTheme { id: fluxTheme }

    property var completionModel: null
    property int selectedIndex: 0
    property bool userNavigated: false
    property bool enterWillComplete: false
    property int totalCount: completionModel ? completionModel.count : 0
    property int _modelRevision: 0
    readonly property bool selectedIsExecuteAction: Boolean(
        _modelRevision >= 0 &&
        completionModel && totalCount > 0 && selectedIndex >= 0 && selectedIndex < totalCount &&
        completionModel.isExecuteAction(selectedIndex)
    )

    Connections {
        target: popupRoot.completionModel
        function onModelReset() {
            ++popupRoot._modelRevision;
        }
    }

    signal itemAccepted(int index)

    readonly property int rowHeight: 30
    readonly property int maxVisibleRows: 9
    readonly property int footerHeight: 26

    visible: totalCount > 0
    width: 560
    height: Math.min(maxVisibleRows, Math.max(1, totalCount)) * rowHeight + footerHeight + 10
    radius: fluxTheme.radiusMd
    color: fluxTheme.surfaceElevated
    border.color: fluxTheme.borderHighlight
    border.width: 1
    z: 300

    function moveSelection(delta) {
        if (totalCount <= 0) return;
        let next = selectedIndex + delta;
        if (next < 0) next = totalCount - 1;
        else if (next >= totalCount) next = 0;
        selectedIndex = next;
        userNavigated = true;
        completionList.positionViewAtIndex(selectedIndex, ListView.Contain);
    }

    function resetSelection() {
        selectedIndex = 0;
        userNavigated = false;
        if (completionList.count > 0) {
            completionList.positionViewAtBeginning();
        }
    }

    onTotalCountChanged: {
        if (selectedIndex >= totalCount || selectedIndex < 0) {
            selectedIndex = 0;
        }
        if (totalCount > 0) {
            completionList.positionViewAtIndex(selectedIndex, ListView.Contain);
        }
    }

    function escapeHtml(str) {
        if (!str) return "";
        return str.replace(/&/g, "&amp;").replace(/</g, "&lt;").replace(/>/g, "&gt;");
    }

    function formatHighlightedText(displayText, matchStart, matchLength) {
        if (!displayText) return "";
        if (matchLength <= 0 || matchStart < 0 || matchStart >= displayText.length) {
            return escapeHtml(displayText);
        }
        const end = Math.min(displayText.length, matchStart + matchLength);
        const before = escapeHtml(displayText.slice(0, matchStart));
        const matched = escapeHtml(displayText.slice(matchStart, end));
        const after = escapeHtml(displayText.slice(end));
        return before + "<span style=\"color:" + fluxTheme.accentCyan + ";font-weight:700;\">" + matched + "</span>" + after;
    }

    function badgeFg(categoryLabel, kind) {
        if (categoryLabel === "porcelain" || categoryLabel === "git" || categoryLabel === "remote" || kind === "remote" || categoryLabel === "ps1") {
            return fluxTheme.info;
        }
        if (categoryLabel === "alias" || kind === "alias") return fluxTheme.accentPurple;
        if (categoryLabel === "ancillary" || categoryLabel === "interop") return fluxTheme.infoText;
        if (categoryLabel === "external" || categoryLabel === "bat" || categoryLabel === "py" || categoryLabel === "tag" || kind === "tag") {
            return fluxTheme.warningText;
        }
        if (categoryLabel === "run" || kind === "execute" || categoryLabel === "flag" || categoryLabel === "param" || kind === "flag" || categoryLabel === "sh" || categoryLabel === "branch" || categoryLabel === "ref" || kind === "branch") {
            return fluxTheme.successText;
        }
        if (categoryLabel === "value" || categoryLabel === "stash" || kind === "stash") return fluxTheme.dangerText;
        if (categoryLabel === "file" || categoryLabel === "dir" || categoryLabel === "path") return fluxTheme.accentTeal;
        if (categoryLabel === "commit") return fluxTheme.warning;
        return fluxTheme.textSecondary;
    }

    function badgeBg(categoryLabel, kind) {
        return Qt.alpha(badgeFg(categoryLabel, kind), 0.14);
    }

    // Subtle outer glow border for depth
    Rectangle {
        anchors.fill: parent
        anchors.margins: -1
        radius: 11
        color: "transparent"
        border.color: "#44000000"
        border.width: 2
        z: -1
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.topMargin: 5
        anchors.bottomMargin: 0
        spacing: 0

        ListView {
            id: completionList
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.leftMargin: 5
            Layout.rightMargin: 5
            clip: true
            model: popupRoot.completionModel
            currentIndex: popupRoot.selectedIndex
            boundsBehavior: Flickable.StopAtBounds
            highlightMoveDuration: 60

            SmoothWheelScroller {}

            ScrollBar.vertical: FluxScrollBar {
                policy: ScrollBar.AsNeeded
            }

            delegate: Rectangle {
                id: rowItem
                required property int index
                required property string displayText
                required property string detail
                required property string categoryLabel
                required property string kind
                required property int matchStart
                required property int matchLength

                readonly property bool isSelected: index === popupRoot.selectedIndex

                width: completionList.width
                height: popupRoot.rowHeight
                radius: 6
                color: isSelected ? fluxTheme.cardHover : (rowMa.containsMouse ? fluxTheme.card : "transparent")
                border.color: isSelected ? fluxTheme.borderFocus : "transparent"
                border.width: 1

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 9
                    anchors.rightMargin: 9
                    spacing: 8

                    // Command / flag / ref name with highlighted matching prefix
                    Text {
                        text: popupRoot.formatHighlightedText(rowItem.displayText, rowItem.matchStart, rowItem.matchLength)
                        textFormat: Text.RichText
                        color: rowItem.isSelected ? fluxTheme.textPrimary : fluxTheme.textSecondary
                        font.family: (typeof gfMonoFont !== "undefined") ? gfMonoFont : "monospace"
                        font.pixelSize: 12
                        font.bold: rowItem.isSelected
                        Layout.maximumWidth: 220
                        elide: Text.ElideRight
                    }

                    // Category badge
                    Rectangle {
                        visible: rowItem.categoryLabel.length > 0
                        Layout.preferredHeight: 17
                        implicitWidth: badgeText.implicitWidth + 10
                        radius: 4
                        color: popupRoot.badgeBg(rowItem.categoryLabel, rowItem.kind)
                        border.color: Qt.alpha(popupRoot.badgeFg(rowItem.categoryLabel, rowItem.kind), 0.35)
                        border.width: 1

                        Text {
                            id: badgeText
                            anchors.centerIn: parent
                            text: rowItem.categoryLabel
                            color: popupRoot.badgeFg(rowItem.categoryLabel, rowItem.kind)
                            font.pixelSize: 9
                            font.bold: true
                        }
                    }

                    // Description
                    Text {
                        text: rowItem.detail
                        color: rowItem.isSelected ? fluxTheme.textPrimary : fluxTheme.textMuted
                        font.pixelSize: 11
                        Layout.fillWidth: true
                        elide: Text.ElideRight
                        horizontalAlignment: Text.AlignRight
                    }
                }

                MouseArea {
                    id: rowMa
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onEntered: {
                        popupRoot.selectedIndex = rowItem.index;
                    }
                    onClicked: {
                        popupRoot.itemAccepted(rowItem.index);
                    }
                }
            }
        }

        // Bottom hint & match counter footer
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: popupRoot.footerHeight
            Layout.topMargin: 4
            color: fluxTheme.footerBg
            radius: 9

            // Flatten top corners so footer connects seamlessly to popup body
            Rectangle {
                anchors.top: parent.top
                anchors.left: parent.left
                anchors.right: parent.right
                height: 8
                color: fluxTheme.footerBg
            }

            Rectangle {
                anchors.top: parent.top
                anchors.left: parent.left
                anchors.right: parent.right
                height: 1
                color: fluxTheme.borderSubtle
            }

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 10
                anchors.rightMargin: 10
                spacing: 8

                Text {
                    objectName: "completionFooterHint"
                    text: popupRoot.selectedIsExecuteAction
                        ? (popupRoot.totalCount > 1
                            ? "Invio esegui  •  ↓ / Tab scegli parametro  •  Esc chiudi"
                            : "Invio / Tab esegui  •  Esc chiudi")
                        : (popupRoot.enterWillComplete
                            ? "Tab / Invio completa  •  ↑↓ naviga  •  Shift+Invio esegui  •  Esc chiudi"
                            : "Tab completa  •  ↑↓ naviga  •  Invio esegui  •  Esc chiudi")
                    color: fluxTheme.textMuted
                    font.pixelSize: 10
                    Layout.fillWidth: true
                    elide: Text.ElideRight
                }

                Text {
                    text: popupRoot.totalCount + (popupRoot.totalCount === 1 ? " risultato" : " risultati")
                    color: fluxTheme.info
                    font.pixelSize: 10
                    font.bold: true
                }
            }
        }
    }
}
