import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../themes"

MenuItem {
    id: control

    readonly property FluxTheme theme: FluxTheme {}

    property bool selected: false
    readonly property bool isDestructive: text.indexOf("🗑") !== -1
        || text.indexOf("🔴") !== -1
        || text.indexOf("Scarta") !== -1
        || text.indexOf("Elimina") !== -1

    readonly property bool isActive: control.highlighted || control.hovered

    implicitWidth: Math.max(220, itemLabel.implicitWidth + (control.subMenu ? 48 : 30))
    implicitHeight: visible ? 32 : 0
    width: ListView.view ? ListView.view.width : implicitWidth
    height: visible ? 32 : 0
    padding: 0
    spacing: 8
    hoverEnabled: true

    font.pixelSize: 12
    font.bold: control.selected

    onImplicitWidthChanged: {
        if (control.menu && typeof control.menu.updateContentWidth === "function") {
            control.menu.updateContentWidth();
        }
    }
    onVisibleChanged: {
        if (control.menu && typeof control.menu.updateContentWidth === "function") {
            control.menu.updateContentWidth();
        }
    }

    indicator: Item {}
    arrow: Item {}

    background: Rectangle {
        x: 2
        y: 1
        width: Math.max(0, control.width - 4)
        height: Math.max(0, control.height - 2)
        radius: control.theme.radiusSm
        color: {
            if (!control.enabled) return "transparent";
            if (control.down) return control.isDestructive ? control.theme.dangerBg : control.theme.cardHover;
            if (control.isActive) return control.isDestructive ? control.theme.dangerBg : control.theme.card;
            if (control.selected) return control.theme.infoBg;
            return "transparent";
        }
        border.color: {
            if (!control.enabled) return "transparent";
            if (control.isActive) return control.isDestructive ? control.theme.danger : control.theme.borderHighlight;
            if (control.selected) return control.theme.accentBlue;
            return "transparent";
        }
        border.width: 1

        Behavior on color {
            ColorAnimation {
                duration: 110
            }
        }
        Behavior on border.color {
            ColorAnimation {
                duration: 110
            }
        }
    }

    contentItem: RowLayout {
        spacing: 8
        anchors.fill: parent
        anchors.leftMargin: 12
        anchors.rightMargin: 12

        Text {
            id: itemLabel
            text: control.text
            font.pixelSize: control.font.pixelSize
            font.bold: control.font.bold
            font.family: control.font.family
            color: {
                if (!control.enabled) return control.theme.textPlaceholder;
                if (control.isDestructive) {
                    return control.isActive ? control.theme.dangerText : control.theme.danger;
                }
                if (control.selected) return control.theme.borderFocus;
                return control.isActive ? control.theme.textPrimary : control.theme.textSecondary;
            }
            elide: Text.ElideRight
            verticalAlignment: Text.AlignVCenter
            Layout.fillWidth: true

            Behavior on color {
                ColorAnimation {
                    duration: 110
                }
            }
        }

        Text {
            visible: control.subMenu !== null
            text: "▸"
            color: control.isActive ? control.theme.borderFocus : control.theme.textMuted
            font.pixelSize: 12
            font.bold: true
            verticalAlignment: Text.AlignVCenter
        }
    }
}
