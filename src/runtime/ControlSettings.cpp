#include "ControlSettings.hpp"
#include <algorithm>
#include <charconv>
#include <cctype>
#include <cmath>
#include <limits>
#include <set>
#include <stdexcept>
#include <string_view>

namespace handbridge {
namespace {
using Json = nlohmann::json;
void object(const Json& input, std::initializer_list<std::string_view> keys, const std::string& name) {
    if (!input.is_object()) throw std::invalid_argument(name + " must be an object");
    for (const auto& [key, value] : input.items()) {
        (void)value;
        if (std::find(keys.begin(), keys.end(), key) == keys.end())
            throw std::invalid_argument("unknown " + name + " field: " + key);
    }
}
bool boolean(const Json& input, const char* key, bool fallback) {
    if (!input.contains(key)) return fallback;
    if (!input.at(key).is_boolean()) throw std::invalid_argument(std::string(key) + " must be boolean");
    return input.at(key).get<bool>();
}
double number(const Json& input, const char* key, double fallback) {
    if (!input.contains(key)) return fallback;
    if (!input.at(key).is_number()) throw std::invalid_argument(std::string(key) + " must be numeric");
    return input.at(key).get<double>();
}
std::string string(const Json& input, const char* key, const std::string& fallback) {
    if (!input.contains(key)) return fallback;
    if (!input.at(key).is_string()) throw std::invalid_argument(std::string(key) + " must be text");
    return input.at(key).get<std::string>();
}
std::array<double, 3> vector(const Json& input, const char* key, std::array<double, 3> fallback) {
    if (!input.contains(key)) return fallback;
    const auto& value = input.at(key);
    if (!value.is_array() || value.size() != 3) throw std::invalid_argument(std::string(key) + " must contain three numbers");
    for (std::size_t axis = 0; axis < 3; ++axis) {
        if (!value[axis].is_number()) throw std::invalid_argument(std::string(key) + " must contain three numbers");
        fallback[axis] = value[axis].get<double>();
    }
    return fallback;
}
void range(double value, double low, double high, const char* name) {
    if (!std::isfinite(value) || value < low || value > high)
        throw std::invalid_argument(std::string(name) + " is outside its supported range");
}
bool ipv4(const std::string& host) {
    std::size_t begin = 0;
    for (int axis = 0; axis < 4; ++axis) {
        const auto end = host.find('.', begin);
        if ((axis == 3) != (end == std::string::npos)) return false;
        const auto part = std::string_view(host).substr(begin, end == std::string::npos ? host.size() - begin : end - begin);
        unsigned value = 0;
        const auto [last, error] = std::from_chars(part.data(), part.data() + part.size(), value);
        if (part.empty() || error != std::errc{} || last != part.data() + part.size() || value > 255) return false;
        begin = end + 1;
    }
    return true;
}
std::string handFilter(std::string hand) {
    std::transform(hand.begin(), hand.end(), hand.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    if (hand.empty() || hand == "none") return "both";
    return hand;
}
#define CHANNEL_MEMBERS(X) \
    X(sendPalm) X(sendWrist) X(sendThumb) X(sendIndex) X(sendMiddle) X(sendRing) X(sendPinky) \
    X(sendPalmOrientation) X(sendPalmVelocity) X(sendPalmNormal) X(sendVisibleTime) \
    X(sendFingerIsExtended) X(sendPinchStrength) X(sendGrabStrength)
}
void ControlSettings::validate() const {
    if (!ipv4(osc.host)) throw std::invalid_argument("osc.host must be a numeric IPv4 address");
    range(osc.port, 1, 65535, "osc.port");
    range(osc.rateHz, 0, 240, "osc.rate_hz");
    if (osc.rateHz > 0 && osc.rateHz < 1) throw std::invalid_argument("osc.rate_hz must be zero or 1..240 Hz");
    range(processing.smoothingMs, 0, 2000, "smoothing_ms");
    range(processing.deadzoneMm, 0, 500, "deadzone_mm");
    for (const auto value : processing.scale) range(value, -100, 100, "scale");
    for (const auto value : processing.offsetMm) range(value, -10000, 10000, "offset_mm");
    for (const auto value : processing.rotationDeg) range(value, -360, 360, "rotation_deg");
    if (processing.outputUnits != "mm" && processing.outputUnits != "m")
        throw std::invalid_argument("output_units must be mm or m");
    for (const auto value : {processing.pinchEnter, processing.pinchExit, processing.grabEnter, processing.grabExit})
        range(value, 0, 1, "gesture threshold");
    if (processing.pinchExit > processing.pinchEnter || processing.grabExit > processing.grabEnter)
        throw std::invalid_argument("gesture exit threshold must not exceed enter threshold");
    if (devices.size() > 64) throw std::invalid_argument("at most 64 devices are supported");
    std::set<std::string> aliases;
    for (const auto& [serial, device] : devices) {
        if (serial.empty() || serial.size() > 256 || std::any_of(serial.begin(), serial.end(), [](unsigned char c) { return c < 32; }))
            throw std::invalid_argument("invalid device serial");
        if (device.alias.size() < 4 || device.alias.size() > 32 || device.alias.substr(0, 3) != "dev" ||
            !std::all_of(device.alias.begin() + 3, device.alias.end(), [](unsigned char c) { return c >= '0' && c <= '9'; }))
            throw std::invalid_argument("device alias must be dev followed by decimal digits");
        std::uint64_t index = 0;
        const auto part = std::string_view(device.alias).substr(3);
        const auto [end, error] = std::from_chars(part.data(), part.data() + part.size(), index);
        if (error != std::errc{} || end != part.data() + part.size() || index == std::numeric_limits<std::uint64_t>::max())
            throw std::invalid_argument("device alias index is exhausted");
        if (!aliases.insert(device.alias).second) throw std::invalid_argument("duplicate device alias");
        if (device.handFilter != "left" && device.handFilter != "right" && device.handFilter != "both" && device.handFilter != "disabled")
            throw std::invalid_argument("hand_filter must be left, right, both or disabled");
    }
}
nlohmann::json ControlSettings::toJson() const {
    Json channelJson = Json::object();
#define WRITE_CHANNEL(member) channelJson[#member] = channels.member;
    CHANNEL_MEMBERS(WRITE_CHANNEL)
#undef WRITE_CHANNEL
    Json deviceJson = Json::object();
    for (const auto& [serial, device] : devices)
        deviceJson[serial] = {{"alias", device.alias}, {"enabled", device.enabled}, {"hand_filter", device.handFilter}};
    return {{"schema_version", 2},
        {"osc", {{"enabled", osc.enabled}, {"host", osc.host}, {"port", osc.port}, {"rate_hz", osc.rateHz}}},
        {"channels", channelJson},
        {"processing", {{"bypass", processing.bypass}, {"smoothing_ms", processing.smoothingMs},
            {"deadzone_mm", processing.deadzoneMm}, {"scale", processing.scale}, {"offset_mm", processing.offsetMm},
            {"rotation_deg", processing.rotationDeg}, {"output_units", processing.outputUnits},
            {"gestures_enabled", processing.gesturesEnabled}, {"pinch_enter", processing.pinchEnter},
            {"pinch_exit", processing.pinchExit}, {"grab_enter", processing.grabEnter}, {"grab_exit", processing.grabExit}}},
        {"devices", deviceJson}};
}
ControlSettings ControlSettings::fromJson(const Json& input) {
    object(input, {"schema_version", "osc", "channels", "processing", "devices"}, "configuration");
    if (!input.contains("schema_version") || !input.at("schema_version").is_number_integer() || input.at("schema_version") != 2)
        throw std::invalid_argument("unsupported configuration schema_version (expected 2)");
    ControlSettings result;
    if (input.contains("osc")) {
        const auto& osc = input.at("osc");
        object(osc, {"enabled", "host", "port", "rate_hz"}, "osc");
        result.osc.enabled = boolean(osc, "enabled", result.osc.enabled);
        result.osc.host = string(osc, "host", result.osc.host);
        if (osc.contains("port")) {
            if (!osc.at("port").is_number_integer() || osc.at("port") < 1 || osc.at("port") > 65535)
                throw std::invalid_argument("osc.port must be an integer 1..65535");
            result.osc.port = osc.at("port").get<int>();
        }
        result.osc.rateHz = number(osc, "rate_hz", result.osc.rateHz);
    }
    if (input.contains("channels")) {
        const auto& channels = input.at("channels");
        object(channels, {"sendPalm", "sendWrist", "sendThumb", "sendIndex", "sendMiddle", "sendRing", "sendPinky",
            "sendPalmOrientation", "sendPalmVelocity", "sendPalmNormal", "sendVisibleTime", "sendFingerIsExtended",
            "sendPinchStrength", "sendGrabStrength"}, "channels");
#define READ_CHANNEL(member) result.channels.member = boolean(channels, #member, result.channels.member);
        CHANNEL_MEMBERS(READ_CHANNEL)
#undef READ_CHANNEL
    }
    if (input.contains("processing")) {
        const auto& p = input.at("processing");
        object(p, {"bypass", "smoothing_ms", "deadzone_mm", "scale", "offset_mm", "rotation_deg", "output_units",
            "gestures_enabled", "pinch_enter", "pinch_exit", "grab_enter", "grab_exit"}, "processing");
        result.processing.bypass = boolean(p, "bypass", result.processing.bypass);
        result.processing.smoothingMs = number(p, "smoothing_ms", result.processing.smoothingMs);
        result.processing.deadzoneMm = number(p, "deadzone_mm", result.processing.deadzoneMm);
        result.processing.scale = vector(p, "scale", result.processing.scale);
        result.processing.offsetMm = vector(p, "offset_mm", result.processing.offsetMm);
        result.processing.rotationDeg = vector(p, "rotation_deg", result.processing.rotationDeg);
        result.processing.outputUnits = string(p, "output_units", result.processing.outputUnits);
        result.processing.gesturesEnabled = boolean(p, "gestures_enabled", result.processing.gesturesEnabled);
        result.processing.pinchEnter = number(p, "pinch_enter", result.processing.pinchEnter);
        result.processing.pinchExit = number(p, "pinch_exit", result.processing.pinchExit);
        result.processing.grabEnter = number(p, "grab_enter", result.processing.grabEnter);
        result.processing.grabExit = number(p, "grab_exit", result.processing.grabExit);
    }
    if (input.contains("devices")) {
        const auto& devices = input.at("devices");
        if (!devices.is_object()) throw std::invalid_argument("devices must be an object");
        for (const auto& [serial, device] : devices.items()) {
            object(device, {"alias", "enabled", "hand_filter"}, "device");
            result.devices[serial] = {string(device, "alias", ""), boolean(device, "enabled", true), string(device, "hand_filter", "both")};
        }
    }
    result.validate();
    return result;
}
ControlSettings ControlSettings::fromLegacy(const ConfigManager& config) {
    ControlSettings result;
    result.osc.host = config.getOscIp(); result.osc.port = config.getOscPort();
    // Legacy getter names capitalize the first letter after is.
    result.channels.sendPalm = config.isSendPalmEnabled(); result.channels.sendWrist = config.isSendWristEnabled();
    result.channels.sendThumb = config.isSendThumbEnabled(); result.channels.sendIndex = config.isSendIndexEnabled();
    result.channels.sendMiddle = config.isSendMiddleEnabled(); result.channels.sendRing = config.isSendRingEnabled();
    result.channels.sendPinky = config.isSendPinkyEnabled(); result.channels.sendPalmOrientation = config.isSendPalmOrientationEnabled();
    result.channels.sendPalmVelocity = config.isSendPalmVelocityEnabled(); result.channels.sendPalmNormal = config.isSendPalmNormalEnabled();
    result.channels.sendVisibleTime = config.isSendVisibleTimeEnabled(); result.channels.sendFingerIsExtended = config.isSendFingerIsExtendedEnabled();
    result.channels.sendPinchStrength = config.isSendPinchStrengthEnabled(); result.channels.sendGrabStrength = config.isSendGrabStrengthEnabled();
    const auto aliases = config.getDeviceAliasManager().toJson().at("deviceAliases").get<std::map<std::string, std::string>>();
    std::set<std::string> used;
    for (const auto& [serial, alias] : aliases) { result.devices[serial] = {alias, true, "both"}; used.insert(alias); }
    unsigned next = 1;
    for (const auto& [serial, assignment] : config.getAllDefaultHandAssignments()) {
        if (!result.devices.contains(serial)) {
            std::string alias;
            do { alias = "dev" + std::to_string(next++); } while (used.contains(alias));
            used.insert(alias); result.devices[serial].alias = alias;
        }
        result.devices[serial].handFilter = handFilter(assignment);
    }
    result.validate();
    return result;
}
#undef CHANNEL_MEMBERS
}
