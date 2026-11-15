import QtQuick

Rectangle {
    id: root
    property string text: ""
    property color tint: "#b1edbc"
    property bool live: false
    implicitWidth: label.implicitWidth + 25
    implicitHeight: 25
    radius: 5
    color: live ? "#26352b" : "#25282f"
    border.color: live ? "#405849" : "#393e49"
    Text {
        id: label
        anchors.centerIn: parent
        text: root.text
        color: root.live ? root.tint : "#919aaa"
        font.pixelSize: 9
        font.weight: Font.DemiBold
        font.letterSpacing: 0.6
    }
}
