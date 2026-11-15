import QtQuick

Item {
    id: root
    property var devices: []
    property bool processed: false
    property string units: "mm"
    Canvas {
        id: view
        anchors.fill: parent
        Connections {
            target: root
            function onDevicesChanged() {
                view.requestPaint();
            }
            function onProcessedChanged() {
                view.requestPaint();
            }
            function onUnitsChanged() {
                view.requestPaint();
            }
        }
        onWidthChanged: requestPaint()
        onHeightChanged: requestPaint()
        onPaint: {
            const c = getContext("2d");
            c.reset();
            c.strokeStyle = "#292e37";
            c.lineWidth = 1;
            for (let x = 0; x < width; x += 24) {
                c.beginPath();
                c.moveTo(x, 0);
                c.lineTo(x, height);
                c.stroke();
            }
            for (let y = 0; y < height; y += 24) {
                c.beginPath();
                c.moveTo(0, y);
                c.lineTo(width, y);
                c.stroke();
            }
            c.setLineDash([3, 5]);
            c.strokeStyle = "#48505e";
            c.beginPath();
            c.moveTo(width / 2, 16);
            c.lineTo(width / 2, height - 12);
            c.stroke();
            c.setLineDash([]);
            const count = Math.min(2, root.devices.length);
            for (let d = 0; d < 2; d++) {
                const dev = d < count ? root.devices[d] : null;
                const cx = width * (d === 0 ? 0.25 : 0.75), cy = height * 0.55;
                const p = dev ? (root.processed ? dev.processed : dev.raw) : null;
                c.fillStyle = "#788295";
                c.font = "10px sans-serif";
                c.textAlign = "center";
                c.fillText(dev ? dev.alias + " / " + dev.serial : "CAMERA " + (d + 1), cx, 20);
                if (!p || !dev.frames_fresh || !dev.hands_visible) {
                    c.fillStyle = "#647080";
                    c.fillText(dev ? "NO LIVE HAND" : "WAITING FOR ENGINE", cx, cy);
                    continue;
                }
                const meters = root.processed && root.units === "m";
                const scl = meters ? 900 : 0.9;
                function project(v) {
                    return [cx + (Number(v[0]) - (meters ? 0.06 : 60)) * scl, cy - (Number(v[1]) - (meters ? 0.19 : 190)) * scl];
                }
                const palm = project(p.palm), wrist = project(p.wrist);
                const tint = root.processed ? "#bbaff3" : (d === 0 ? "#b1edbc" : "#9fcbdc");
                c.strokeStyle = tint;
                c.lineWidth = 2;
                c.lineCap = "round";
                c.beginPath();
                c.moveTo(wrist[0], wrist[1]);
                c.lineTo(palm[0], palm[1]);
                c.stroke();
                const tips = p.tips || [];
                for (let i = 0; i < tips.length; i++) {
                    const t = project(tips[i]);
                    c.globalAlpha = 0.65;
                    c.beginPath();
                    c.moveTo(palm[0], palm[1]);
                    c.lineTo(t[0], t[1]);
                    c.stroke();
                    c.globalAlpha = 1;
                    c.fillStyle = "#15191e";
                    c.beginPath();
                    c.arc(t[0], t[1], 4, 0, Math.PI * 2);
                    c.fill();
                    c.stroke();
                }
                c.fillStyle = "#171d1c";
                c.beginPath();
                c.arc(palm[0], palm[1], 10, 0, Math.PI * 2);
                c.fill();
                c.stroke();
                c.fillStyle = tint;
                c.beginPath();
                c.arc(palm[0], palm[1], 3, 0, Math.PI * 2);
                c.fill();
                c.fillStyle = "#9ca5b5";
                c.font = "10px monospace";
                c.fillText(p.hand + " / " + Number(p.palm[0]).toFixed(1) + ", " + Number(p.palm[1]).toFixed(1) + ", " + Number(p.palm[2]).toFixed(1), cx, height - 14);
            }
        }
    }
}
