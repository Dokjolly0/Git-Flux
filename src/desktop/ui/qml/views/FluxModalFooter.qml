import QtQuick
import QtQuick.Layouts
import "../themes"

Rectangle {
    id: root

    FluxTheme { id: theme }

    property int footerHeight: 50
    property int horizontalMargin: 16
    property int buttonSpacing: 8
    default property alias actions: row.data

    color: theme.footerBg
    implicitHeight: footerHeight
    height: footerHeight
    radius: theme.radiusLg

    // Flatten top corners so footer joins the dialog body seamlessly.
    Rectangle {
        anchors.top: parent.top
        width: parent.width
        height: root.radius
        color: root.color
    }

    Rectangle {
        anchors.top: parent.top
        width: parent.width
        height: 1
        color: theme.border
    }

    RowLayout {
        id: row
        anchors.fill: parent
        anchors.leftMargin: root.horizontalMargin
        anchors.rightMargin: root.horizontalMargin
        spacing: root.buttonSpacing
    }
}
