import QtQuick
import QtQuick.Controls

Button {
    id: root
    property bool primary: false
    property bool compact: false
    property color tint: "#b1edbc"
    implicitHeight: compact ? 30 : 38
    implicitWidth: Math.max(compact ? 56 : 88, contentItem.implicitWidth + (compact ? 22 : 30))
    hoverEnabled: true
    font.pixelSize: compact ? 11 : 12
    font.weight: Font.DemiBold
    Accessible.name: text
    contentItem: Text {
        text: root.text
        font: root.font
        color: !root.enabled ? "#626977" : root.primary ? "#132019" : "#e5e7ec"
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
    }
    background: Rectangle {
        radius: 6
        color: root.primary ? (root.down ? Qt.darker(root.tint, 1.25) : root.hovered ? Qt.lighter(root.tint, 1.06) : root.tint) : (root.down ? "#30343e" : root.hovered ? "#2a2e37" : "#22252c")
        border.color: root.primary ? root.tint : root.activeFocus ? root.tint : "#393e49"
        opacity: root.enabled ? 1 : 0.45
    }
}
