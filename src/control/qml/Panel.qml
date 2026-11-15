import QtQuick

Rectangle {
    id: root
    property string title: ""
    property string subtitle: ""
    property string badge: ""
    property color badgeColor: "#9298a6"
    default property alias panelContent: content.data
    color: "#1b1d23"
    radius: 12
    border.color: "#30343e"
    implicitHeight: 200
    Text {
        id: titleText
        x: 20
        y: 18
        text: root.title
        color: "#e4e7ed"
        font.pixelSize: 12
        font.weight: Font.DemiBold
        font.letterSpacing: 0.3
    }
    Text {
        x: 20
        y: 39
        text: root.subtitle
        color: "#838b9b"
        font.pixelSize: 10
        visible: text.length > 0
    }
    Text {
        anchors.right: parent.right
        anchors.rightMargin: 20
        y: 20
        text: root.badge
        color: root.badgeColor
        font.pixelSize: 9
        font.letterSpacing: 1
        font.weight: Font.DemiBold
    }
    Item {
        id: content
        x: 20
        y: root.subtitle.length ? 65 : root.title.length ? 48 : 20
        width: root.width - 40
        height: root.height - y - 20
    }
}
