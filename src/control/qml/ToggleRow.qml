import QtQuick
import QtQuick.Layouts

FocusScope {
    id: root
    property string text: ""
    property string detail: ""
    property bool checked: false
    property color accent: "#b1edbc"
    signal toggled(bool value)
    implicitHeight: detail.length ? 51 : 34
    implicitWidth: 220
    Accessible.role: Accessible.CheckBox
    Accessible.name: text
    Accessible.checkable: true
    Accessible.checked: checked
    activeFocusOnTab: true
    opacity: enabled ? 1 : 0.38
    Keys.onSpacePressed: if (enabled)
        toggled(!checked)
    RowLayout {
        anchors.fill: parent
        spacing: 14
        ColumnLayout {
            spacing: 4
            Layout.fillWidth: true
            Text {
                text: root.text
                color: "#dce0e8"
                font.pixelSize: 12
                Layout.fillWidth: true
                elide: Text.ElideRight
            }
            Text {
                visible: root.detail.length > 0
                text: root.detail
                color: "#7f8797"
                font.pixelSize: 10
                Layout.fillWidth: true
                elide: Text.ElideRight
            }
        }
        Rectangle {
            width: 34
            height: 18
            radius: 9
            color: root.checked ? "#3c5344" : "#101217"
            border.color: root.activeFocus ? root.accent : root.checked ? "#6f9b7b" : "#3d424e"
            Rectangle {
                x: root.checked ? 18 : 3
                y: 3
                width: 12
                height: 12
                radius: 6
                color: root.checked ? root.accent : "#737b8a"
                Behavior on x {
                    NumberAnimation {
                        duration: 100
                    }
                }
            }
        }
    }
    MouseArea {
        anchors.fill: parent
        cursorShape: Qt.PointingHandCursor
        onClicked: {
            root.forceActiveFocus();
            root.toggled(!root.checked);
        }
    }
}
