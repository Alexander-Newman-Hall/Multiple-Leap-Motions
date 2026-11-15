import QtQuick
import QtQuick.Controls

ComboBox {
    id: root
    implicitHeight: 36
    implicitWidth: 140
    padding: 10
    leftPadding: 12
    rightPadding: 26
    font.pixelSize: 12
    contentItem: Text {
        text: root.displayText
        font: root.font
        color: root.enabled ? "#e7e9ef" : "#747b89"
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }
    background: Rectangle {
        radius: 5
        color: "#14161c"
        border.color: root.activeFocus ? "#a1c9aa" : "#373c47"
    }
    indicator: Text {
        text: "⌄"
        color: "#959dab"
        font.pixelSize: 17
        x: root.width - 22
        y: 5
    }
    delegate: ItemDelegate {
        required property int index
        required property var modelData
        width: root.width
        implicitHeight: 34
        contentItem: Text {
            text: modelData
            color: "#e7e9ef"
            font: root.font
            verticalAlignment: Text.AlignVCenter
        }
        background: Rectangle {
            color: parent.highlighted ? "#343d39" : "#20232a"
        }
        highlighted: root.highlightedIndex === index
    }
    popup: Popup {
        y: root.height + 4
        width: root.width
        padding: 1
        implicitHeight: Math.min(contentItem.implicitHeight + 2, 240)
        contentItem: ListView {
            clip: true
            implicitHeight: contentHeight
            model: root.popup.visible ? root.delegateModel : null
            currentIndex: root.highlightedIndex
            ScrollIndicator.vertical: ScrollIndicator {}
        }
        background: Rectangle {
            radius: 6
            color: "#20232a"
            border.color: "#454b58"
        }
    }
}
