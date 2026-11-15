import QtQuick

QtObject {
    readonly property color background: "#111216"
    readonly property color rail: "#15161b"
    readonly property color panel: "#1b1d23"
    readonly property color inset: "#14161b"
    readonly property color border: "#2e313a"
    readonly property color text: "#edeef2"
    readonly property color muted: "#9298a6"
    readonly property color dim: "#626977"
    readonly property color accent: "#b1edbc"
    readonly property color violet: "#bbaff3"
    readonly property color amber: "#e9c184"
    readonly property color red: "#ee9396"
    readonly property string family: Qt.platform.os === "windows" ? "Segoe UI" : "DejaVu Sans"
    readonly property string mono: Qt.platform.os === "windows" ? "Consolas" : "DejaVu Sans Mono"
}
