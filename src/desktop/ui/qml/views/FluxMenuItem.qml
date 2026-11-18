import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

MenuItem {
    id: control

    property bool destructive: false
    readonly property bool isDestructive: destructive
        || text.indexOf("🗑") !== -1
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

    indicator: Item {}
    arrow: Item {}

    background: Rectangle {
        x: 2
        y: 1
        width: Math.max(0, control.width - 4)
        height: Math.max(0, control.height - 2)
        radius: 8
        color: {
            if (!control.enabled) return "transparent";
            if (control.down) return control.isDestructive ? "#4C151E" : "#243652";
            if (control.isActive) return control.isDestructive ? "#38121A" : "#1C2A40";
            return "transparent";
        }
        border.color: {
            if (!control.enabled || !control.isActive) return "transparent";
            return control.isDestructive ? "#7F1D1D" : "#2D4263";
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
                if (!control.enabled) return "#475569";
                if (control.isDestructive) {
                    return control.isActive ? "#FCA5A5" : "#F87171";
                }
                return control.isActive ? "#F8FAFC" : "#CBD5E1";
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
            color: control.isActive ? "#38BDF8" : "#64748B"
            font.pixelSize: 12
            font.bold: true
            verticalAlignment: Text.AlignVCenter
        }
    }
}
