#include "MockTrackingSource.hpp"

namespace handbridge {
bool MockTrackingSource::start(Millis now) {
    running_ = announce_ = true;
    started_ = nextFrame_ = now;
    sequence_ = {};
    return true;
}
std::vector<TrackingEvent> MockTrackingSource::poll(Millis now) {
    std::vector<TrackingEvent> events;
    if (!running_) return events;
    if (announce_) {
        events.push_back({TrackingEventKind::ServiceConnected, {}, 0, now, {}});
        for (const auto* serial : {"SIM-A", "SIM-B"}) {
            events.push_back({TrackingEventKind::DeviceConnected, serial, 0, now, {}});
            events.push_back({TrackingEventKind::Subscribed, serial, 0, now, {}});
        }
        announce_ = false;
    }
    if (now < nextFrame_) return events;
    nextFrame_ = now + 16; // No catch-up burst or unbounded backlog after a pause.
    for (std::size_t index = 0; index < 2; ++index) {
        const auto elapsed = now - started_;
        if (stallScenario_ && index == 0 && elapsed >= 200 && elapsed < 900) continue;
        FrameData frame;
        frame.deviceId = index == 0 ? "SIM-A" : "SIM-B";
        frame.timestamp = now * 1000;
        HandData hand;
        hand.handType = index == 0 ? "left" : "right";
        hand.nativeHandId = static_cast<std::uint32_t>(100 + index);
        hand.confidence = 1;
        hand.visibleTime = elapsed * 1000;
        hand.pinchDistance = 35;
        hand.grabAngle = .7f;
        hand.palm.position = {100.f + static_cast<float>(sequence_[index] % 50), 200.f, -30.f};
        hand.palm.stabilizedPosition = hand.palm.position;
        hand.palm.direction = {0, 0, -1};
        hand.palm.normal = {0, -1, 0};
        hand.palm.width = 80;
        hand.arm.wristPosition = {80.f, 150.f, -40.f};
        hand.arm.elbowPosition = {80.f, 150.f, 230.f};
        hand.arm.width = 50;
        hand.pinchStrength = .25f;
        hand.grabStrength = .5f;
        for (int finger = 0; finger < 5; ++finger) {
            FingerData digit;
            digit.fingerId = finger;
            // Keep the legacy simulated fingertips, fill in the four named segments.
            const float x = static_cast<float>(finger * 10);
            const std::array<float, 5> joints{finger == 0 ? 195.f : 175.f, 195.f, 220.f, 237.f, 250.f};
            for (std::size_t segment = 0; segment < 4; ++segment) {
                BoneData bone;
                bone.prevJoint = {x, joints[segment], -20.f};
                bone.nextJoint = {x, joints[segment + 1], -20.f};
                bone.width = finger == 0 && segment == 0 ? 0.f : 9.f;
                digit.bones.push_back(bone);
            }
            hand.fingers.push_back(digit);
        }
        frame.hands.push_back(hand);
        events.push_back({TrackingEventKind::Frame, frame.deviceId, ++sequence_[index], now, frame});
    }
    return events;
}
bool MockTrackingSource::restartDevice(const std::string& serial, Millis now) {
    return running_ && (serial == "SIM-A" || serial == "SIM-B") &&
           !(stallScenario_ && serial == "SIM-A" && now - started_ < 900);
}
}
