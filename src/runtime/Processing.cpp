#include "Processing.hpp"
#include <algorithm>
#include <cmath>
#include <numbers>

namespace handbridge {
namespace {
Vector3 add(Vector3 a, Vector3 b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
Vector3 multiply(Vector3 v, double scalar) { return {float(v.x * scalar), float(v.y * scalar), float(v.z * scalar)}; }
Vector3 scale(Vector3 v, const std::array<double, 3>& factors) {
    return {float(v.x * factors[0]), float(v.y * factors[1]), float(v.z * factors[2])};
}
Quaternion product(Quaternion a, Quaternion b) {
    return {a.w*b.w-a.x*b.x-a.y*b.y-a.z*b.z,
            a.w*b.x+a.x*b.w+a.y*b.z-a.z*b.y,
            a.w*b.y-a.x*b.z+a.y*b.w+a.z*b.x,
            a.w*b.z+a.x*b.y-a.y*b.x+a.z*b.w};
}
Quaternion rotation(const std::array<double, 3>& degrees) {
    const auto factor = std::numbers::pi / 360;
    Quaternion x{float(std::cos(degrees[0]*factor)),float(std::sin(degrees[0]*factor)),0,0};
    Quaternion y{float(std::cos(degrees[1]*factor)),0,float(std::sin(degrees[1]*factor)),0};
    Quaternion z{float(std::cos(degrees[2]*factor)),0,0,float(std::sin(degrees[2]*factor))};
    return product(z, product(y, x)); // Apply X, then Y, then Z.
}
Vector3 rotate(Vector3 v, Quaternion q) {
    const auto value = product(product(q, {0,v.x,v.y,v.z}), {q.w,-q.x,-q.y,-q.z});
    return {value.x,value.y,value.z};
}
Vector3 normalize(Vector3 v) {
    const auto length = std::sqrt(double(v.x)*v.x + double(v.y)*v.y + double(v.z)*v.z);
    return length > 0 ? multiply(v, 1/length) : Vector3{};
}
}
void ProcessingPipeline::setSettings(const ProcessingSettings& settings) { settings_ = settings; reset(); }
void ProcessingPipeline::reset(const std::string& serial) { hands_.erase(serial); gestures_.erase(serial); }
void ProcessingPipeline::reset() { hands_.clear(); gestures_.clear(); }
FrameData ProcessingPipeline::process(const FrameData& raw, Millis arrivalMs) {
    if (settings_.bypass) { hands_.erase(raw.deviceId); return raw; }
    FrameData result = raw;
    std::erase_if(result.hands, [](const HandData& hand) { return !hand.isValid(); });
    auto& states = hands_[raw.deviceId];
    std::set<std::string> current;
    const auto quaternion = rotation(settings_.rotationDeg);
    const double unit = settings_.outputUnits == "m" ? .001 : 1;
    const Vector3 offset{float(settings_.offsetMm[0]),float(settings_.offsetMm[1]),float(settings_.offsetMm[2])};
    for (auto& hand : result.hands) {
        current.insert(hand.handType);
        auto& state = states[hand.handType];
        if (hand.nativeHandId && state.nativeId && hand.nativeHandId != state.nativeId) state = {};
        // Clock discontinuity resets rather than creating an extrapolated pose.
        if (state.initialized && arrivalMs < state.lastMs) state = {};
        if (hand.nativeHandId) state.nativeId = hand.nativeHandId;
        const auto delta = state.initialized ? arrivalMs - state.lastMs : 0;
        const auto alpha = settings_.smoothingMs > 0 ? 1 - std::exp(-double(delta)/settings_.smoothingMs) : 1;
        std::set<std::string> validPositions;
        auto position = [&](Vector3 value, const std::string& channel) {
            validPositions.insert(channel);
            auto prior = state.positions.find(channel);
            if (prior != state.positions.end()) {
                const auto dx = double(value.x) - prior->second.x, dy = double(value.y) - prior->second.y,
                           dz = double(value.z) - prior->second.z;
                if (std::sqrt(dx*dx + dy*dy + dz*dz) < settings_.deadzoneMm) value = prior->second;
                else if (settings_.smoothingMs > 0) value = add(prior->second, multiply(
                    {value.x-prior->second.x,value.y-prior->second.y,value.z-prior->second.z}, alpha));
            }
            state.positions[channel] = value;
            return multiply(add(rotate(scale(value,settings_.scale),quaternion),offset),unit);
        };
        hand.palm.position = position(hand.palm.position,"palm");
        hand.palm.velocity = multiply(rotate(scale(hand.palm.velocity,settings_.scale),quaternion),unit);
        hand.palm.normal = normalize(rotate(hand.palm.normal,quaternion));
        hand.palm.direction = normalize(rotate(hand.palm.direction,quaternion));
        hand.palm.orientation = product(quaternion,hand.palm.orientation);
        hand.palm.width = float(hand.palm.width*unit);
        if (hand.arm.isValid()) {
            hand.arm.wristPosition = position(hand.arm.wristPosition,"wrist");
            hand.arm.elbowPosition = position(hand.arm.elbowPosition,"elbow");
            hand.arm.rotation = product(quaternion,hand.arm.rotation);
            hand.arm.width = float(hand.arm.width*unit);
        }
        for (std::size_t f = 0; f < hand.fingers.size(); ++f) {
            auto& finger = hand.fingers[f];
            if (!finger.isValid()) continue;
            for (std::size_t b = 0; b < finger.bones.size(); ++b) {
                auto& bone = finger.bones[b];
                if (!bone.isValid()) continue;
                const auto key = "finger/" + std::to_string(f) + "/" + std::to_string(b);
                bone.prevJoint = position(bone.prevJoint,key + "/prev");
                bone.nextJoint = position(bone.nextJoint,key + "/next");
                bone.rotation = product(quaternion,bone.rotation);
                bone.width = float(bone.width*unit);
            }
        }
        std::erase_if(state.positions,[&](const auto& entry) { return !validPositions.contains(entry.first); });
        state.lastMs = arrivalMs; state.initialized = true;
    }
    std::erase_if(states, [&](const auto& entry) { return !current.contains(entry.first); });
    return result;
}
std::vector<OscMessage> ProcessingPipeline::gestures(const FrameData& frame, const std::string& alias) {
    std::vector<OscMessage> result;
    if (!settings_.gesturesEnabled) { gestures_.erase(frame.deviceId); return result; }
    auto& states = gestures_[frame.deviceId];
    std::set<std::string> current;
    const auto message = [&](const std::string& hand, const char* name, bool value) {
        result.push_back({"/leap/" + alias + "/" + hand + "/" + name,{value ? 1.f : 0.f}});
    };
    for (const auto& hand : frame.hands) {
        if (!hand.isValid()) continue;
        current.insert(hand.handType);
        auto& state = states[hand.handType];
        if (hand.nativeHandId && state.nativeId && hand.nativeHandId != state.nativeId) state = {};
        if (hand.nativeHandId) state.nativeId = hand.nativeHandId;
        state.pinch = state.pinch ? hand.pinchStrength > settings_.pinchExit : hand.pinchStrength >= settings_.pinchEnter;
        state.grab = state.grab ? hand.grabStrength > settings_.grabExit : hand.grabStrength >= settings_.grabEnter;
        message(hand.handType,"pinchActive",state.pinch); message(hand.handType,"grabActive",state.grab);
    }
    for (const auto& [hand, state] : states) {
        (void)state;
        if (!current.contains(hand)) { message(hand,"pinchActive",false); message(hand,"grabActive",false); }
    }
    std::erase_if(states, [&](const auto& entry) { return !current.contains(entry.first); });
    return result;
}
}
