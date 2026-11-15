import QtQuick
import QtQuick.Layouts

Rectangle {
    id: root
    property var device: ({})
    property int cameraIndex: 0
    property color accent: cameraIndex === 0 ? "#b1edbc" : "#a6cfdf"
    color: "#1b1d23"
    radius: 12
    border.color: "#30343e"
    implicitHeight: 186
    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 18
        spacing: 11
        RowLayout {
            Layout.fillWidth: true
            spacing: 10
            Rectangle {
                width: 34
                height: 34
                radius: 8
                color: "#252e2a"
                border.color: "#3c5143"
                Text {
                    anchors.centerIn: parent
                    text: "0" + (root.cameraIndex + 1)
                    color: root.accent
                    font.pixelSize: 12
                    font.weight: Font.DemiBold
                }
            }
            ColumnLayout {
                spacing: 3
                Layout.fillWidth: true
                Text {
                    text: root.device.alias || "Camera " + (root.cameraIndex + 1)
                    color: "#eceef3"
                    font.pixelSize: 15
                    font.weight: Font.DemiBold
                }
                Text {
                    text: root.device.serial || "Awaiting discovery"
                    color: "#7e899a"
                    font.pixelSize: 10
                }
            }
            StatePill {
                text: root.device.frames_fresh ? "STREAMING" : (root.device.state || "OFFLINE").toUpperCase()
                live: !!root.device.frames_fresh
                tint: root.accent
            }
        }
        RowLayout {
            Layout.fillWidth: true
            spacing: 6
            Repeater {
                model: [
                    {
                        n: "DEVICE",
                        v: !!root.device.present
                    },
                    {
                        n: "SUBSCRIBED",
                        v: !!root.device.subscribed
                    },
                    {
                        n: "FRAMES",
                        v: !!root.device.frames_fresh
                    }
                ]
                delegate: RowLayout {
                    required property var modelData
                    spacing: 5
                    Layout.fillWidth: true
                    Rectangle {
                        width: 4
                        height: 4
                        radius: 2
                        color: modelData.v ? root.accent : "#555d6c"
                    }
                    Text {
                        text: modelData.n
                        color: modelData.v ? "#aab7b3" : "#636e7d"
                        font.pixelSize: 8
                        font.letterSpacing: 0.5
                    }
                }
            }
        }
        Rectangle {
            Layout.fillWidth: true
            height: 1
            color: "#2d323c"
        }
        RowLayout {
            Layout.fillWidth: true
            spacing: 10
            ColumnLayout {
                spacing: 4
                Layout.fillWidth: true
                RowLayout {
                    spacing: 4
                    Text {
                        text: root.device.input_fps === undefined ? "—" : Number(root.device.input_fps).toFixed(0)
                        color: "#e3e8ee"
                        font.pixelSize: 22
                        font.weight: Font.Medium
                    }
                    Text {
                        text: "Hz"
                        color: "#747f90"
                        font.pixelSize: 10
                        Layout.alignment: Qt.AlignBottom
                    }
                }
                Text {
                    text: "INPUT RATE"
                    color: "#747f90"
                    font.pixelSize: 8
                    font.letterSpacing: 0.8
                }
            }
            ColumnLayout {
                spacing: 5
                Layout.fillWidth: true
                Text {
                    text: root.device.last_frame_age_ms === null || root.device.last_frame_age_ms === undefined ? "—" : root.device.last_frame_age_ms + " ms"
                    color: "#c6ced9"
                    font.pixelSize: 16
                }
                Text {
                    text: "FRAME AGE"
                    color: "#747f90"
                    font.pixelSize: 8
                    font.letterSpacing: 0.8
                }
            }
            ColumnLayout {
                spacing: 5
                Layout.fillWidth: true
                Text {
                    text: root.device.hands_visible ? "Visible" : "No hands"
                    color: root.device.hands_visible ? root.accent : "#9ca5b4"
                    font.pixelSize: 12
                }
                Text {
                    text: "HAND STATUS"
                    color: "#747f90"
                    font.pixelSize: 8
                    font.letterSpacing: 0.8
                }
            }
        }
        Rectangle {
            Layout.fillWidth: true
            height: 3
            radius: 2
            color: "#101319"
            Rectangle {
                height: parent.height
                radius: 2
                color: root.accent
                width: parent.width * Math.max(0, Math.min(1, (root.device.frames_fresh ? root.device.input_fps || 0 : 0) / 120))
            }
        }
    }
}
