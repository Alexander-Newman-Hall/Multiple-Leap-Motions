import QtQuick

Item {
    id: root
    property var devices: []
    property bool processed: true
    property string units: "mm"
    property string outputMessage: ""
    property var deviceSettings: ({})
    readonly property var cameras: Array.from(devices).filter(device => device.present !== false).slice(0, 2)
    readonly property int paneCount: Math.max(1, cameras.length)
    function project(point, centerX) {
        // In desktop mode positive Z points toward the user (down on screen).
        const pixelsPerMm = Math.min(width / (paneCount === 1 ? 650 : 800), Math.max(1, height - 56) / 320);
        const scale = pixelsPerMm * (processed && units === "m" ? 1000 : 1);
        return Qt.point(centerX + point[0] * scale, height / 2 + point[2] * scale);
    }
    function drawHand(c, p, cx) {
        const palm = root.project(p.palm, cx);
        const tint = root.processed ? (p.hand === "right" ? "#f1b4d5" : "#bbaff3") : (p.hand === "right" ? "#9fcbdc" : "#b1edbc");
        c.strokeStyle = tint;
        c.lineWidth = 2;
        c.lineCap = "round";
        if (p.wrist) {
            const wrist = root.project(p.wrist, cx);
            c.beginPath();
            c.moveTo(wrist.x, wrist.y);
            c.lineTo(palm.x, palm.y);
            c.stroke();
        }
        const tips = p.tips || [];
        for (let i = 0; i < tips.length; i++) {
            const t = root.project(tips[i], cx);
            c.globalAlpha = 0.65;
            c.beginPath();
            c.moveTo(palm.x, palm.y);
            c.lineTo(t.x, t.y);
            c.stroke();
            c.globalAlpha = 1;
            c.fillStyle = "#15191e";
            c.beginPath();
            c.arc(t.x, t.y, 4, 0, Math.PI * 2);
            c.fill();
            c.stroke();
        }
        c.fillStyle = "#171d1c";
        c.beginPath();
        c.arc(palm.x, palm.y, 10, 0, Math.PI * 2);
        c.fill();
        c.stroke();
        c.fillStyle = tint;
        c.beginPath();
        c.arc(palm.x, palm.y, 3, 0, Math.PI * 2);
        c.fill();
        c.textAlign = "center";
        c.font = "bold 10px sans-serif";
        c.fillText(p.hand.toUpperCase(), palm.x, palm.y - 14);
    }
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
            function onOutputMessageChanged() { view.requestPaint(); }
            function onDeviceSettingsChanged() { view.requestPaint(); }
        }
        onWidthChanged: requestPaint()
        onHeightChanged: requestPaint()
        onPaint: {
            const c = getContext("2d");
            c.reset();
            c.clearRect(0, 0, width, height);
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
            if (root.paneCount > 1) {
                c.setLineDash([3, 5]);
                c.strokeStyle = "#48505e";
                c.beginPath();
                c.moveTo(width / 2, 16);
                c.lineTo(width / 2, height - 12);
                c.stroke();
                c.setLineDash([]);
            }
            const paneWidth = width / root.paneCount;
            for (let d = 0; d < root.paneCount; d++) {
                const dev = root.cameras[d];
                const cx = paneWidth * (d + 0.5), cy = height * 0.55;
                const first = dev ? (root.processed ? dev.processed : dev.raw) : null;
                const all = dev ? (root.processed ? dev.processed_hands : dev.raw_hands) : null;
                const hands = all || (first ? [first] : []);
                const route = dev && root.processed ? root.deviceSettings[dev.serial] : null;
                const muted = route && (route.enabled === false || route.hand_filter === "disabled");
                let heading = dev ? dev.alias + " / " + dev.serial : "CAMERA " + (d + 1);
                if (route?.hand_filter && !muted)
                    heading = dev.alias + " / " + (route.hand_filter === "both" ? "BOTH HANDS" : route.hand_filter.toUpperCase() + " ONLY");
                c.fillStyle = "#788295";
                c.font = "10px sans-serif";
                c.textAlign = "center";
                c.fillText(heading, cx, 20);
                const outputStatus = root.processed && (root.outputMessage || (muted ? "CAMERA OUTPUT DISABLED" : ""));
                if (!dev || outputStatus || !dev.frames_fresh || !hands.length) {
                    c.fillStyle = "#647080";
                    const emptyText = root.processed ? "NO ROUTED HAND" : "NO LIVE HAND";
                    c.fillText(dev ? (outputStatus || emptyText) : "WAITING FOR CAMERA", cx, cy);
                    continue;
                }
                c.save();
                c.beginPath();
                c.rect(d * paneWidth, 28, paneWidth, Math.max(0, height - 56));
                c.clip();
                for (let i = 0; i < hands.length; ++i)
                    root.drawHand(c, hands[i], cx);
                c.restore();
                c.fillStyle = "#9ca5b5";
                c.font = "10px monospace";
                const h = hands[0];
                const caption = hands.length === 1
                    ? h.hand + " / " + Number(h.palm[0]).toFixed(1) + ", " + Number(h.palm[1]).toFixed(1) + ", " + Number(h.palm[2]).toFixed(1)
                    : hands.length + " hands";
                c.fillText(caption, cx, height - 14);
            }
        }
    }
}
