import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: root
    property string label: "PARAMETER"
    property string suffix: ""
    property real value: 0
    property real minimum: 0
    property real maximum: 100
    property real step: 1
    property int decimals: 0
    property color accent: "#bbaff3"
    signal valueEdited(real value)
    implicitWidth: 128
    implicitHeight: 166
    opacity: enabled ? 1 : 0.32
    Text {
        anchors.top: parent.top
        anchors.horizontalCenter: parent.horizontalCenter
        text: root.label
        color: "#b1b7c4"
        font.pixelSize: 9
        font.weight: Font.DemiBold
        font.letterSpacing: 1.1
    }
    Dial {
        id: dial
        objectName: root.objectName + "Input"
        anchors.horizontalCenter: parent.horizontalCenter
        y: 23
        width: 88
        height: 88
        from: root.minimum
        to: root.maximum
        stepSize: root.step
        value: root.value
        wheelEnabled: true
        inputMode: Dial.Vertical
        Accessible.name: root.label
        onMoved: root.valueEdited(value)
        background: Canvas {
            id: face
            anchors.fill: parent
            Connections {
                target: root
                function onValueChanged() {
                    face.requestPaint();
                }
                function onAccentChanged() {
                    face.requestPaint();
                }
            }
            onPaint: {
                const c = getContext("2d");
                c.reset();
                const cx = width / 2, cy = height / 2, start = Math.PI * 0.75, sweep = Math.PI * 1.5;
                const fraction = Math.max(0, Math.min(1, (root.value - root.minimum) / (root.maximum - root.minimum)));
                c.lineCap = "round";
                c.lineWidth = 2.5;
                c.strokeStyle = "#363b46";
                c.beginPath();
                c.arc(cx, cy, 39, start, start + sweep);
                c.stroke();
                c.strokeStyle = root.accent;
                c.beginPath();
                c.arc(cx, cy, 39, start, start + sweep * fraction);
                c.stroke();
                const g = c.createRadialGradient(cx - 10, cy - 12, 2, cx, cy, 33);
                g.addColorStop(0, "#3e434e");
                g.addColorStop(0.6, "#2c3039");
                g.addColorStop(1, "#17191f");
                c.fillStyle = g;
                c.strokeStyle = "#454b58";
                c.lineWidth = 1;
                c.beginPath();
                c.arc(cx, cy, 31, 0, Math.PI * 2);
                c.fill();
                c.stroke();
                c.strokeStyle = root.accent;
                c.lineWidth = 2.5;
                const a = start + sweep * fraction;
                c.beginPath();
                c.moveTo(cx + Math.cos(a) * 19, cy + Math.sin(a) * 19);
                c.lineTo(cx + Math.cos(a) * 27, cy + Math.sin(a) * 27);
                c.stroke();
                c.strokeStyle = "#16ffffff";
                c.beginPath();
                c.arc(cx, cy, 28, Math.PI, Math.PI * 1.8);
                c.stroke();
            }
        }
        handle: Item {}
    }
    RowLayout {
        anchors.horizontalCenter: parent.horizontalCenter
        y: 124
        spacing: 5
        StudioField {
            id: field
            objectName: root.objectName + "Value"
            Layout.preferredWidth: 67
            implicitHeight: 28
            padding: 4
            horizontalAlignment: Text.AlignHCenter
            text: Number(root.value).toFixed(root.decimals)
            font.family: Qt.platform.os === "windows" ? "Consolas" : "DejaVu Sans Mono"
            font.pixelSize: 11
            validator: DoubleValidator {
                locale: "C"
                bottom: root.minimum
                top: root.maximum
                decimals: root.decimals
                notation: DoubleValidator.StandardNotation
            }
            onEditingFinished: if (acceptableInput)
                root.valueEdited(Number.fromLocaleString(Qt.locale("C"), text))
            Accessible.name: root.label + " value"
        }
        Text {
            text: root.suffix
            color: "#788293"
            font.pixelSize: 10
        }
    }
}
