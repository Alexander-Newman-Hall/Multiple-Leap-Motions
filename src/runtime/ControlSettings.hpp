#pragma once
#include "core/ConfigManager.h"
#include <array>
#include <map>
#include <string>

namespace handbridge {
struct OscSettings {
    bool enabled = true;
    std::string host = "127.0.0.1";
    int port = 9000;
    double rateHz = 0; // Zero preserves source frame rate.
};
struct ChannelSettings {
    bool sendPalm = true, sendWrist = true, sendThumb = true, sendIndex = true;
    bool sendMiddle = true, sendRing = true, sendPinky = true;
    bool sendPalmOrientation = false, sendPalmVelocity = false, sendPalmNormal = false;
    bool sendVisibleTime = false, sendFingerIsExtended = false;
    bool sendPinchStrength = true, sendGrabStrength = true;
};
struct ProcessingSettings {
    bool bypass = true;
    double smoothingMs = 0, deadzoneMm = 0;
    std::array<double, 3> scale{1, 1, 1}, offsetMm{0, 0, 0}, rotationDeg{0, 0, 0};
    std::string outputUnits = "mm";
    bool gesturesEnabled = false;
    double pinchEnter = .8, pinchExit = .6, grabEnter = .8, grabExit = .6;
};
struct DeviceSettings {
    std::string alias;
    bool enabled = true; // Output selection; tracking health remains observable.
    std::string handFilter = "both";
};
struct ControlSettings {
    OscSettings osc;
    ChannelSettings channels;
    ProcessingSettings processing;
    std::map<std::string, DeviceSettings> devices;
    nlohmann::json toJson() const;
    static ControlSettings fromJson(const nlohmann::json& json);
    static ControlSettings fromLegacy(const ConfigManager& config);
    void validate() const;
};
}
