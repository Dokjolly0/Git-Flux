import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: popupRoot

    property var completionModel: null
    property int selectedIndex: 0
    property int totalCount: completionModel ? completionModel.count : 0

    signal itemAccepted(int index)

    readonly property int rowHeight: 30
    readonly property int maxVisibleRows: 9
    readonly property int footerHeight: 26

    visible: totalCount > 0
    width: 560
    height: Math.min(maxVisibleRows, Math.max(1, totalCount)) * rowHeight + footerHeight + 10
    radius: 10
    color: "#0D1420"
    border.color: "#2A3D58"
    border.width: 1
    z: 300

    function moveSelection(delta) {
        if (totalCount <= 0) return;
        let next = selectedIndex + delta;
        if (next < 0) next = totalCount - 1;
        else if (next >= totalCount) next = 0;
        selectedIndex = next;
        completionList.positionViewAtIndex(selectedIndex, ListView.Contain);
    }

    function resetSelection() {
        selectedIndex = 0;
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
        return before + "<span style=\"color:#00E5FF;font-weight:700;\">" + matched + "</span>" + after;
    }

    function badgeFg(categoryLabel, kind) {
        if (categoryLabel === "porcelain" || categoryLabel === "git") return "#38BDF8";
        if (categoryLabel === "alias" || kind === "alias") return "#C084FC";
        if (categoryLabel === "ancillary" || categoryLabel === "interop") return "#7DD3FC";
        if (categoryLabel === "external") return "#FBBF24";
        if (categoryLabel === "flag" || kind === "flag") return "#34D399";
        if (categoryLabel === "branch" || categoryLabel === "ref" || kind === "branch") return "#4ADE80";
        if (categoryLabel === "remote" || kind === "remote") return "#60A5FA";
        if (categoryLabel === "tag" || kind === "tag") return "#FACC15";
        if (categoryLabel === "stash" || kind === "stash") return "#F472B6";
        if (categoryLabel === "file" || categoryLabel === "dir" || categoryLabel === "path") return "#2DD4BF";
        if (categoryLabel === "commit") return "#FB923C";
        return "#94A3B8";
    }

    function badgeBg(categoryLabel, kind) {
        if (categoryLabel === "porcelain" || categoryLabel === "git") return "#0C2A44";
        if (categoryLabel === "alias" || kind === "alias") return "#2B1547";
        if (categoryLabel === "ancillary" || categoryLabel === "interop") return "#10283E";
        if (categoryLabel === "external") return "#3A2509";
        if (categoryLabel === "flag" || kind === "flag") return "#083529";
        if (categoryLabel === "branch" || categoryLabel === "ref" || kind === "branch") return "#0A3320";
        if (categoryLabel === "remote" || kind === "remote") return "#10274A";
        if (categoryLabel === "tag" || kind === "tag") return "#382E08";
        if (categoryLabel === "stash" || kind === "stash") return "#3B132A";
        if (categoryLabel === "file" || categoryLabel === "dir" || categoryLabel === "path") return "#0A2E2C";
        if (categoryLabel === "commit") return "#3B1D0B";
        return "#1A2434";
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
                color: isSelected ? "#172A42" : (rowMa.containsMouse ? "#121E30" : "transparent")
                border.color: isSelected ? "#2C4B73" : "transparent"
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
                        color: rowItem.isSelected ? "#F8FAFC" : "#E2E8F0"
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
                        color: rowItem.isSelected ? "#CBD5E1" : "#64748B"
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
            color: "#090E17"
            radius: 9

            // Flatten top corners so footer connects seamlessly to popup body
            Rectangle {
                anchors.top: parent.top
                anchors.left: parent.left
                anchors.right: parent.right
                height: 8
                color: "#090E17"
            }

            Rectangle {
                anchors.top: parent.top
                anchors.left: parent.left
                anchors.right: parent.right
                height: 1
                color: "#1A2538"
            }

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 10
                anchors.rightMargin: 10
                spacing: 8

                Text {
                    text: "Tab completa  •  ↑↓ naviga  •  Invio esegui  •  Esc chiudi"
                    color: "#64748B"
                    font.pixelSize: 10
                    Layout.fillWidth: true
                    elide: Text.ElideRight
                }

                Text {
                    text: popupRoot.totalCount + (popupRoot.totalCount === 1 ? " risultato" : " risultati")
                    color: "#38BDF8"
                    font.pixelSize: 10
                    font.bold: true
                }
            }
        }
    }
}
