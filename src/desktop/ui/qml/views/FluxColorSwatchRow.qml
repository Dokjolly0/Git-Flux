import QtQuick
import QtQuick.Layouts
import "../themes"

RowLayout {
    id: root

    FluxTheme { id: theme }

    property var paletteColors: theme.accentPalette
    property string selectedColor: ""
    property bool showDefaultOption: false
    property string defaultLabel: "Predefinito"
    property color defaultAccentColor: theme.defaultTabAccent
    property int swatchSize: 24
    property string objectNamePrefix: ""

    signal colorPicked(string hexColor)

    spacing: 7
    Layout.preferredHeight: 28
    implicitHeight: 28

    Rectangle {
        id: defaultPill
        visible: root.showDefaultOption
        objectName: root.objectNamePrefix.length > 0 ? (root.objectNamePrefix + "default") : ""
        readonly property bool isSelected: root.selectedColor.length === 0
        Layout.preferredHeight: 22
        implicitWidth: defaultPillText.implicitWidth + 12
        radius: theme.radiusXs
        color: isSelected
            ? theme.surfaceGlass
            : (defaultMa.containsMouse ? theme.card : theme.inputBg)
        border.color: isSelected
            ? root.defaultAccentColor
            : (defaultMa.containsMouse ? theme.borderHighlight : theme.borderSubtle)
        border.width: 1

        Text {
            id: defaultPillText
            anchors.centerIn: parent
            text: root.defaultLabel
            color: defaultPill.isSelected ? theme.textPrimary : theme.textSecondary
            font.pixelSize: 10
            font.bold: defaultPill.isSelected
        }

        MouseArea {
            id: defaultMa
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: {
                root.selectedColor = "";
                root.colorPicked("");
            }
        }
    }

    Repeater {
        model: root.paletteColors
        delegate: Rectangle {
            id: swatchRect
            required property string modelData
            objectName: root.objectNamePrefix.length > 0 ? (root.objectNamePrefix + modelData) : ""
            readonly property bool isSelected: root.selectedColor.toLowerCase() === modelData.toLowerCase()

            implicitWidth: root.swatchSize
            implicitHeight: root.swatchSize
            Layout.preferredWidth: root.swatchSize
            Layout.preferredHeight: root.swatchSize
            Layout.alignment: Qt.AlignVCenter
            width: root.swatchSize
            height: root.swatchSize
            radius: root.swatchSize / 2
            color: modelData
            border.color: isSelected
                ? theme.textPrimary
                : (swatchMa.containsMouse ? theme.textSecondary : "transparent")
            border.width: isSelected ? 2.5 : 1.5
            scale: isSelected ? 1.12 : (swatchMa.containsMouse ? 1.06 : 1.0)

            Behavior on scale { NumberAnimation { duration: 100 } }
            Behavior on border.color { ColorAnimation { duration: 100 } }

            Text {
                anchors.centerIn: parent
                visible: swatchRect.isSelected
                text: "✓"
                color: theme.inputBg
                font.pixelSize: Math.max(10, Math.round(root.swatchSize * 0.5))
                font.bold: true
            }

            MouseArea {
                id: swatchMa
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: {
                    root.selectedColor = swatchRect.modelData;
                    root.colorPicked(swatchRect.modelData);
                }
            }
        }
    }
}
