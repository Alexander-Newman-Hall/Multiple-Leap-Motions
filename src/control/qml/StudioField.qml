import QtQuick
import QtQuick.Controls

TextField {
    id: root
    implicitHeight: 36
    padding: 10
    color: "#edeef2"
    placeholderTextColor: "#737b8c"
    selectionColor: "#668b71"
    selectedTextColor: "#ffffff"
    font.pixelSize: 12
    selectByMouse: true
    background: Rectangle {
        radius: 5
        color: "#13151a"
        border.color: !root.acceptableInput ? "#b9757c" : root.activeFocus ? "#a1c9aa" : "#373c47"
    }
    opacity: enabled ? 1 : 0.45
}
