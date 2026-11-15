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
        hand.palm.position = {100.f + static_cast<float>(sequence_[index] % 50), 200.f, -30.f};
        hand.arm.wristPosition = {80.f, 150.f, -40.f};
        hand.pinchStrength = .25f;
        hand.grabStrength = .5f;
        for (int finger = 0; finger < 5; ++finger) {
            FingerData digit;
            digit.fingerId = finger;
            digit.bones.resize(4);
            digit.bones[3].nextJoint = {float(finger * 10), 250.f, -20.f};
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
