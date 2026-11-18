import QtQuick
import QtQuick.Controls

CheckBox {
    id: control

    property color textColor: "#CBD5E1"
    property color checkedTextColor: "#F8FAFC"
    property int fontSize: 11

    hoverEnabled: true
    padding: 2
    spacing: 8

    indicator: Rectangle {
        implicitWidth: 18
        implicitHeight: 18
        x: control.leftPadding
        y: parent.height / 2 - height / 2
        radius: 6
        color: control.checked
            ? (control.hovered ? "#0EA5E9" : "#0284C7")
            : (control.hovered ? "#1E293B" : "#0B1019")
        border.color: control.checked
            ? "#38BDF8"
            : (control.activeFocus ? "#38BDF8" : (control.hovered ? "#475569" : "#2D3B4E"))
        border.width: 1

        Behavior on color {
            ColorAnimation {
                duration: 120
            }
        }
        Behavior on border.color {
            ColorAnimation {
                duration: 120
            }
        }

        Text {
            anchors.centerIn: parent
            text: "✓"
            color: "#FFFFFF"
            font.pixelSize: 11
            font.bold: true
            opacity: control.checked ? 1.0 : 0.0
            scale: control.checked ? 1.0 : 0.6

            Behavior on opacity {
                NumberAnimation {
                    duration: 110
                }
            }
            Behavior on scale {
                NumberAnimation {
                    duration: 130
                    easing.type: Easing.OutBack
                }
            }
        }
    }

    contentItem: Text {
        text: control.text
        font.pixelSize: control.fontSize
        font.bold: control.font.bold
        color: !control.enabled ? "#475569" : (control.checked ? control.checkedTextColor : control.textColor)
        leftPadding: control.indicator && control.indicator.visible ? control.indicator.width + control.spacing : 0
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight

        Behavior on color {
            ColorAnimation {
                duration: 120
            }
        }
    }
}
