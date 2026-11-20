#include "LeapCTrackingSource.hpp"
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <set>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace handbridge {
namespace {
constexpr std::size_t MaximumDevices = 64;
constexpr std::size_t MaximumHands = 32;
constexpr Millis HandshakeTimeoutMs = 3000;
constexpr Millis EnumerationIntervalMs = 1000;
constexpr unsigned PollBudget = 8;
Vector3 vector(const LEAP_VECTOR& value) { return {value.x, value.y, value.z}; }
Quaternion quaternion(const LEAP_QUATERNION& value) { return {value.w, value.x, value.y, value.z}; }
bool finite(const LEAP_VECTOR& value) {
    return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
}
}

LeapCTrackingSource::LeapCTrackingSource(LeapCApi api, Log log)
    : api_(api), log_(std::move(log)) {
    if (!api_.complete()) throw std::invalid_argument("LeapC API table is incomplete");
}
LeapCTrackingSource::~LeapCTrackingSource() { stop(); }
bool LeapCTrackingSource::owner(const char* operation) {
    const auto current = std::this_thread::get_id();
    if (owner_ == std::thread::id{}) owner_ = current;
    if (owner_ == current) return true;
    error(std::string(operation) + " must run on the tracking source owner thread");
    return false;
}
void LeapCTrackingSource::error(const std::string& operation, eLeapRS result) {
    std::ostringstream message;
    message << operation << " failed (LeapC 0x" << std::hex << static_cast<std::uint32_t>(result) << ')';
    error(message.str());
}
void LeapCTrackingSource::error(const std::string& message) {
    if (message == lastError_) return;
    lastError_ = message;
    if (log_) { try { log_("LeapC: " + message); } catch (...) {} }
}
void LeapCTrackingSource::add(TrackingEventKind kind, const std::string& serial) {
    events_.push_back({kind, serial, 0, 0, {}});
}
void LeapCTrackingSource::release(Device& device) noexcept {
    if (!device.handle) return;
    if (device.subscribed && connection_) (void)api_.unsubscribeEvents(connection_, device.handle);
    device.subscribed = false;
    api_.closeDevice(device.handle);
    device.handle = nullptr;
}
void LeapCTrackingSource::cleanup() noexcept {
    for (auto& [id, device] : devices_) { (void)id; release(device); }
    devices_.clear();
    if (connection_) {
        api_.closeConnection(connection_);
        api_.destroyConnection(connection_);
        connection_ = nullptr;
    }
    connected_ = opening_ = false;
}
void LeapCTrackingSource::stop() noexcept {
    // The host stops/joins its owner worker before destroying the source. No SDK
    // call may overlap this cleanup; the interface deliberately owns no GUI loop.
    cleanup();
    events_.clear();
    owner_ = {};
}
bool LeapCTrackingSource::start(Millis now) {
    if (!owner("start")) return false;
    if (connection_) {
        if (connected_ || (now >= openedMs_ && now - openedMs_ < HandshakeTimeoutMs)) return true;
        error("service handshake timed out; recreating the client connection");
        cleanup();
    }
    events_.clear();
    serviceVersion_.clear();
    LEAP_CONNECTION_CONFIG config{};
    config.size = sizeof(config);
    config.flags = eLeapConnectionConfig_MultiDeviceAware;
    ++connectionAttempts_;
    const auto created = api_.createConnection(&config, &connection_);
    if (created != eLeapRS_Success || !connection_) {
        error("LeapCreateConnection", created);
        if (connection_) cleanup();
        return false;
    }
    const auto opened = api_.openConnection(connection_);
    if (opened != eLeapRS_Success) {
        error("LeapOpenConnection", opened);
        cleanup();
        return false;
    }
    openedMs_ = now;
    opening_ = true;
    connected_ = false;
    nextEnumerationMs_ = now;
    return true; // ServiceConnected is emitted only after the actual SDK event.
}
bool LeapCTrackingSource::verifyServiceVersion() {
    LEAP_VERSION version{};
    const auto result = api_.getVersion(connection_, eLeapVersionPart_ServerLibrary, &version);
    if (result != eLeapRS_Success) {
        serviceVersion_.clear();
        error("LeapGetVersion ServerLibrary", result);
        cleanup();
        return false;
    }
    serviceVersion_ = std::to_string(version.major) + "." + std::to_string(version.minor) + "." + std::to_string(version.patch);
    if (version.major != 6 || version.minor < 2 || version.patch < 0) {
        error("Hyperion 6.2.0 or a newer 6.x tracking service is required; service reported " + serviceVersion_);
        cleanup();
        return false;
    }
    return true;
}
void LeapCTrackingSource::loseService(const std::string& reason) {
    const bool wasConnected = connected_;
    error(reason);
    cleanup();
    if (wasConnected) add(TrackingEventKind::ServiceLost);
}
void LeapCTrackingSource::loseDevice(std::uint32_t id) {
    const auto it = devices_.find(id);
    if (it == devices_.end()) return;
    if (it->second.announced) add(TrackingEventKind::DeviceLost, it->second.serial);
    release(it->second);
    devices_.erase(it);
}
std::string LeapCTrackingSource::serial(LEAP_DEVICE handle) {
    LEAP_DEVICE_INFO info{};
    info.size = sizeof(info);
    const auto probe = api_.getDeviceInfo(handle, &info);
    if (probe != eLeapRS_Success && probe != eLeapRS_InsufficientBuffer) {
        error("LeapGetDeviceInfo size query", probe); return {};
    }
    // Official header specifies that serial_length includes the terminating NUL.
    if (info.serial_length < 2 || info.serial_length > 4096) {
        error("device serial size is empty or exceeds the 4096-byte limit"); return {};
    }
    std::vector<char> storage(info.serial_length, '\0');
    info.serial = storage.data();
    info.serial_length = static_cast<std::uint32_t>(storage.size());
    const auto fetched = api_.getDeviceInfo(handle, &info);
    if (fetched != eLeapRS_Success) { error("LeapGetDeviceInfo serial query", fetched); return {}; }
    const auto end = std::find(storage.begin(), storage.end(), '\0');
    if (end == storage.begin() || end == storage.end()) {
        error("device serial is empty or not NUL terminated"); return {};
    }
    return std::string(storage.begin(), end);
}
bool LeapCTrackingSource::openAndSubscribe(std::uint32_t id, Millis now) {
    const auto it = devices_.find(id);
    if (it == devices_.end() || !connected_) return false;
    auto& device = it->second;
    if (device.subscribed) return true;
    release(device);
    const auto opened = api_.openDevice(device.reference, &device.handle);
    if (opened != eLeapRS_Success || !device.handle) {
        error("LeapOpenDevice id=" + std::to_string(id), opened);
        release(device);
        device.nextRetryMs = now + device.backoffMs;
        device.backoffMs = std::min<Millis>(1000, device.backoffMs * 2);
        return false;
    }
    const auto foundSerial = serial(device.handle);
    if (foundSerial.empty()) {
        release(device);
        device.nextRetryMs = now + device.backoffMs;
        device.backoffMs = std::min<Millis>(1000, device.backoffMs * 2);
        return false;
    }
    for (const auto& [otherId, other] : devices_) {
        if (otherId != id && other.announced && other.serial == foundSerial) {
            error("duplicate serial on SDK device id=" + std::to_string(id) + "; retaining the existing stream");
            device.serial = foundSerial;
            device.duplicate = true;
            release(device);
            return false;
        }
    }
    if (device.announced && device.serial != foundSerial) {
        add(TrackingEventKind::DeviceLost, device.serial);
        device.announced = false;
    }
    device.serial = foundSerial;
    device.duplicate = false;
    const bool firstAttemptForSerial = !device.announced;
    if (firstAttemptForSerial) {
        add(TrackingEventKind::DeviceConnected, device.serial);
        device.announced = true;
    }
    const auto subscribed = api_.subscribeEvents(connection_, device.handle);
    if (subscribed != eLeapRS_Success) {
        error("LeapSubscribeEvents id=" + std::to_string(id), subscribed);
        // Discovery reports failure through poll(). For an announced serial,
        // restartDevice() returns the failure directly to the retry scheduler.
        if (firstAttemptForSerial) add(TrackingEventKind::SubscriptionFailed, device.serial);
        release(device);
        return false; // The runtime now owns retries for this identified device.
    }
    device.subscribed = true;
    device.backoffMs = 100;
    add(TrackingEventKind::Subscribed, device.serial);
    return true;
}
bool LeapCTrackingSource::enumerate(Millis now) {
    if (!connected_) return false;
    nextEnumerationMs_ = now + EnumerationIntervalMs;
    std::uint32_t count = 0;
    const auto probe = api_.getDeviceList(connection_, nullptr, &count);
    if (probe != eLeapRS_Success && probe != eLeapRS_InsufficientBuffer) {
        error("LeapGetDeviceList size query", probe);
        if (probe == eLeapRS_NotConnected) loseService(lastError_);
        return false;
    }
    if (count > MaximumDevices) { error("device count exceeds the 64-device limit"); return false; }
    std::vector<LEAP_DEVICE_REF> references(count);
    if (count > 0) {
        const auto fetched = api_.getDeviceList(connection_, references.data(), &count);
        if (fetched != eLeapRS_Success || count > references.size()) {
            error("LeapGetDeviceList snapshot", fetched); return false;
        }
        references.resize(count);
    }
    std::set<std::uint32_t> present;
    for (const auto& reference : references) present.insert(reference.id);
    for (auto it = devices_.begin(); it != devices_.end();) {
        if (!present.contains(it->first)) { const auto id = it++->first; loseDevice(id); }
        else ++it;
    }
    for (const auto& reference : references) {
        auto known = devices_.find(reference.id);
        if (known != devices_.end() && known->second.reference.handle != reference.handle) {
            loseDevice(reference.id); known = devices_.end();
        }
        if (known == devices_.end()) {
            if (devices_.size() >= MaximumDevices) continue;
            Device device; device.reference = reference;
            devices_.emplace(reference.id, device);
            openAndSubscribe(reference.id, now);
        } else if (known->second.duplicate) {
            const auto duplicateSerial = known->second.serial;
            const bool retainedElsewhere = std::any_of(devices_.begin(), devices_.end(), [&](const auto& pair) {
                return pair.first != reference.id && pair.second.announced && pair.second.serial == duplicateSerial;
            });
            if (!retainedElsewhere) { known->second.duplicate = false; known->second.serial.clear(); openAndSubscribe(reference.id, now); }
        }
    }
    return true;
}
bool LeapCTrackingSource::copyTracking(const LEAP_TRACKING_EVENT& tracking, const std::string& deviceSerial, Millis now) {
    if (tracking.tracking_frame_id < 0 || tracking.info.timestamp < 0 || tracking.nHands > MaximumHands ||
        (tracking.nHands != 0 && !tracking.pHands)) {
        error("malformed tracking frame metadata"); return false;
    }
    FrameData frame;
    frame.deviceId = deviceSerial;
    frame.timestamp = static_cast<std::uint64_t>(tracking.info.timestamp);
    frame.hands.reserve(tracking.nHands);
    for (std::uint32_t index = 0; index < tracking.nHands; ++index) {
        const auto& source = tracking.pHands[index];
        if ((source.type != eLeapHandType_Left && source.type != eLeapHandType_Right) || !finite(source.palm.position)) {
            error("tracking hand has invalid chirality or palm coordinates"); continue;
        }
        HandData hand;
        hand.nativeHandId = source.id;
        hand.flags = source.flags;
        hand.handType = source.type == eLeapHandType_Left ? "left" : "right";
        hand.palm.position = vector(source.palm.position);
        hand.palm.stabilizedPosition = vector(source.palm.stabilized_position);
        hand.palm.velocity = vector(source.palm.velocity);
        hand.palm.normal = vector(source.palm.normal);
        hand.palm.direction = vector(source.palm.direction);
        hand.palm.orientation = quaternion(source.palm.orientation);
        hand.palm.width = source.palm.width;
        hand.arm.wristPosition = vector(source.arm.next_joint);
        hand.arm.elbowPosition = vector(source.arm.prev_joint);
        hand.arm.width = source.arm.width;
        hand.arm.rotation = quaternion(source.arm.rotation);
        hand.arm.valid = finite(source.arm.next_joint) && finite(source.arm.prev_joint);
        hand.fingers.reserve(5);
        for (const auto& sourceDigit : source.digits) {
            FingerData digit;
            digit.fingerId = sourceDigit.finger_id;
            digit.isExtended = sourceDigit.is_extended != 0;
            digit.bones.reserve(4);
            for (const auto& sourceBone : sourceDigit.bones) {
                BoneData bone;
                bone.prevJoint = vector(sourceBone.prev_joint);
                bone.nextJoint = vector(sourceBone.next_joint);
                bone.width = sourceBone.width;
                bone.rotation = quaternion(sourceBone.rotation);
                bone.valid = finite(sourceBone.prev_joint) && finite(sourceBone.next_joint);
                digit.bones.push_back(bone);
            }
            hand.fingers.push_back(std::move(digit));
        }
        hand.pinchStrength = source.pinch_strength;
        hand.pinchDistance = source.pinch_distance;
        hand.grabAngle = source.grab_angle;
        hand.grabStrength = source.grab_strength;
        hand.confidence = source.confidence; // SDK calls this reserved/always 1; not a quality estimate.
        hand.visibleTime = source.visible_time;
        frame.hands.push_back(std::move(hand));
    }
    events_.push_back({TrackingEventKind::Frame, deviceSerial,
                      static_cast<std::uint64_t>(tracking.tracking_frame_id), now, std::move(frame)});
    return true;
}
std::vector<TrackingEvent> LeapCTrackingSource::poll(Millis now) {
    if (!owner("poll")) return {};
    if (!connection_) return std::exchange(events_, {});
    if (opening_ && now >= openedMs_ && now - openedMs_ >= HandshakeTimeoutMs) {
        error("service handshake timed out; waiting for bounded connection retry");
        cleanup();
        return std::exchange(events_, {});
    }
    for (unsigned index = 0; index < PollBudget && connection_; ++index) {
        LEAP_CONNECTION_MESSAGE message{};
        message.size = sizeof(message);
        const auto result = api_.pollConnection(connection_, 0, &message);
        if (result == eLeapRS_Timeout) break;
        if (result != eLeapRS_Success) {
            if (!connected_ && (result == eLeapRS_NotConnected || result == eLeapRS_HandshakeIncomplete)) break;
            ++pollErrors_;
            error("LeapPollConnection", result);
            loseService(lastError_);
            break;
        }
        if (message.type == eLeapEventType_None) break;
        if (message.type == eLeapEventType_Connection) {
            if (!connected_) {
                if (!verifyServiceVersion()) break;
                connected_ = true; opening_ = false;
                add(TrackingEventKind::ServiceConnected);
                enumerate(now);
            }
        } else if (message.type == eLeapEventType_ConnectionLost) {
            loseService("service connection lost");
            break;
        } else if (connected_ && message.type == eLeapEventType_Device && message.device_event) {
            // System-wide Device events carry their ID inside device_event, not message.device_id.
            const auto reference = message.device_event->device;
            auto known = devices_.find(reference.id);
            if (known != devices_.end() && known->second.reference.handle != reference.handle) {
                loseDevice(reference.id); known = devices_.end();
            }
            if (known == devices_.end() && devices_.size() < MaximumDevices) {
                Device device; device.reference = reference;
                devices_.emplace(reference.id, device);
                openAndSubscribe(reference.id, now);
            }
        } else if (connected_ && message.type == eLeapEventType_DeviceLost && message.device_event) {
            loseDevice(message.device_event->device.id);
        } else if (connected_ && message.type == eLeapEventType_Tracking && message.tracking_event) {
            const auto found = devices_.find(message.device_id);
            if (found != devices_.end() && found->second.subscribed)
                copyTracking(*message.tracking_event, found->second.serial, now);
        }
    }
    if (connected_) {
        if (now >= nextEnumerationMs_) enumerate(now);
        for (auto& [id, device] : devices_) {
            if (!device.announced && !device.duplicate && now >= device.nextRetryMs) openAndSubscribe(id, now);
        }
    }
    return std::exchange(events_, {});
}
bool LeapCTrackingSource::restartDevice(const std::string& deviceSerial, Millis now) {
    if (!owner("restartDevice") || !connected_) return false;
    // Re-enumeration refreshes transient SDK references and removes absent handles;
    // it never unsubscribes healthy devices or substitutes a different serial.
    enumerate(now);
    const auto found = std::find_if(devices_.begin(), devices_.end(), [&](const auto& pair) {
        return pair.second.announced && pair.second.serial == deviceSerial;
    });
    if (found == devices_.end()) return false;
    release(found->second);
    return openAndSubscribe(found->first, now);
}
}
