import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ApplicationWindow {
    id: window
    objectName: "handbridgeWindow"
    width: 1360
    height: 860
    minimumWidth: 1100
    minimumHeight: 720
    visible: true
    title: "Hand Bridge " + Qt.application.version + " — Native Control"
    color: theme.background
    font.family: theme.family
    font.pixelSize: 12
    palette.window: theme.background
    palette.text: theme.text
    palette.buttonText: theme.text
    palette.highlight: theme.accent
    Theme {
        id: theme
    }
    property int pageIndex: 0
    property bool previewProcessed: true
    readonly property var pageNames: ["Overview", "Devices", "Processing", "OSC Output", "Profiles & Diagnostics"]
    readonly property var pageSubtitles: ["Independent cameras. One dependable signal path.", "Identity, routing, and recovery for each camera.", "Shape the output. Keep the raw observation intact.", "Choose exactly what TouchDesigner receives.", "Recall a session. Inspect the engine's actual state."]
    readonly property var deviceRows: controller.devices || []
    readonly property var output: controller.status.output || ({})
    readonly property var sendErrors: output.send_errors_exact ?? output.send_errors
    readonly property var tracking: controller.status.tracking || ({})
    readonly property string lastTrackingError: tracking.last_error || ""
    readonly property bool processingBypass: setting("processing.bypass", true)
    readonly property var appliedProcessing: controller.config.processing || ({})
    readonly property bool appliedBypass: appliedProcessing.bypass !== false
    readonly property string appliedPreviewUnits: appliedBypass ? "mm" : (appliedProcessing.output_units || "mm")
    readonly property string routedOutputMessage: {
        const applied = controller.config;
        if (applied.osc?.enabled === false)
            return "OSC MUTED";
        if (applied.channels && !Object.values(applied.channels).some(enabled => enabled)
                && !appliedProcessing.gestures_enabled)
            return "NO OSC CHANNELS ENABLED";
        return "";
    }
    function setting(path, fallback) {
        let object = controller.draft;
        const parts = path.split(".");
        for (let i = 0; i < parts.length; i++) {
            if (object === null || object === undefined || object[parts[i]] === undefined)
                return fallback;
            object = object[parts[i]];
        }
        return object;
    }
    function deviceSetting(serial, field, fallback) {
        const devices = controller.draft.devices;
        const device = devices ? devices[serial] : undefined;
        return device && device[field] !== undefined ? device[field] : fallback;
    }
    function edit(path, value) {
        controller.setSetting(path, value);
    }
    function metric(number, decimals) {
        return number === undefined || number === null ? "—" : Number(number).toFixed(decimals || 0);
    }
    function liveDevices() {
        let n = 0;
        for (let i = 0; i < deviceRows.length; i++)
            if (deviceRows[i].frames_fresh)
                n++;
        return n;
    }
    function framesProcessed() {
        let n = 0;
        for (let i = 0; i < deviceRows.length; i++)
            n += Number(deviceRows[i].frames_processed || 0);
        return n;
    }
    Component.onCompleted: {
        if (typeof initialPage !== "undefined") {
            const keys = ["overview", "devices", "processing", "osc", "profiles"];
            const chosen = keys.indexOf(initialPage);
            if (chosen >= 0)
                pageIndex = chosen;
        }
    }

    component SmallLabel: Text {
        color: "#818b9d"
        font.pixelSize: 9
        font.letterSpacing: 1.1
        font.weight: Font.DemiBold
    }
    component SectionHeading: Text {
        color: "#eef0f5"
        font.pixelSize: 23
        font.weight: Font.DemiBold
    }
    component MetricCell: ColumnLayout {
        id: metricRoot
        property string label: ""
        property var total
        readonly property string exactTotal: total === undefined || total === null ? "" : String(total)
        property string value: compactCount(exactTotal)
        property string unit: ""
        property color tint: "#e7ebf2"
        // Work directly with decimal digits so the tooltip can show every
        // increment, including totals beyond JavaScript's safe integer range.
        function compactCount(digits) {
            if (!digits)
                return "—";
            if (digits.length <= 4)
                return digits;
            const group = Math.floor((digits.length - 1) / 3);
            const suffixes = ["", "K", "M", "B", "T"];
            if (group >= suffixes.length)
                return digits[0] + "." + digits[1] + "e" + (digits.length - 1);
            const whole = digits.length - 3 * group;
            const tenth = digits[whole];
            return digits.slice(0, whole) + (whole < 3 && tenth !== "0" ? "." + tenth : "") + suffixes[group];
        }
        activeFocusOnTab: exactTotal.length > 0 && value !== exactTotal
        Accessible.role: Accessible.StaticText
        Accessible.name: label + " " + (exactTotal || value) + " " + unit
        HoverHandler { id: hover }
        ToolTip.visible: activeFocusOnTab && (hover.hovered || activeFocus)
        ToolTip.text: label + " · " + exactTotal
        spacing: 8
        SmallLabel {
            text: metricRoot.label
        }
        RowLayout {
            Layout.fillWidth: true
            spacing: 6
            Text {
                Layout.minimumWidth: 0
                Layout.maximumWidth: implicitWidth
                text: metricRoot.value
                color: metricRoot.tint
                font.pixelSize: 27
                fontSizeMode: Text.HorizontalFit
                minimumPixelSize: 13
                elide: Text.ElideRight
                font.weight: Font.Medium
            }
            Text {
                text: metricRoot.unit
                color: "#798596"
                font.pixelSize: 10
                Layout.alignment: Qt.AlignBottom
            }
        }
    }

    // The header is intentionally separate from telemetry content and the audio-style controls.
    Rectangle {
        id: header
        anchors.top: parent.top
        width: parent.width
        height: 88
        color: "#17191e"
        Rectangle {
            width: parent.width
            height: 1
            y: parent.height - 1
            color: "#30343e"
        }
        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 24
            anchors.rightMargin: 25
            spacing: 20
            RowLayout {
                Layout.preferredWidth: 220
                spacing: 12
                Rectangle {
                    width: 32
                    height: 36
                    radius: 6
                    color: "#24392c"
                    border.color: "#476750"
                    Rectangle {
                        x: 8
                        y: 8
                        width: 4
                        height: 20
                        radius: 1
                        color: theme.accent
                    }
                    Rectangle {
                        x: 20
                        y: 8
                        width: 4
                        height: 20
                        radius: 1
                        color: theme.accent
                    }
                    Rectangle {
                        x: 11
                        y: 16
                        width: 10
                        height: 4
                        color: theme.accent
                    }
                }
                ColumnLayout {
                    spacing: 2
                    Text {
                        text: "HAND BRIDGE"
                        color: "#f0f1f5"
                        font.pixelSize: 17
                        font.weight: Font.Bold
                        font.letterSpacing: 1.7
                    }
                    Text {
                        text: "NATIVE TRACKING CONTROL  /  " + Qt.application.version
                        color: "#828b9c"
                        font.pixelSize: 7
                        font.letterSpacing: 1.2
                    }
                }
            }
            Rectangle {
                width: 1
                height: 30
                color: "#333841"
            }
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 5
                SmallLabel {
                    text: "ENGINE SESSION"
                }
                RowLayout {
                    spacing: 7
                    Rectangle {
                        width: 6
                        height: 6
                        radius: 3
                        color: controller.connected ? theme.accent : theme.dim
                    }
                    Text {
                        text: controller.connected ? "Local engine connected" : "Engine disconnected"
                        color: "#c6cdd8"
                        font.pixelSize: 11
                    }
                }
            }
            StatePill {
                text: "SIMULATED SOURCE"
                live: false
                tint: theme.amber
                visible: controller.connected && controller.status.simulated === true
            }
            StatePill {
                objectName: "hardwareProviderBadge"
                text: "LEAPC SOURCE"
                live: false
                visible: controller.connected && controller.status.simulated === false
            }
            StatePill {
                text: controller.dirty ? "EDITING" : controller.revision === controller.savedRevision && controller.connected ? "SAVED" : controller.connected ? "APPLIED · UNSAVED" : "OFFLINE"
                tint: controller.dirty ? theme.amber : theme.accent
                live: controller.connected
            }
            StudioButton {
                objectName: "revertButton"
                text: "Revert"
                enabled: controller.connected && controller.dirty && !controller.busy
                onClicked: controller.revert()
            }
            StudioButton {
                id: applyAction
                objectName: "applyButton"
                text: controller.busy ? "Applying…" : "Apply changes"
                primary: true
                enabled: controller.connected && !controller.busy
                onPressed: forceActiveFocus()
                onClicked: {
                    forceActiveFocus();
                    Qt.callLater(function () {
                        controller.apply();
                    });
                }
            }
        }
    }

    Rectangle {
        id: navigation
        x: 0
        y: header.height
        width: 196
        height: parent.height - header.height
        color: "#15171c"
        Rectangle {
            x: parent.width - 1
            width: 1
            height: parent.height
            color: "#2c3039"
        }
        SmallLabel {
            x: 23
            y: 25
            text: "WORKSPACE"
            color: "#606b7c"
        }
        Column {
            x: 12
            y: 56
            width: parent.width - 24
            spacing: 6
            Repeater {
                model: [
                    {
                        name: "Overview",
                        symbol: "◫",
                        id: "navOverview"
                    },
                    {
                        name: "Devices",
                        symbol: "⌘",
                        id: "navDevices"
                    },
                    {
                        name: "Processing",
                        symbol: "◉",
                        id: "navProcessing"
                    },
                    {
                        name: "OSC Output",
                        symbol: "⇥",
                        id: "navOsc"
                    },
                    {
                        name: "Profiles & Logs",
                        symbol: "≡",
                        id: "navProfiles"
                    }
                ]
                delegate: Rectangle {
                    required property int index
                    required property var modelData
                    objectName: modelData.id
                    activeFocusOnTab: true
                    Keys.onReturnPressed: pageIndex = index
                    Keys.onSpacePressed: pageIndex = index
                    width: parent.width
                    height: 46
                    radius: 7
                    color: pageIndex === index ? "#2a3530" : navMouse.containsMouse ? "#20242c" : "transparent"
                    border.color: pageIndex === index ? "#405448" : "transparent"
                    Rectangle {
                        visible: pageIndex === index
                        x: 0
                        y: 12
                        width: 3
                        height: 22
                        radius: 1
                        color: theme.accent
                    }
                    Text {
                        x: 16
                        anchors.verticalCenter: parent.verticalCenter
                        text: modelData.symbol
                        color: pageIndex === index ? theme.accent : "#7f8b9c"
                        font.pixelSize: 18
                    }
                    Text {
                        x: 45
                        anchors.verticalCenter: parent.verticalCenter
                        text: modelData.name
                        color: pageIndex === index ? "#e4f3e8" : "#9da6b5"
                        font.pixelSize: 11
                        font.weight: pageIndex === index ? Font.DemiBold : Font.Normal
                    }
                    MouseArea {
                        id: navMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            parent.forceActiveFocus();
                            pageIndex = index;
                        }
                    }
                    Accessible.role: Accessible.PageTab
                    Accessible.name: modelData.name
                }
            }
        }
        Rectangle {
            x: 18
            width: parent.width - 36
            height: 150
            anchors.bottom: parent.bottom
            anchors.bottomMargin: 28
            radius: 9
            color: "#1b1e25"
            border.color: "#303641"
            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 14
                spacing: 10
                SmallLabel {
                    text: "INDEPENDENT ENGINE"
                    font.pixelSize: 8
                }
                Text {
                    text: "Close the control window.\nThe signal keeps flowing."
                    color: "#a0aaba"
                    font.pixelSize: 10
                    lineHeight: 1.45
                    Layout.fillWidth: true
                }
                Rectangle {
                    height: 1
                    color: "#313743"
                    Layout.fillWidth: true
                }
                Text {
                    text: "LOCAL IPC ONLY"
                    color: "#738177"
                    font.pixelSize: 8
                    font.letterSpacing: 1.1
                }
            }
        }
    }

    ColumnLayout {
        anchors.left: navigation.right
        anchors.right: parent.right
        anchors.top: header.bottom
        anchors.bottom: footer.top
        anchors.leftMargin: 25
        anchors.rightMargin: 25
        anchors.topMargin: 25
        anchors.bottomMargin: 16
        spacing: 18
        RowLayout {
            Layout.fillWidth: true
            spacing: 15
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 7
                SectionHeading {
                    text: pageNames[pageIndex]
                }
                Text {
                    text: pageSubtitles[pageIndex]
                    color: "#858fa1"
                    font.pixelSize: 11
                }
            }
            StudioButton {
                objectName: "connectEngineButton"
                text: "Reconnect"
                visible: !controller.connected
                enabled: !controller.busy
                onClicked: controller.connectToEngine()
            }
            StudioButton {
                objectName: "startHardwareButton"
                text: "Start engine"
                primary: true
                visible: !controller.connected
                enabled: !controller.busy
                onClicked: controller.startHardwareEngine()
            }
            StudioButton {
                objectName: "startSimulatorButton"
                text: "Start simulator"
                visible: !controller.connected
                enabled: !controller.busy
                onClicked: controller.startSimulator()
            }
            RowLayout {
                visible: controller.connected
                spacing: 7
                SmallLabel {
                    text: "REV"
                    font.pixelSize: 8
                }
                Text {
                    text: String(controller.revision).padStart(3, "0")
                    color: "#b7c1d2"
                    font.family: theme.mono
                    font.pixelSize: 11
                }
            }
        }
        StackLayout {
            id: pages
            currentIndex: pageIndex
            Layout.fillWidth: true
            Layout.fillHeight: true

            // OVERVIEW
            ScrollView {
                id: overviewScroll
                clip: true
                contentWidth: availableWidth
                ColumnLayout {
                    width: overviewScroll.availableWidth
                    spacing: 15
                    Rectangle {
                        Layout.fillWidth: true
                        implicitHeight: 100
                        radius: 11
                        color: "#1b1e24"
                        border.color: "#303641"
                        RowLayout {
                            anchors.fill: parent
                            anchors.margins: 19
                            spacing: 20
                            MetricCell {
                                label: "CAMERAS STREAMING"
                                value: controller.connected ? String(liveDevices()) : "—"
                                unit: "/ " + (deviceRows.length || "0")
                                tint: theme.accent
                                Layout.fillWidth: true
                            }
                            Rectangle {
                                width: 1
                                height: 41
                                color: "#343a45"
                            }
                            MetricCell {
                                label: "PROCESSED FRAMES"
                                total: controller.connected ? framesProcessed() : undefined
                                unit: "total"
                                Layout.fillWidth: true
                            }
                            Rectangle {
                                width: 1
                                height: 41
                                color: "#343a45"
                            }
                            MetricCell {
                                label: "OSC PACKET RATE"
                                value: controller.connected ? metric(output.packet_fps, 0) : "—"
                                unit: "pkt/s"
                                tint: theme.violet
                                Layout.fillWidth: true
                            }
                            Rectangle {
                                width: 1
                                height: 41
                                color: "#343a45"
                            }
                            MetricCell {
                                objectName: "overviewSendErrors"
                                label: "SEND ERRORS"
                                total: controller.connected ? sendErrors : undefined
                                tint: Number(total) > 0 ? theme.amber : "#d9e1ed"
                                Layout.fillWidth: true
                            }
                        }
                    }
                    Rectangle {
                        objectName: "trackingErrorBanner"
                        visible: controller.connected && lastTrackingError.length > 0
                        Layout.fillWidth: true
                        implicitHeight: trackingErrorContent.implicitHeight + 30
                        color: "#30291f"
                        radius: 9
                        border.color: "#67533c"
                        ColumnLayout {
                            id: trackingErrorContent
                            anchors.fill: parent
                            anchors.margins: 15
                            spacing: 7
                            SmallLabel {
                                text: "LAST TRACKING ERROR · RETAINED HISTORY"
                                color: theme.amber
                            }
                            Text {
                                objectName: "trackingErrorText"
                                Layout.fillWidth: true
                                text: lastTrackingError
                                color: "#dfc7a4"
                                font.pixelSize: 11
                                wrapMode: Text.WrapAnywhere
                            }
                            Text {
                                Layout.fillWidth: true
                                text: "Retained after recovery; check the live service and device state below."
                                color: "#a99881"
                                font.pixelSize: 9
                                wrapMode: Text.WordWrap
                            }
                        }
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 15
                        Repeater {
                            model: Math.max(2, Math.min(4, deviceRows.length))
                            delegate: DeviceCard {
                                required property int index
                                Layout.fillWidth: true
                                Layout.preferredWidth: 1
                                device: index < deviceRows.length ? deviceRows[index] : ({})
                                cameraIndex: index
                            }
                        }
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 15
                        Panel {
                            Layout.fillWidth: true
                            Layout.preferredWidth: 720
                            Layout.preferredHeight: 296
                            title: "Live coordinate monitor"
                            subtitle: previewProcessed ? "Hands selected for OSC by each camera’s applied settings" : "All hands observed by each camera"
                            badge: "TOP VIEW · XZ"
                            ColumnLayout {
                                anchors.fill: parent
                                spacing: 7
                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: 6
                                    StudioButton {
                                        objectName: "outputPreviewButton"
                                        text: "Routed hands"
                                        compact: true
                                        primary: previewProcessed
                                        tint: theme.violet
                                        onClicked: previewProcessed = true
                                    }
                                    StudioButton {
                                        objectName: "rawPreviewButton"
                                        text: "Raw camera"
                                        compact: true
                                        primary: !previewProcessed
                                        onClicked: previewProcessed = false
                                    }
                                    Item {
                                        Layout.fillWidth: true
                                    }
                                    SmallLabel {
                                        text: previewProcessed ? "PROCESSED · " + appliedPreviewUnits.toUpperCase() : "RAW MILLIMETERS"
                                        font.pixelSize: 8
                                    }
                                }
                                PreviewCanvas {
                                    objectName: "coordinatePreview"
                                    Layout.fillWidth: true
                                    Layout.fillHeight: true
                                    devices: controller.previewAvailable ? controller.previewDevices : deviceRows
                                    Timer {
                                        interval: 33
                                        repeat: true
                                        triggeredOnStart: true
                                        running: window.visible && window.visibility !== Window.Minimized && window.pageIndex === 0 && controller.connected && controller.previewAvailable
                                        onTriggered: controller.refreshPreview()
                                    }
                                    processed: previewProcessed
                                    units: appliedPreviewUnits
                                    outputMessage: routedOutputMessage
                                    deviceSettings: controller.config.devices || ({})
                                }
                            }
                        }
                        Panel {
                            Layout.preferredWidth: 278
                            Layout.preferredHeight: 296
                            title: "Output stage"
                            badge: output.degraded ? "DEGRADED" : output.enabled ? "ACTIVE" : "MUTED"
                            badgeColor: output.degraded ? theme.amber : theme.accent
                            ColumnLayout {
                                anchors.fill: parent
                                spacing: 12
                                ToggleRow {
                                    objectName: "overviewOutputEnabled"
                                    Layout.fillWidth: true
                                    text: "Transmit OSC"
                                    checked: setting("osc.enabled", true)
                                    enabled: controller.connected && !controller.busy
                                    onToggled: value => edit("osc.enabled", value)
                                }
                                Rectangle {
                                    height: 1
                                    color: "#303641"
                                    Layout.fillWidth: true
                                }
                                SmallLabel {
                                    text: "DESTINATION"
                                }
                                Text {
                                    text: (output.host || setting("osc.host", "127.0.0.1")) + ":" + (output.port || setting("osc.port", 9000))
                                    font.family: theme.mono
                                    color: "#d8e1ef"
                                    font.pixelSize: 16
                                }
                                SmallLabel {
                                    text: "PROCESSING"
                                }
                                Text {
                                    text: appliedBypass ? "Raw pass-through" : "Processed signal"
                                    color: appliedBypass ? "#acb8c7" : theme.violet
                                    font.pixelSize: 12
                                }
                                Item {
                                    Layout.fillHeight: true
                                }
                                Text {
                                    text: (controller.config.channels?.sendFullSkeleton
                                           ? "Full hand skeleton enabled · raw sensor mm"
                                           : "Legacy OSC · scalar float32")
                                          + "\nOS send success ≠ receiver receipt"
                                    color: "#7d889a"
                                    font.pixelSize: 9
                                    lineHeight: 1.5
                                    Layout.fillWidth: true
                                    wrapMode: Text.WordWrap
                                }
                            }
                        }
                    }
                }
            }

            // DEVICES
            ScrollView {
                id: deviceScroll
                clip: true
                contentWidth: availableWidth
                ColumnLayout {
                    width: deviceScroll.availableWidth
                    spacing: 16
                    enabled: controller.connected && !controller.busy
                    Repeater {
                        model: controller.deviceSerials
                        delegate: Panel {
                            id: devicePanel
                            required property string modelData
                            required property int index
                            readonly property string serial: modelData
                            readonly property var device: deviceRows[index] || ({})
                            Layout.fillWidth: true
                            Layout.preferredHeight: 225
                            title: "Camera " + (index + 1) + " / " + (device.alias || serial)
                            subtitle: serial + " · " + (device.reason || device.state || "Waiting")
                            badge: device.frames_fresh ? "FRAMES FRESH" : (device.state || "OFFLINE").toUpperCase()
                            badgeColor: device.frames_fresh ? theme.accent : theme.amber
                            ColumnLayout {
                                anchors.fill: parent
                                spacing: 16
                                RowLayout {
                                    spacing: 16
                                    ColumnLayout {
                                        Layout.preferredWidth: 180
                                        spacing: 7
                                        SmallLabel {
                                            text: "OSC ALIAS"
                                        }
                                        StudioField {
                                            objectName: "alias_" + devicePanel.serial
                                            Layout.fillWidth: true
                                            text: deviceSetting(devicePanel.serial, "alias", devicePanel.serial)
                                            onEditingFinished: edit("devices." + devicePanel.serial + ".alias", text)
                                            Accessible.name: "Camera OSC alias"
                                        }
                                    }
                                    ColumnLayout {
                                        Layout.preferredWidth: 180
                                        spacing: 7
                                        SmallLabel {
                                            text: "LIVE HAND ASSIGNMENT"
                                        }
                                        StudioCombo {
                                            objectName: "handFilter_" + devicePanel.serial
                                            Layout.fillWidth: true
                                            model: ["Both hands", "Left only", "Right only", "Disabled"]
                                            readonly property var filters: ["both", "left", "right", "disabled"]
                                            readonly property int appliedIndex: Math.max(0, filters.indexOf(
                                                controller.config.devices?.[devicePanel.serial]?.hand_filter ?? "both"))
                                            currentIndex: appliedIndex
                                            onActivated: index => {
                                                // ComboBox also moves its index locally. Keep displaying the
                                                // engine's selection until it acknowledges the new filter.
                                                const filter = filters[index]
                                                currentIndex = Qt.binding(() => appliedIndex)
                                                controller.applyHandFilter(devicePanel.serial, filter)
                                            }
                                        }
                                    }
                                    Item {
                                        Layout.fillWidth: true
                                    }
                                    ToggleRow {
                                        Layout.preferredWidth: 160
                                        text: "Device output"
                                        checked: deviceSetting(devicePanel.serial, "enabled", true)
                                        onToggled: value => edit("devices." + devicePanel.serial + ".enabled", value)
                                    }
                                    StudioButton {
                                        text: "Restart stream"
                                        compact: true
                                        enabled: !controller.busy
                                        onClicked: controller.restartDevice(devicePanel.serial)
                                    }
                                }
                                Rectangle {
                                    height: 1
                                    color: "#303641"
                                    Layout.fillWidth: true
                                }
                                RowLayout {
                                    spacing: 25
                                    Repeater {
                                        model: [
                                            {
                                                n: "INPUT",
                                                v: metric(devicePanel.device.input_fps, 1) + " Hz"
                                            },
                                            {
                                                n: "LAST FRAME",
                                                v: metric(devicePanel.device.last_frame_age_ms, 0) + " ms"
                                            },
                                            {
                                                n: "PROCESSED",
                                                v: String(devicePanel.device.frames_processed || 0)
                                            },
                                            {
                                                n: "DROPS",
                                                v: String(devicePanel.device.queue_drops || 0)
                                            },
                                            {
                                                n: "RETRIES",
                                                v: String(devicePanel.device.reconnect_attempts || 0)
                                            }
                                        ]
                                        delegate: ColumnLayout {
                                            required property var modelData
                                            spacing: 5
                                            Layout.fillWidth: true
                                            SmallLabel {
                                                text: modelData.n
                                                font.pixelSize: 8
                                            }
                                            Text {
                                                text: modelData.v
                                                color: "#c3cdda"
                                                font.pixelSize: 13
                                                font.family: theme.mono
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                    Panel {
                        visible: !deviceRows.length
                        Layout.fillWidth: true
                        Layout.preferredHeight: 180
                        title: "No devices discovered"
                        Text {
                            anchors.centerIn: parent
                            text: "Connect to the engine or start the two-camera simulator."
                            color: theme.muted
                            font.pixelSize: 12
                        }
                    }
                    Text {
                        text: "Aliases stay with device serials. Empty hand frames are healthy; no hands is not a device failure."
                        color: "#8390a2"
                        font.pixelSize: 10
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                    }
                }
            }

            // PROCESSING
            ScrollView {
                id: processingScroll
                clip: true
                contentWidth: availableWidth
                ColumnLayout {
                    width: processingScroll.availableWidth
                    spacing: 15
                    enabled: controller.connected && !controller.busy
                    Panel {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 86
                        title: "Signal path"
                        badge: "GLOBAL PROCESSING"
                        RowLayout {
                            anchors.fill: parent
                            spacing: 22
                            ToggleRow {
                                Layout.preferredWidth: 235
                                text: "Raw pass-through"
                                checked: processingBypass
                                accent: theme.violet
                                onToggled: value => edit("processing.bypass", value)
                            }
                            Text {
                                text: "RAW"
                                color: "#8694a6"
                                font.pixelSize: 9
                                font.letterSpacing: 1
                            }
                            Text {
                                text: "→"
                                color: "#5c6676"
                                font.pixelSize: 16
                            }
                            Text {
                                text: "DEADZONE / SMOOTH"
                                color: processingBypass ? "#5e6674" : theme.violet
                                font.pixelSize: 9
                                font.letterSpacing: 0.7
                            }
                            Text {
                                text: "→"
                                color: "#5c6676"
                                font.pixelSize: 16
                            }
                            Text {
                                text: "TRANSFORM / UNITS"
                                color: processingBypass ? "#5e6674" : theme.violet
                                font.pixelSize: 9
                                font.letterSpacing: 0.7
                            }
                            Item {
                                Layout.fillWidth: true
                            }
                            Text {
                                text: "OSC"
                                color: theme.accent
                                font.pixelSize: 9
                                font.letterSpacing: 1
                            }
                        }
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 15
                        Panel {
                            Layout.preferredWidth: 365
                            Layout.preferredHeight: 224
                            title: "Motion shaping"
                            subtitle: "Time-based filters · position channels only"
                            badge: processingBypass ? "BYPASSED" : "ENGAGED"
                            badgeColor: theme.violet
                            RowLayout {
                                anchors.fill: parent
                                enabled: !processingBypass
                                Knob {
                                    objectName: "smoothingDial"
                                    Layout.fillWidth: true
                                    label: "SMOOTHING"
                                    suffix: "ms"
                                    maximum: 2000
                                    value: setting("processing.smoothing_ms", 0)
                                    onValueEdited: value => edit("processing.smoothing_ms", value)
                                }
                                Knob {
                                    objectName: "deadzoneDial"
                                    Layout.fillWidth: true
                                    label: "DEADZONE"
                                    suffix: "mm"
                                    maximum: 500
                                    step: 0.1
                                    decimals: 1
                                    value: setting("processing.deadzone_mm", 0)
                                    onValueEdited: value => edit("processing.deadzone_mm", value)
                                }
                            }
                        }
                        Panel {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 224
                            title: "Coordinate transform"
                            subtitle: "Deadzone → smoothing → scale → XYZ rotation → offset → units"
                            badge: "EXPLICIT UNITS"
                            ColumnLayout {
                                anchors.fill: parent
                                spacing: 12
                                enabled: !processingBypass
                                VectorControl {
                                    Layout.fillWidth: true
                                    label: "SCALE"
                                    values: setting("processing.scale", [1, 1, 1])
                                    onValueEdited: (axis, value) => edit("processing.scale." + axis, value)
                                }
                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: 18
                                    VectorControl {
                                        Layout.fillWidth: true
                                        label: "OFFSET"
                                        suffix: "mm"
                                        values: setting("processing.offset_mm", [0, 0, 0])
                                        onValueEdited: (axis, value) => edit("processing.offset_mm." + axis, value)
                                    }
                                    ColumnLayout {
                                        Layout.preferredWidth: 100
                                        spacing: 10
                                        SmallLabel {
                                            text: "OUTPUT UNITS"
                                        }
                                        StudioCombo {
                                            objectName: "outputUnits"
                                            Layout.fillWidth: true
                                            model: ["mm", "m"]
                                            currentIndex: setting("processing.output_units", "mm") === "m" ? 1 : 0
                                            onActivated: index => edit("processing.output_units", index === 1 ? "m" : "mm")
                                        }
                                    }
                                }
                            }
                        }
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 15
                        Panel {
                            Layout.preferredWidth: 365
                            Layout.preferredHeight: 238
                            title: "Axis rotation"
                            subtitle: "Degrees · right-handed XYZ rotations"
                            ColumnLayout {
                                anchors.fill: parent
                                spacing: 16
                                VectorControl {
                                    Layout.fillWidth: true
                                    enabled: !processingBypass
                                    label: "ROTATION"
                                    suffix: "°"
                                    decimals: 1
                                    values: setting("processing.rotation_deg", [0, 0, 0])
                                    onValueEdited: (axis, value) => edit("processing.rotation_deg." + axis, value)
                                }
                                Text {
                                    text: "Applied after scale and before offset.
Raw observations remain unchanged."
                                    color: "#7e899b"
                                    font.pixelSize: 10
                                    lineHeight: 1.5
                                    Layout.fillWidth: true
                                    wrapMode: Text.WordWrap
                                }
                                Item {
                                    Layout.fillHeight: true
                                }
                            }
                        }
                        Panel {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 238
                            title: "Gesture gates"
                            subtitle: "Opt-in pinchActive / grabActive flags · native strengths unchanged"
                            ToggleRow {
                                objectName: "gesturesEnabled"
                                x: parent.width - width
                                y: -55
                                width: 130
                                height: 28
                                text: "Enabled"
                                checked: setting("processing.gestures_enabled", false)
                                accent: theme.violet
                                onToggled: value => edit("processing.gestures_enabled", value)
                            }
                            RowLayout {
                                anchors.fill: parent
                                spacing: 8
                                Repeater {
                                    model: [
                                        {
                                            label: "PINCH ENTER",
                                            key: "pinch_enter",
                                            value: 0.8
                                        },
                                        {
                                            label: "PINCH RELEASE",
                                            key: "pinch_exit",
                                            value: 0.6
                                        },
                                        {
                                            label: "GRAB ENTER",
                                            key: "grab_enter",
                                            value: 0.8
                                        },
                                        {
                                            label: "GRAB RELEASE",
                                            key: "grab_exit",
                                            value: 0.6
                                        }
                                    ]
                                    delegate: Knob {
                                        required property var modelData
                                        Layout.fillWidth: true
                                        enabled: setting("processing.gestures_enabled", false)
                                        label: modelData.label
                                        maximum: 1
                                        step: 0.01
                                        decimals: 2
                                        value: setting("processing." + modelData.key, modelData.value)
                                        onValueEdited: value => edit("processing." + modelData.key, value)
                                    }
                                }
                            }
                        }
                    }
                }
            }

            // OSC OUTPUT
            ScrollView {
                id: oscScroll
                clip: true
                contentWidth: availableWidth
                ColumnLayout {
                    width: oscScroll.availableWidth
                    spacing: 15
                    enabled: controller.connected && !controller.busy
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 15
                        Panel {
                            Layout.preferredWidth: 365
                            Layout.preferredHeight: 340
                            title: "OSC destination"
                            subtitle: "UDP output · same-PC TouchDesigner workflow"
                            badge: "OSC / UDP"
                            ColumnLayout {
                                anchors.fill: parent
                                spacing: 14
                                ToggleRow {
                                    objectName: "outputEnabled"
                                    Layout.fillWidth: true
                                    text: "Enable transmission"
                                    checked: setting("osc.enabled", true)
                                    onToggled: value => edit("osc.enabled", value)
                                }
                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: 11
                                    ColumnLayout {
                                        Layout.fillWidth: true
                                        spacing: 7
                                        SmallLabel {
                                            text: "IPV4 ADDRESS"
                                        }
                                        StudioField {
                                            objectName: "oscHost"
                                            Layout.fillWidth: true
                                            text: setting("osc.host", "127.0.0.1")
                                            onEditingFinished: edit("osc.host", text)
                                            Accessible.name: "OSC destination IPv4 address"
                                        }
                                    }
                                    ColumnLayout {
                                        Layout.preferredWidth: 90
                                        spacing: 7
                                        SmallLabel {
                                            text: "UDP PORT"
                                        }
                                        StudioField {
                                            objectName: "oscPort"
                                            Layout.fillWidth: true
                                            text: String(setting("osc.port", 9000))
                                            validator: IntValidator {
                                                locale: "C"
                                                bottom: 1
                                                top: 65535
                                            }
                                            onEditingFinished: if (acceptableInput)
                                                edit("osc.port", Number.fromLocaleString(Qt.locale("C"), text))
                                            Accessible.name: "OSC destination UDP port"
                                        }
                                    }
                                }
                                Rectangle {
                                    Layout.fillWidth: true
                                    height: 1
                                    color: "#303641"
                                }
                                RowLayout {
                                    Layout.fillWidth: true
                                    Text {
                                        text: "Socket"
                                        color: "#8793a6"
                                        font.pixelSize: 11
                                    }
                                    Item {
                                        Layout.fillWidth: true
                                    }
                                    StatePill {
                                        text: output.socket_ready ? "READY" : "UNAVAILABLE"
                                        live: !!output.socket_ready
                                    }
                                }
                                StudioButton {
                                    text: "Reset output socket"
                                    compact: true
                                    enabled: !controller.busy
                                    onClicked: controller.resetOutput()
                                }
                                Item {
                                    Layout.fillHeight: true
                                }
                                Text {
                                    text: "Apply to retarget the engine.\nSend success does not confirm receiver receipt."
                                    color: "#7e899b"
                                    font.pixelSize: 9
                                    lineHeight: 1.45
                                    Layout.fillWidth: true
                                    wrapMode: Text.WordWrap
                                }
                            }
                        }
                        Panel {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 340
                            title: "Channel routing"
                            subtitle: "Full hand skeleton sends one raw pose per hand in millimetres"
                            GridLayout {
                                anchors.fill: parent
                                columns: 2
                                columnSpacing: 26
                                rowSpacing: 0
                                Repeater {
                                    model: [
                                        {
                                            n: "Palm position",
                                            k: "sendPalm",
                                            d: true
                                        },
                                        {
                                            n: "Wrist position",
                                            k: "sendWrist",
                                            d: true
                                        },
                                        {
                                            n: "Thumb tip",
                                            k: "sendThumb",
                                            d: true
                                        },
                                        {
                                            n: "Index tip",
                                            k: "sendIndex",
                                            d: true
                                        },
                                        {
                                            n: "Middle tip",
                                            k: "sendMiddle",
                                            d: true
                                        },
                                        {
                                            n: "Ring tip",
                                            k: "sendRing",
                                            d: true
                                        },
                                        {
                                            n: "Pinky tip",
                                            k: "sendPinky",
                                            d: true
                                        },
                                        {
                                            n: "Palm orientation",
                                            k: "sendPalmOrientation",
                                            d: false
                                        },
                                        {
                                            n: "Palm velocity",
                                            k: "sendPalmVelocity",
                                            d: false
                                        },
                                        {
                                            n: "Palm normal",
                                            k: "sendPalmNormal",
                                            d: false
                                        },
                                        {
                                            n: "Visible time",
                                            k: "sendVisibleTime",
                                            d: false
                                        },
                                        {
                                            n: "Finger extension",
                                            k: "sendFingerIsExtended",
                                            d: false
                                        },
                                        {
                                            n: "Pinch strength",
                                            k: "sendPinchStrength",
                                            d: true
                                        },
                                        {
                                            n: "Grab strength",
                                            k: "sendGrabStrength",
                                            d: true
                                        },
                                        {
                                            n: "Full hand skeleton",
                                            k: "sendFullSkeleton",
                                            d: false
                                        }
                                    ]
                                    delegate: ToggleRow {
                                        required property var modelData
                                        objectName: "channel_" + modelData.k
                                        Layout.fillWidth: true
                                        Layout.preferredHeight: 31
                                        implicitWidth: 180
                                        text: modelData.n
                                        checked: setting("channels." + modelData.k, modelData.d)
                                        onToggled: value => edit("channels." + modelData.k, value)
                                    }
                                }
                            }
                        }
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 15
                        Panel {
                            Layout.preferredWidth: 365
                            Layout.preferredHeight: 251
                            title: "Output cadence"
                            subtitle: "0 follows accepted source frames"
                            badge: "PER DEVICE"
                            RowLayout {
                                anchors.fill: parent
                                spacing: 15
                                Knob {
                                    objectName: "outputRateDial"
                                    Layout.preferredWidth: 140
                                    label: "SEND RATE"
                                    suffix: "Hz"
                                    maximum: 240
                                    value: setting("osc.rate_hz", 0)
                                    accent: theme.accent
                                    onValueEdited: value => edit("osc.rate_hz", value)
                                }
                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 13
                                    SmallLabel {
                                        text: "ACTUAL PACKETS"
                                    }
                                    Text {
                                        text: metric(output.packet_fps, 0)
                                        color: theme.accent
                                        font.pixelSize: 29
                                    }
                                    Text {
                                        text: "packets / second"
                                        color: "#8490a3"
                                        font.pixelSize: 9
                                        lineHeight: 1.4
                                    }
                                }
                            }
                        }
                        Panel {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 251
                            title: "Transport monitor"
                            subtitle: "OS acceptance only · receiver confirmation unavailable"
                            badge: output.degraded ? "DEGRADED" : "OBSERVED"
                            badgeColor: output.degraded ? theme.amber : theme.muted
                            ColumnLayout {
                                anchors.fill: parent
                                spacing: 17
                                RowLayout {
                                    Layout.fillWidth: true
                                    MetricCell {
                                        objectName: "transportPacketsSent"
                                        Layout.fillWidth: true
                                        label: "PACKETS SENT"
                                        total: output.packets_sent_exact ?? output.packets_sent
                                        unit: "total"
                                        tint: theme.accent
                                    }
                                    MetricCell {
                                        objectName: "transportSendErrors"
                                        Layout.fillWidth: true
                                        label: "SEND ERRORS"
                                        total: sendErrors
                                        unit: "total"
                                        tint: Number(total) > 0 ? theme.amber : theme.text
                                    }
                                    MetricCell {
                                        Layout.fillWidth: true
                                        label: "PAYLOAD"
                                        value: output.bytes_sent === undefined ? "—" : (Number(output.bytes_sent) / 1024).toFixed(1)
                                        unit: "KiB"
                                    }
                                }
                                Rectangle {
                                    Layout.fillWidth: true
                                    height: 1
                                    color: "#303641"
                                }
                                SmallLabel {
                                    text: "ADDRESS FORMAT EXAMPLE"
                                }
                                Text {
                                    text: "/leap/{alias}/{left|right}/palm/tx   ,f"
                                    color: "#a9b5c8"
                                    font.family: theme.mono
                                    font.pixelSize: 11
                                    Layout.fillWidth: true
                                    elide: Text.ElideRight
                                }
                                Text {
                                    text: "Legacy address preview. Inspect actual receipt in TouchDesigner OSC In DAT/CHOP."
                                    color: "#7d899b"
                                    font.pixelSize: 9
                                    Layout.fillWidth: true
                                    wrapMode: Text.WordWrap
                                }
                            }
                        }
                    }
                }
            }

            // PROFILES & DIAGNOSTICS
            ScrollView {
                id: profilesScroll
                clip: true
                contentWidth: availableWidth
                ColumnLayout {
                    width: profilesScroll.availableWidth
                    spacing: 15
                    Panel {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 185
                        title: "Session profiles"
                        subtitle: "Atomic saves, restored by the engine · schema version 2"
                        badge: controller.revision === controller.savedRevision && controller.connected ? "SAVED" : "UNSAVED"
                        badgeColor: theme.violet
                        ColumnLayout {
                            anchors.fill: parent
                            spacing: 12
                            RowLayout {
                                Layout.fillWidth: true
                                spacing: 12
                                StudioCombo {
                                    id: selectedProfile
                                    objectName: "profilePicker"
                                    Layout.preferredWidth: 220
                                    model: controller.profileNames
                                    enabled: controller.connected && controller.profileNames.length > 0 && !controller.busy
                                    displayText: controller.profileNames.length ? currentText : "No saved profiles"
                                }
                                StudioButton {
                                    objectName: "loadProfileButton"
                                    text: "Load profile"
                                    enabled: controller.connected && controller.profileNames.length > 0 && !controller.busy
                                    onClicked: controller.loadProfile(selectedProfile.currentText)
                                }
                                Rectangle {
                                    width: 1
                                    height: 30
                                    color: "#343a45"
                                }
                                StudioField {
                                    id: profileName
                                    objectName: "profileName"
                                    Layout.fillWidth: true
                                    placeholderText: "New profile name"
                                    maximumLength: 64
                                    enabled: controller.connected && !controller.busy
                                }
                                StudioButton {
                                    objectName: "saveProfileButton"
                                    text: "Save applied session"
                                    primary: true
                                    tint: theme.violet
                                    enabled: controller.connected && !controller.dirty && profileName.text.trim().length > 0 && !controller.busy
                                    onClicked: controller.saveProfile(profileName.text.trim())
                                }
                            }
                            Text {
                                text: controller.dirty ? "Apply your edits before saving a profile. The engine's applied state is the saved source of truth." : "Profiles include routing, devices, transforms and channels. A backup protects the previous save."
                                color: controller.dirty ? theme.amber : "#8793a6"
                                font.pixelSize: 10
                                Layout.fillWidth: true
                                wrapMode: Text.WordWrap
                            }
                        }
                    }
                    Panel {
                        objectName: "trackingProviderDiagnostics"
                        visible: controller.connected && controller.status.simulated === false
                        Layout.fillWidth: true
                        Layout.preferredHeight: providerDetails.implicitHeight + 85
                        title: "LeapC provider diagnostics"
                        subtitle: Qt.platform.os === "osx" ? "Installed Hyperion runtime and live service status" : "Loaded library metadata; physical tracking remains unverified"
                        badge: "NATIVE PROVIDER"
                        ColumnLayout {
                            id: providerDetails
                            anchors.fill: parent
                            spacing: 12
                            RowLayout {
                                Layout.fillWidth: true
                                spacing: 30
                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 5
                                    SmallLabel {
                                        text: "LIBRARY VERSION"
                                    }
                                    Text {
                                        objectName: "trackingLibraryVersion"
                                        text: tracking.library_version || "Not reported"
                                        color: "#bcc9db"
                                        font.family: theme.mono
                                        font.pixelSize: 12
                                    }
                                }
                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 5
                                    SmallLabel {
                                        text: "TRACKING SERVICE"
                                    }
                                    Text {
                                        objectName: "trackingServiceVersion"
                                        text: tracking.service_version || "Not connected / unverified"
                                        color: "#bcc9db"
                                        font.family: theme.mono
                                        font.pixelSize: 12
                                    }
                                }
                                ColumnLayout {
                                    spacing: 5
                                    SmallLabel {
                                        text: "CONNECT ATTEMPTS"
                                    }
                                    Text {
                                        text: metric(tracking.connection_attempts, 0)
                                        color: "#bcc9db"
                                        font.family: theme.mono
                                        font.pixelSize: 12
                                    }
                                }
                                ColumnLayout {
                                    spacing: 5
                                    SmallLabel {
                                        text: "POLL ERRORS"
                                    }
                                    Text {
                                        text: metric(tracking.poll_errors, 0)
                                        color: tracking.poll_errors > 0 ? theme.amber : "#bcc9db"
                                        font.family: theme.mono
                                        font.pixelSize: 12
                                    }
                                }
                            }
                            SmallLabel {
                                text: "LIBRARY PATH"
                            }
                            Text {
                                Layout.fillWidth: true
                                text: tracking.library_path || tracking.path || "Not reported"
                                color: "#8f9eb3"
                                font.family: theme.mono
                                font.pixelSize: 10
                                wrapMode: Text.WrapAnywhere
                            }
                            SmallLabel {
                                text: "LAST TRACKING ERROR · RETAINED HISTORY"
                                color: lastTrackingError.length ? theme.amber : "#818b9d"
                            }
                            Text {
                                objectName: "trackingDiagnosticError"
                                Layout.fillWidth: true
                                text: lastTrackingError || "No tracking error reported."
                                color: lastTrackingError.length ? "#d5bd9c" : "#8f9eb3"
                                font.pixelSize: 11
                                wrapMode: Text.WrapAnywhere
                            }
                        }
                    }
                    Panel {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 340
                        title: "Engine event log"
                        subtitle: "Bounded lifecycle and output diagnostics · source timestamps retained"
                        badge: "READ ONLY"
                        ColumnLayout {
                            anchors.fill: parent
                            spacing: 12
                            RowLayout {
                                Layout.fillWidth: true
                                SmallLabel {
                                    text: "LATEST " + controller.logEntries.length + " EVENTS"
                                    font.pixelSize: 8
                                }
                                Item {
                                    Layout.fillWidth: true
                                }
                                StudioButton {
                                    objectName: "exportDiagnosticsButton"
                                    text: "Export diagnostics"
                                    compact: true
                                    enabled: controller.connected && !controller.busy
                                    onClicked: controller.exportDiagnostics()
                                }
                            }
                            Rectangle {
                                Layout.fillWidth: true
                                Layout.fillHeight: true
                                color: "#13161b"
                                radius: 5
                                border.color: "#2f3540"
                                ListView {
                                    id: logView
                                    anchors.fill: parent
                                    anchors.margins: 12
                                    clip: true
                                    spacing: 5
                                    model: controller.logEntries
                                    delegate: Text {
                                        required property string modelData
                                        width: logView.width
                                        text: modelData
                                        color: modelData.indexOf("failed") >= 0 || modelData.indexOf("stale") >= 0 ? theme.amber : "#9cabbf"
                                        font.family: theme.mono
                                        font.pixelSize: 10
                                        wrapMode: Text.WrapAnywhere
                                    }
                                    ScrollBar.vertical: ScrollBar {}
                                    onCountChanged: positionViewAtEnd()
                                }
                                Text {
                                    visible: controller.logEntries.length === 0
                                    anchors.centerIn: parent
                                    text: controller.connected ? "No lifecycle events recorded yet." : "Connect to the engine to inspect its log."
                                    color: "#647183"
                                    font.pixelSize: 11
                                }
                            }
                        }
                    }
                    Text {
                        text: Qt.platform.os === "osx" ? "For two cameras, connect the first, wait for tracking, then connect the second." : !controller.connected ? "UNTESTED ON HARDWARE · Connect to a local engine, or start the simulator without cameras." : controller.status.simulated === true ? "UNTESTED ON HARDWARE · The current source is simulated. Real Hyperion, USB recovery and TouchDesigner receipt need local Windows verification." : "UNTESTED ON HARDWARE · LeapC provider selected. Verify physical tracking, USB recovery and TouchDesigner receipt on this Windows PC."
                        color: "#8d96a5"
                        font.pixelSize: 10
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                    }
                }
            }
        }
    }

    Rectangle {
        id: footer
        x: navigation.width
        anchors.bottom: parent.bottom
        width: parent.width - navigation.width
        height: 43
        color: "#16181e"
        Rectangle {
            width: parent.width
            height: 1
            color: "#303641"
        }
        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 25
            anchors.rightMargin: 25
            spacing: 12
            Rectangle {
                width: 5
                height: 5
                radius: 3
                color: controller.errorMessage.length ? theme.red : controller.connected ? theme.accent : theme.dim
            }
            Text {
                text: controller.errorMessage || controller.notice || controller.statusText || "Waiting for a local engine"
                color: controller.errorMessage.length ? theme.red : "#95a0b2"
                font.pixelSize: 10
                Layout.fillWidth: true
                elide: Text.ElideRight
            }
            Text {
                visible: Qt.platform.os !== "osx"
                text: "UNTESTED ON HARDWARE"
                color: "#7e899a"
                font.pixelSize: 8
                font.letterSpacing: 1
            }
            Rectangle {
                width: 1
                height: 13
                color: "#343a45"
            }
            Text {
                text: "OSC / UDP"
                color: "#8897a9"
                font.pixelSize: 8
                font.letterSpacing: 0.9
            }
        }
    }
}
