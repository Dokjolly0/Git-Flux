import QtQuick
import QtQuick.Controls
import QtQuick.Templates as T

T.Menu {
    id: control

    function computeMaxItemWidth() {
        var maxW = 220;
        for (var i = 0; i < control.count; ++i) {
            var it = control.itemAt(i);
            if (it && it.visible && it.implicitWidth > maxW) {
                maxW = it.implicitWidth;
            }
        }
        return Math.min(480, maxW);
    }

    property real calculatedContentWidth: 220

    onAboutToShow: {
        calculatedContentWidth = computeMaxItemWidth();
    }

    implicitWidth: Math.max(implicitBackgroundWidth + leftInset + rightInset,
                            calculatedContentWidth + leftPadding + rightPadding)
    implicitHeight: Math.max(implicitBackgroundHeight + topInset + bottomInset,
                             implicitContentHeight + topPadding + bottomPadding)

    margins: 6
    overlap: 4
    topPadding: 6
    bottomPadding: 6
    leftPadding: 6
    rightPadding: 6

    transformOrigin: Item.Top

    enter: Transition {
        ParallelAnimation {
            NumberAnimation {
                property: "opacity"
                from: 0.0
                to: 1.0
                duration: 130
                easing.type: Easing.OutCubic
            }
            NumberAnimation {
                property: "scale"
                from: 0.96
                to: 1.0
                duration: 140
                easing.type: Easing.OutCubic
            }
        }
    }

    exit: Transition {
        ParallelAnimation {
            NumberAnimation {
                property: "opacity"
                from: 1.0
                to: 0.0
                duration: 90
                easing.type: Easing.InCubic
            }
            NumberAnimation {
                property: "scale"
                from: 1.0
                to: 0.97
                duration: 90
                easing.type: Easing.InCubic
            }
        }
    }

    delegate: FluxMenuItem {}

    contentItem: ListView {
        implicitHeight: contentHeight
        implicitWidth: control.calculatedContentWidth
        model: control.contentModel
        interactive: Window.window
            ? contentHeight + control.topPadding + control.bottomPadding > control.height
            : false
        clip: true
        currentIndex: control.currentIndex
        spacing: 1
        boundsBehavior: Flickable.StopAtBounds
    }

    background: Rectangle {
        implicitWidth: 220
        implicitHeight: 36
        color: "#111824"
        radius: 12
        border.color: "#28374E"
        border.width: 1

        // Soft multi-layer ambient shadow (zero external shader module required)
        Rectangle {
            z: -3
            anchors.fill: parent
            anchors.margins: -12
            anchors.topMargin: -8
            anchors.bottomMargin: -16
            radius: 22
            color: "#000000"
            opacity: 0.16
        }
        Rectangle {
            z: -2
            anchors.fill: parent
            anchors.margins: -6
            anchors.topMargin: -4
            anchors.bottomMargin: -9
            radius: 17
            color: "#000000"
            opacity: 0.24
        }
        Rectangle {
            z: -1
            anchors.fill: parent
            anchors.margins: -2
            anchors.topMargin: -1
            anchors.bottomMargin: -3
            radius: 14
            color: "#000000"
            opacity: 0.34
        }

        // Top subtle specular highlight
        Rectangle {
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.topMargin: 1
            anchors.leftMargin: 10
            anchors.rightMargin: 10
            height: 1
            color: "#FFFFFF"
            opacity: 0.07
        }
    }
}
