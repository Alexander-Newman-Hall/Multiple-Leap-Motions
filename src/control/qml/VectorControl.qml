import QtQuick
import QtQuick.Layouts

ColumnLayout {
    id: root
    property string label: "VECTOR"
    property string suffix: ""
    property var values: [0, 0, 0]
    property int decimals: 2
    signal valueEdited(int axis, real value)
    spacing: 10
    Text {
        text: root.label
        color: "#9098a8"
        font.pixelSize: 10
        font.letterSpacing: 0.7
    }
    RowLayout {
        spacing: 8
        Repeater {
            model: ["X", "Y", "Z"]
            delegate: RowLayout {
                required property int index
                required property string modelData
                spacing: 6
                Text {
                    text: modelData
                    color: index === 0 ? "#e49fa1" : index === 1 ? "#b1edbc" : "#a0b7eb"
                    font.pixelSize: 10
                }
                StudioField {
                    Layout.fillWidth: true
                    Layout.minimumWidth: 55
                    text: Number(root.values[index] === undefined ? 0 : root.values[index]).toFixed(root.decimals)
                    horizontalAlignment: Text.AlignHCenter
                    font.pixelSize: 11
                    validator: DoubleValidator {
                        locale: "C"
                        notation: DoubleValidator.StandardNotation
                    }
                    Accessible.name: root.label + " " + modelData
                    onEditingFinished: if (acceptableInput)
                        root.valueEdited(index, Number.fromLocaleString(Qt.locale("C"), text))
                }
            }
        }
        Text {
            visible: root.suffix.length > 0
            text: root.suffix
            color: "#7b8494"
            font.pixelSize: 10
        }
    }
}
