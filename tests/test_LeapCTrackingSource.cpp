#include <gtest/gtest.h>
#include "tracking/LeapCTrackingSource.hpp"
#include "runtime/EngineRuntime.hpp"
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <deque>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

// The production adapter is compiled against the genuine SDK header. Only its
// documented C entry points are injected here; there is no fake LeapC.h, linked
// SDK binary, service or physical camera in this test process.
namespace {
using namespace handbridge;

struct BorrowedMessage {
    eLeapEventType kind = eLeapEventType_None;
    eLeapRS result = eLeapRS_Success;
    std::uint32_t id = 0;
    LEAP_CONNECTION_EVENT connected{};
    LEAP_CONNECTION_LOST_EVENT lost{};
    LEAP_DEVICE_EVENT device{};
    LEAP_TRACKING_EVENT tracking{};
    std::vector<LEAP_HAND> hands;
};

class FakeLeapC {
public:
    static FakeLeapC* active;
    std::map<std::uint32_t, std::string> serials{{11, "SDK-A"}, {22, "SDK-B"}};
    std::map<std::uint32_t, void*> referenceOverrides;
    std::vector<std::uint32_t> present{11, 22};
    std::deque<BorrowedMessage> queued;
    BorrowedMessage borrowed;
    std::map<LEAP_DEVICE, std::uint32_t> liveDevices;
    std::set<LEAP_DEVICE> subscriptions;
    std::set<LEAP_CONNECTION> liveConnections;
    std::set<std::uint32_t> failOpen, failInfo, failSubscribe;
    std::map<std::uint32_t, unsigned> opens, closes, subscribes, unsubscribes;
    std::vector<std::string> calls;
    std::vector<std::uint32_t> pollTimeouts;
    eLeapRS createResult = eLeapRS_Success, openResult = eLeapRS_Success;
    eLeapRS serialProbeResult = eLeapRS_InsufficientBuffer;
    eLeapRS versionResult = eLeapRS_Success;
    LEAP_VERSION serverVersion{6, 2, 0};
    unsigned creates = 0, destroys = 0, closesOfConnection = 0, enumerations = 0;
    unsigned serialProbes = 0, serialReads = 0, versionQueries = 0;
    bool serviceReady = false;
    std::uintptr_t nextConnection = 0x10000, nextDevice = 0x20000;

    static LEAP_DEVICE_REF reference(std::uint32_t id) {
        if (active && active->referenceOverrides.contains(id)) return {active->referenceOverrides.at(id), id};
        return {reinterpret_cast<void*>(std::uintptr_t(0x30000) + id), id};
    }
    void serviceConnected() { BorrowedMessage message; message.kind = eLeapEventType_Connection; queued.push_back(message); }
    void serviceLost() { BorrowedMessage message; message.kind = eLeapEventType_ConnectionLost; queued.push_back(message); }
    void deviceEvent(std::uint32_t id, bool connected = true) {
        BorrowedMessage message;
        message.kind = connected ? eLeapEventType_Device : eLeapEventType_DeviceLost;
        message.device.device = reference(id);
        queued.push_back(message);
    }
    void tracking(std::uint32_t id, std::int64_t frameId, std::int64_t timestamp,
                  std::vector<LEAP_HAND> hands = {}) {
        BorrowedMessage message;
        message.kind = eLeapEventType_Tracking;
        message.id = id;
        message.tracking.info.frame_id = frameId + 1000; // Image ID is deliberately distinct.
        message.tracking.tracking_frame_id = frameId;
        message.tracking.info.timestamp = timestamp;
        message.hands = std::move(hands);
        queued.push_back(std::move(message));
    }
    void pollFailure(eLeapRS result) { BorrowedMessage message; message.result = result; queued.push_back(message); }

    LeapCApi api() const {
        LeapCApi api;
        api.createConnection = create;
        api.openConnection = open;
        api.pollConnection = poll;
        api.getDeviceList = list;
        api.openDevice = openDevice;
        api.getDeviceInfo = info;
        api.subscribeEvents = subscribe;
        api.unsubscribeEvents = unsubscribe;
        api.closeDevice = closeDevice;
        api.closeConnection = close;
        api.destroyConnection = destroy;
        api.getVersion = version;
        return api;
    }

private:
    static eLeapRS LEAP_CALL create(const LEAP_CONNECTION_CONFIG* config, LEAP_CONNECTION* output) {
        auto& sdk = *active;
        ++sdk.creates;
        sdk.calls.push_back("create");
        EXPECT_NE(config, nullptr);
        if (config) {
            EXPECT_EQ(config->size, sizeof(LEAP_CONNECTION_CONFIG));
            EXPECT_NE(config->flags & eLeapConnectionConfig_MultiDeviceAware, 0u);
        }
        *output = nullptr;
        if (sdk.createResult != eLeapRS_Success) return sdk.createResult;
        *output = reinterpret_cast<LEAP_CONNECTION>(++sdk.nextConnection);
        sdk.liveConnections.insert(*output);
        return eLeapRS_Success;
    }
    static eLeapRS LEAP_CALL open(LEAP_CONNECTION connection) {
        auto& sdk = *active;
        sdk.calls.push_back("open_connection");
        EXPECT_TRUE(sdk.liveConnections.contains(connection));
        return sdk.openResult;
    }
    static eLeapRS LEAP_CALL version(LEAP_CONNECTION connection, eLeapVersionPart part, LEAP_VERSION* output) {
        auto& sdk = *active;
        ++sdk.versionQueries;
        sdk.calls.push_back("server_version");
        EXPECT_TRUE(sdk.liveConnections.contains(connection));
        EXPECT_TRUE(sdk.serviceReady) << "The actual service version is unavailable before its Connection event";
        EXPECT_EQ(part, eLeapVersionPart_ServerLibrary);
        EXPECT_NE(output, nullptr);
        if (sdk.versionResult != eLeapRS_Success) return sdk.versionResult;
        if (output) *output = sdk.serverVersion;
        return eLeapRS_Success;
    }
    static eLeapRS LEAP_CALL poll(LEAP_CONNECTION connection, std::uint32_t timeout, LEAP_CONNECTION_MESSAGE* output) {
        auto& sdk = *active;
        EXPECT_TRUE(sdk.liveConnections.contains(connection));
        sdk.pollTimeouts.push_back(timeout);
        // SDK event memory is invalidated by the next poll. Even the final
        // timeout destroys the previous pHands storage, exposing borrowed data.
        sdk.borrowed = {};
        if (sdk.queued.empty()) return eLeapRS_Timeout;
        sdk.borrowed = std::move(sdk.queued.front());
        sdk.queued.pop_front();
        auto& message = sdk.borrowed;
        if (message.result != eLeapRS_Success) return message.result;
        output->type = message.kind;
        output->device_id = message.id;
        switch (message.kind) {
        case eLeapEventType_Connection:
            sdk.serviceReady = true;
            output->connection_event = &message.connected;
            break;
        case eLeapEventType_ConnectionLost:
            sdk.serviceReady = false;
            output->connection_lost_event = &message.lost;
            break;
        case eLeapEventType_Device:
        case eLeapEventType_DeviceLost:
            // Device events identify the camera through device.id, not the
            // message's multi-device tracking routing field (which is zero).
            output->device_id = 0;
            output->device_event = &message.device;
            break;
        case eLeapEventType_Tracking:
            message.tracking.nHands = static_cast<std::uint32_t>(message.hands.size());
            message.tracking.pHands = message.hands.empty() ? nullptr : message.hands.data();
            output->tracking_event = &message.tracking;
            break;
        default: output->pointer = nullptr; break;
        }
        return eLeapRS_Success;
    }
    static eLeapRS LEAP_CALL list(LEAP_CONNECTION connection, LEAP_DEVICE_REF* output, std::uint32_t* count) {
        auto& sdk = *active;
        ++sdk.enumerations;
        sdk.calls.push_back("enumerate");
        EXPECT_TRUE(sdk.liveConnections.contains(connection));
        EXPECT_TRUE(sdk.serviceReady) << "Enumeration must wait for LeapC's asynchronous Connection event";
        if (!sdk.serviceReady) return eLeapRS_HandshakeIncomplete;
        if (!output) { *count = static_cast<std::uint32_t>(sdk.present.size()); return eLeapRS_Success; }
        if (*count < sdk.present.size()) { *count = static_cast<std::uint32_t>(sdk.present.size()); return eLeapRS_InsufficientBuffer; }
        for (std::size_t index = 0; index < sdk.present.size(); ++index) output[index] = reference(sdk.present[index]);
        *count = static_cast<std::uint32_t>(sdk.present.size());
        return eLeapRS_Success;
    }
    static eLeapRS LEAP_CALL openDevice(LEAP_DEVICE_REF ref, LEAP_DEVICE* output) {
        auto& sdk = *active;
        ++sdk.opens[ref.id];
        sdk.calls.push_back("open_device:" + std::to_string(ref.id));
        *output = nullptr;
        if (sdk.failOpen.contains(ref.id)) return eLeapRS_InsufficientResources;
        *output = reinterpret_cast<LEAP_DEVICE>(++sdk.nextDevice);
        sdk.liveDevices[*output] = ref.id;
        return eLeapRS_Success;
    }
    static eLeapRS LEAP_CALL info(LEAP_DEVICE handle, LEAP_DEVICE_INFO* info) {
        auto& sdk = *active;
        EXPECT_TRUE(sdk.liveDevices.contains(handle));
        const auto found = sdk.liveDevices.find(handle);
        if (found == sdk.liveDevices.end()) return eLeapRS_InvalidArgument;
        const auto id = found->second;
        if (sdk.failInfo.contains(id)) return eLeapRS_ProtocolError;
        EXPECT_EQ(info->size, sizeof(LEAP_DEVICE_INFO));
        const auto& serial = sdk.serials.at(id);
        const auto needed = static_cast<std::uint32_t>(serial.size() + 1);
        if (!info->serial) {
            ++sdk.serialProbes;
            info->serial_length = needed;
            return sdk.serialProbeResult;
        }
        ++sdk.serialReads;
        if (info->serial_length < needed) { info->serial_length = needed; return eLeapRS_InsufficientBuffer; }
        std::memcpy(info->serial, serial.c_str(), needed);
        info->serial_length = needed;
        return eLeapRS_Success;
    }
    static eLeapRS LEAP_CALL subscribe(LEAP_CONNECTION connection, LEAP_DEVICE handle) {
        auto& sdk = *active;
        EXPECT_TRUE(sdk.liveConnections.contains(connection));
        EXPECT_TRUE(sdk.liveDevices.contains(handle));
        if (!sdk.liveDevices.contains(handle)) return eLeapRS_InvalidArgument;
        const auto id = sdk.liveDevices.at(handle);
        ++sdk.subscribes[id];
        sdk.calls.push_back("subscribe:" + std::to_string(id));
        if (sdk.failSubscribe.contains(id)) return eLeapRS_ProtocolError;
        EXPECT_FALSE(sdk.subscriptions.contains(handle));
        sdk.subscriptions.insert(handle);
        return eLeapRS_Success;
    }
    static eLeapRS LEAP_CALL unsubscribe(LEAP_CONNECTION connection, LEAP_DEVICE handle) {
        auto& sdk = *active;
        EXPECT_TRUE(sdk.liveConnections.contains(connection));
        EXPECT_TRUE(sdk.liveDevices.contains(handle));
        if (!sdk.liveDevices.contains(handle)) return eLeapRS_InvalidArgument;
        const auto id = sdk.liveDevices.at(handle);
        ++sdk.unsubscribes[id];
        sdk.calls.push_back("unsubscribe:" + std::to_string(id));
        EXPECT_EQ(sdk.subscriptions.erase(handle), 1u);
        return eLeapRS_Success;
    }
    static void LEAP_CALL closeDevice(LEAP_DEVICE handle) {
        auto& sdk = *active;
        EXPECT_TRUE(sdk.liveDevices.contains(handle)) << "A device handle must close exactly once";
        if (!sdk.liveDevices.contains(handle)) return;
        const auto id = sdk.liveDevices.at(handle);
        EXPECT_FALSE(sdk.subscriptions.contains(handle)) << "Unsubscribe before releasing an owned device";
        ++sdk.closes[id];
        sdk.calls.push_back("close_device:" + std::to_string(id));
        sdk.liveDevices.erase(handle);
    }
    static void LEAP_CALL close(LEAP_CONNECTION connection) {
        auto& sdk = *active;
        ++sdk.closesOfConnection;
        sdk.calls.push_back("close_connection");
        EXPECT_TRUE(sdk.liveConnections.contains(connection));
        EXPECT_TRUE(sdk.liveDevices.empty()) << "Owned devices must be released before closing their connection";
        sdk.serviceReady = false;
    }
    static void LEAP_CALL destroy(LEAP_CONNECTION connection) {
        auto& sdk = *active;
        ++sdk.destroys;
        sdk.calls.push_back("destroy_connection");
        EXPECT_EQ(sdk.liveConnections.erase(connection), 1u);
    }
};
FakeLeapC* FakeLeapC::active = nullptr;

LEAP_HAND hand(float x, std::uint32_t id = 42, eLeapHandType side = eLeapHandType_Left) {
    LEAP_HAND hand{};
    hand.id = id;
    hand.type = side;
    hand.confidence = .75f;
    hand.flags = eLeapHandFlag_GestureDetectionAvailable | eLeapHandFlag_GesturePinch;
    hand.pinch_distance = 17.25f;
    hand.grab_angle = 1.5f;
    hand.palm.stabilized_position.x = x + 2;
    hand.visible_time = 2500000;
    hand.pinch_strength = .25f;
    hand.grab_strength = .5f;
    hand.palm.position.x = x; hand.palm.position.y = 200; hand.palm.position.z = -30;
    hand.palm.velocity.x = 1; hand.palm.velocity.y = 2; hand.palm.velocity.z = 3;
    hand.palm.normal.y = 1; hand.palm.direction.z = -1;
    hand.palm.orientation.w = .5f; hand.palm.orientation.x = -.5f;
    hand.palm.orientation.y = .25f; hand.palm.orientation.z = -.25f;
    hand.palm.width = 80;
    hand.arm.prev_joint.x = 7; hand.arm.prev_joint.y = 8; hand.arm.prev_joint.z = 9;
    hand.arm.next_joint.x = 4; hand.arm.next_joint.y = 5; hand.arm.next_joint.z = 6;
    hand.arm.width = 25; hand.arm.rotation.w = 1;
    for (int finger = 0; finger < 5; ++finger) {
        auto& digit = hand.digits[finger];
        digit.finger_id = 100 + finger;
        digit.is_extended = finger % 2 == 0;
        for (int index = 0; index < 4; ++index) {
            auto& bone = digit.bones[index];
            bone.prev_joint.x = static_cast<float>(finger * 10 + index);
            bone.next_joint.x = static_cast<float>(100 + finger * 10 + index);
            bone.next_joint.y = 250; bone.next_joint.z = -20;
            bone.width = static_cast<float>(5 + index);
            bone.rotation.w = 1;
        }
    }
    return hand;
}

std::size_t count(const std::vector<TrackingEvent>& events, TrackingEventKind kind, const std::string& serial = {}) {
    return static_cast<std::size_t>(std::count_if(events.begin(), events.end(), [&](const TrackingEvent& event) {
        return event.kind == kind && (serial.empty() || event.serial == serial);
    }));
}
const TrackingEvent* find(const std::vector<TrackingEvent>& events, TrackingEventKind kind, const std::string& serial) {
    const auto found = std::find_if(events.begin(), events.end(), [&](const TrackingEvent& event) {
        return event.kind == kind && event.serial == serial;
    });
    return found == events.end() ? nullptr : &*found;
}

class LeapCTrackingSourceTest : public testing::Test {
protected:
    FakeLeapC sdk;
    std::unique_ptr<LeapCTrackingSource> source;
    std::vector<std::string> logs;
    void SetUp() override {
        FakeLeapC::active = &sdk;
        source = std::make_unique<LeapCTrackingSource>(sdk.api(), [&](const std::string& line) { logs.push_back(line); });
    }
    void TearDown() override {
        source.reset();
        EXPECT_TRUE(sdk.liveDevices.empty());
        EXPECT_TRUE(sdk.subscriptions.empty());
        EXPECT_TRUE(sdk.liveConnections.empty());
        FakeLeapC::active = nullptr;
    }
    std::vector<TrackingEvent> connect() {
        EXPECT_TRUE(source->start(0));
        sdk.serviceConnected();
        return source->poll(0);
    }
};

TEST_F(LeapCTrackingSourceTest, ConnectionIsMultiDeviceAwareAndEnumerationWaitsForServiceAcknowledgement) {
    ASSERT_TRUE(source->start(0));
    EXPECT_TRUE(source->start(100));
    EXPECT_EQ(sdk.creates, 1u);
    EXPECT_TRUE(source->poll(100).empty());
    EXPECT_EQ(sdk.enumerations, 0u);
    sdk.serviceConnected();
    const auto events = source->poll(101);
    ASSERT_FALSE(events.empty());
    EXPECT_EQ(events.front().kind, TrackingEventKind::ServiceConnected);
    EXPECT_EQ(count(events, TrackingEventKind::DeviceConnected), 2u);
    EXPECT_EQ(count(events, TrackingEventKind::Subscribed), 2u);
    EXPECT_EQ(sdk.serialProbes, 2u); EXPECT_EQ(sdk.serialReads, 2u);
    EXPECT_EQ(sdk.subscribes[11], 1u); EXPECT_EQ(sdk.subscribes[22], 1u);
    EXPECT_EQ(sdk.versionQueries, 1u); EXPECT_EQ(source->serviceVersion(), "6.2.0");
    EXPECT_LT(std::find(sdk.calls.begin(), sdk.calls.end(), "server_version"),
              std::find(sdk.calls.begin(), sdk.calls.end(), "enumerate"));
    EXPECT_TRUE(std::all_of(sdk.pollTimeouts.begin(), sdk.pollTimeouts.end(), [](auto timeout) { return timeout == 0; }));
}

TEST_F(LeapCTrackingSourceTest, HyperionFiveServiceIsRejectedBeforeAnnouncingHealthOrOpeningDevices) {
    sdk.serverVersion = {5, 30, 3};
    const auto events = connect();
    EXPECT_EQ(count(events, TrackingEventKind::ServiceConnected), 0u);
    EXPECT_EQ(count(events, TrackingEventKind::DeviceConnected), 0u);
    EXPECT_EQ(count(events, TrackingEventKind::Subscribed), 0u);
    EXPECT_EQ(sdk.versionQueries, 1u); EXPECT_EQ(sdk.enumerations, 0u);
    EXPECT_TRUE(sdk.liveConnections.empty()); EXPECT_TRUE(sdk.liveDevices.empty());
    EXPECT_EQ(sdk.destroys, 1u); EXPECT_EQ(source->serviceVersion(), "5.30.3");
    EXPECT_NE(source->lastError().find("6.2"), std::string::npos);
}

TEST_F(LeapCTrackingSourceTest, HyperionSixOneServiceIsRejectedUntilTheSupportedSixTwoMinimum) {
    sdk.serverVersion = {6, 1, 9};
    const auto events = connect();
    EXPECT_EQ(count(events, TrackingEventKind::ServiceConnected), 0u);
    EXPECT_EQ(count(events, TrackingEventKind::Subscribed), 0u);
    EXPECT_EQ(sdk.enumerations, 0u); EXPECT_EQ(sdk.destroys, 1u);
    EXPECT_TRUE(sdk.liveConnections.empty()); EXPECT_EQ(source->serviceVersion(), "6.1.9");
    EXPECT_NE(source->lastError().find("6.2"), std::string::npos);
}

TEST_F(LeapCTrackingSourceTest, FailedServiceVersionQueryCannotBecomeAHealthyConnection) {
    sdk.versionResult = eLeapRS_ProtocolError;
    const auto events = connect();
    EXPECT_EQ(count(events, TrackingEventKind::ServiceConnected), 0u);
    EXPECT_EQ(count(events, TrackingEventKind::DeviceConnected), 0u);
    EXPECT_EQ(sdk.versionQueries, 1u); EXPECT_EQ(sdk.enumerations, 0u);
    EXPECT_EQ(sdk.destroys, 1u); EXPECT_TRUE(sdk.liveConnections.empty());
    EXPECT_TRUE(source->serviceVersion().empty());
    EXPECT_NE(source->lastError().find("LeapGetVersion"), std::string::npos);
}

TEST_F(LeapCTrackingSourceTest, LaterHyperionSixMinorCanConnectAndReportsItsActualServiceVersion) {
    sdk.serverVersion = {6, 3, 1};
    const auto events = connect();
    EXPECT_EQ(count(events, TrackingEventKind::ServiceConnected), 1u);
    EXPECT_EQ(count(events, TrackingEventKind::Subscribed), 2u);
    EXPECT_EQ(source->serviceVersion(), "6.3.1");
}

TEST_F(LeapCTrackingSourceTest, GenuineSdkHandsAreCopiedBeforeTheNextPollInvalidatesBorrowedMemory) {
    sdk.serialProbeResult = eLeapRS_Success; // Both documented sizing outcomes are tolerated.
    connect();
    sdk.tracking(11, 101, 1234567, {hand(123)});
    sdk.tracking(22, 202, 2345678, {hand(456, 99, eLeapHandType_Right)});
    auto events = source->poll(42);
    ASSERT_EQ(count(events, TrackingEventKind::Frame), 2u);
    const auto a = find(events, TrackingEventKind::Frame, "SDK-A");
    const auto b = find(events, TrackingEventKind::Frame, "SDK-B");
    ASSERT_NE(a, nullptr); ASSERT_NE(b, nullptr);
    EXPECT_EQ(a->frameId, 101u); EXPECT_EQ(a->arrivalMs, 42u);
    EXPECT_EQ(a->frame.timestamp, 1234567u); EXPECT_EQ(a->frame.deviceId, "SDK-A");
    ASSERT_EQ(a->frame.hands.size(), 1u); ASSERT_EQ(b->frame.hands.size(), 1u);
    const auto& observed = a->frame.hands.front();
    ASSERT_TRUE(observed.nativeHandId.has_value()); EXPECT_EQ(*observed.nativeHandId, 42u);
    EXPECT_EQ(observed.handType, "left"); EXPECT_EQ(b->frame.hands.front().handType, "right");
    EXPECT_FLOAT_EQ(observed.palm.position.x, 123); EXPECT_FLOAT_EQ(b->frame.hands.front().palm.position.x, 456);
    EXPECT_FLOAT_EQ(observed.palm.velocity.z, 3); EXPECT_FLOAT_EQ(observed.palm.normal.y, 1);
    EXPECT_FLOAT_EQ(observed.palm.direction.z, -1); EXPECT_FLOAT_EQ(observed.palm.width, 80);
    EXPECT_FLOAT_EQ(observed.palm.orientation.w, .5f); EXPECT_FLOAT_EQ(observed.palm.orientation.x, -.5f);
    EXPECT_FLOAT_EQ(observed.palm.orientation.y, .25f); EXPECT_FLOAT_EQ(observed.palm.orientation.z, -.25f);
    EXPECT_FLOAT_EQ(observed.arm.wristPosition.x, 4); EXPECT_FLOAT_EQ(observed.arm.elbowPosition.z, 9);
    EXPECT_FLOAT_EQ(observed.arm.width, 25); EXPECT_FLOAT_EQ(observed.arm.rotation.w, 1);
    EXPECT_FLOAT_EQ(observed.pinchStrength, .25f); EXPECT_FLOAT_EQ(observed.grabStrength, .5f);
    EXPECT_FLOAT_EQ(observed.confidence, .75f); EXPECT_EQ(observed.visibleTime, 2500000u);
    EXPECT_EQ(observed.flags, 3u);
    EXPECT_FLOAT_EQ(observed.pinchDistance, 17.25f);
    EXPECT_FLOAT_EQ(observed.grabAngle, 1.5f);
    EXPECT_FLOAT_EQ(observed.palm.stabilizedPosition.x, 125);
    ASSERT_EQ(observed.fingers.size(), 5u);
    EXPECT_EQ(observed.fingers[1].fingerId, 101); EXPECT_FALSE(observed.fingers[1].isExtended);
    ASSERT_EQ(observed.fingers[4].bones.size(), 4u);
    EXPECT_FLOAT_EQ(observed.fingers[4].bones[3].nextJoint.x, 143);
    EXPECT_FLOAT_EQ(observed.fingers[4].bones[3].width, 8);
    source->stop(); // Source lifetime cannot invalidate returned owned observations.
    EXPECT_FLOAT_EQ(observed.palm.position.x, 123);
}

TEST_F(LeapCTrackingSourceTest, EmptyHandsAreFreshFramesAndUnknownDeviceFramesAreIgnored) {
    connect();
    sdk.tracking(11, 1, 1000);
    sdk.tracking(999, 2, 2000, {hand(10)});
    const auto events = source->poll(10);
    ASSERT_EQ(count(events, TrackingEventKind::Frame), 1u);
    const auto frame = find(events, TrackingEventKind::Frame, "SDK-A");
    ASSERT_NE(frame, nullptr); EXPECT_TRUE(frame->frame.hands.empty());
    EXPECT_EQ(frame->frameId, 1u); EXPECT_EQ(frame->arrivalMs, 10u);
    EXPECT_EQ(sdk.destroys, 0u);
}

TEST_F(LeapCTrackingSourceTest, FailedSubscriptionIsVisibleAndTargetedRetryDoesNotReopenTheOtherDevice) {
    sdk.failSubscribe.insert(11);
    auto events = connect();
    EXPECT_EQ(count(events, TrackingEventKind::DeviceConnected, "SDK-A"), 1u);
    EXPECT_EQ(count(events, TrackingEventKind::SubscriptionFailed, "SDK-A"), 1u);
    EXPECT_EQ(count(events, TrackingEventKind::Subscribed, "SDK-A"), 0u);
    EXPECT_EQ(count(events, TrackingEventKind::Subscribed, "SDK-B"), 1u);
    EXPECT_EQ(sdk.closes[11], 1u); EXPECT_EQ(sdk.closes[22], 0u);
    const auto opensB = sdk.opens[22];
    sdk.failSubscribe.clear();
    ASSERT_TRUE(source->restartDevice("SDK-A", 100));
    EXPECT_EQ(sdk.opens[22], opensB); EXPECT_EQ(sdk.subscribes[11], 2u);
    sdk.tracking(11, 1, 1000, {hand(10)}); sdk.tracking(22, 1, 1000, {hand(20)});
    events = source->poll(101);
    EXPECT_EQ(count(events, TrackingEventKind::Frame), 2u);
}

TEST_F(LeapCTrackingSourceTest, RepeatedSdkSubscriptionFailuresRespectRuntimeBackoff) {
    ConfigManager config;
    EngineRuntime runtime(*source, config, [](const OscMessage&) { return true; });
    sdk.failSubscribe.insert(11);
    runtime.start(0);
    sdk.serviceConnected();
    runtime.tick(0);
    EXPECT_EQ(runtime.devices().at("SDK-A").reason, "subscription_failed");
    std::vector<Millis> attempts;
    for (Millis now = 1; now <= 2600; ++now) {
        const auto before = sdk.subscribes[11];
        if (now % 100 == 0) sdk.tracking(22, now, now * 1000);
        if (now == 2600) sdk.failSubscribe.clear();
        runtime.tick(now);
        if (sdk.subscribes[11] > before) attempts.push_back(now);
    }
    EXPECT_EQ(attempts, (std::vector<Millis>{100, 200, 400, 800, 1600, 2600}));
    sdk.tracking(11, 1, 2601000);
    runtime.tick(2601);
    EXPECT_TRUE(runtime.devices().at("SDK-A").framesFresh);
    EXPECT_TRUE(runtime.devices().at("SDK-B").framesFresh);
    EXPECT_EQ(sdk.subscribes[22], 1u);
}

TEST_F(LeapCTrackingSourceTest, ManualRestartReallyReplacesOnlyTheRequestedSubscribedHandle) {
    connect();
    const auto previous = std::find_if(sdk.liveDevices.begin(), sdk.liveDevices.end(), [](const auto& pair) {
        return pair.second == 11;
    })->first;
    ASSERT_TRUE(source->restartDevice("SDK-A", 50));
    EXPECT_EQ(sdk.unsubscribes[11], 1u); EXPECT_EQ(sdk.closes[11], 1u);
    EXPECT_EQ(sdk.opens[11], 2u); EXPECT_EQ(sdk.subscribes[11], 2u);
    EXPECT_FALSE(sdk.liveDevices.contains(previous));
    EXPECT_EQ(sdk.opens[22], 1u); EXPECT_EQ(sdk.subscribes[22], 1u);
    EXPECT_EQ(sdk.closes[22], 0u); EXPECT_EQ(sdk.unsubscribes[22], 0u);
    sdk.tracking(11, 1, 51000, {hand(10)}); sdk.tracking(22, 1, 51000, {hand(20)});
    const auto events = source->poll(51);
    EXPECT_EQ(count(events, TrackingEventKind::Frame), 2u);
}

TEST_F(LeapCTrackingSourceTest, DeviceLossClearsOnlyItsOwnHandleAndDuplicateLossIsIdempotent) {
    connect();
    sdk.present = {22};
    sdk.deviceEvent(11, false); sdk.deviceEvent(11, false);
    sdk.tracking(22, 1, 16000, {hand(20)});
    const auto events = source->poll(16);
    EXPECT_EQ(count(events, TrackingEventKind::DeviceLost, "SDK-A"), 1u);
    EXPECT_EQ(count(events, TrackingEventKind::Frame, "SDK-B"), 1u);
    EXPECT_EQ(sdk.closes[11], 1u); EXPECT_EQ(sdk.unsubscribes[11], 1u);
    EXPECT_EQ(sdk.closes[22], 0u); EXPECT_EQ(sdk.unsubscribes[22], 0u);
    EXPECT_FALSE(source->restartDevice("SDK-A", 17));
}

TEST_F(LeapCTrackingSourceTest, DuplicateDeviceAndSerialEventsDoNotLeakOrCreateDuplicateStreams) {
    connect();
    sdk.deviceEvent(11); sdk.deviceEvent(11);
    sdk.serials[33] = "SDK-A";
    sdk.deviceEvent(33);
    auto events = source->poll(10);
    EXPECT_EQ(sdk.opens[11], 1u);
    EXPECT_EQ(sdk.opens[33], 1u); EXPECT_EQ(sdk.closes[33], 1u);
    EXPECT_EQ(count(events, TrackingEventKind::DeviceConnected), 0u);
    EXPECT_EQ(count(events, TrackingEventKind::Subscribed), 0u);
    sdk.tracking(33, 1, 1000, {hand(30)}); sdk.tracking(11, 1, 1000, {hand(10)});
    events = source->poll(11);
    EXPECT_EQ(count(events, TrackingEventKind::Frame, "SDK-A"), 1u);
}

TEST_F(LeapCTrackingSourceTest, ReusedSdkIdWithNewReferenceReleasesOldHandleAndAnnouncesFreshSubscription) {
    connect();
    sdk.referenceOverrides[11] = reinterpret_cast<void*>(std::uintptr_t(0x4000b));
    sdk.deviceEvent(11);
    const auto events = source->poll(10);
    EXPECT_EQ(count(events, TrackingEventKind::DeviceLost, "SDK-A"), 1u);
    EXPECT_EQ(count(events, TrackingEventKind::DeviceConnected, "SDK-A"), 1u);
    EXPECT_EQ(count(events, TrackingEventKind::Subscribed, "SDK-A"), 1u);
    EXPECT_EQ(sdk.opens[11], 2u); EXPECT_EQ(sdk.closes[11], 1u); EXPECT_EQ(sdk.unsubscribes[11], 1u);
    EXPECT_EQ(sdk.opens[22], 1u); EXPECT_EQ(sdk.closes[22], 0u);
}

TEST_F(LeapCTrackingSourceTest, DuplicateSerialCanRecoverAfterThePreviouslyRetainedDeviceIsRemoved) {
    connect();
    sdk.serials[33] = "SDK-A"; sdk.present = {11, 22, 33};
    sdk.deviceEvent(33);
    source->poll(10);
    EXPECT_EQ(sdk.opens[33], 1u); EXPECT_EQ(sdk.closes[33], 1u);
    EXPECT_EQ(sdk.subscribes[33], 0u);
    sdk.present = {22, 33}; sdk.deviceEvent(11, false);
    const auto lost = source->poll(20);
    EXPECT_EQ(count(lost, TrackingEventKind::DeviceLost, "SDK-A"), 1u);
    const auto recovered = source->poll(1000); // Periodic re-enumeration, no busy-loop retries.
    EXPECT_EQ(count(recovered, TrackingEventKind::DeviceConnected, "SDK-A"), 1u);
    EXPECT_EQ(count(recovered, TrackingEventKind::Subscribed, "SDK-A"), 1u);
    EXPECT_EQ(sdk.opens[33], 2u); EXPECT_EQ(sdk.subscribes[33], 1u);
    EXPECT_EQ(sdk.opens[22], 1u); EXPECT_EQ(sdk.closes[22], 0u);
    sdk.tracking(33, 1, 1001000, {hand(33)});
    const auto frames = source->poll(1001);
    const auto returned = find(frames, TrackingEventKind::Frame, "SDK-A");
    ASSERT_NE(returned, nullptr);
    ASSERT_EQ(returned->frame.hands.size(), 1u);
    EXPECT_FLOAT_EQ(returned->frame.hands.front().palm.position.x, 33);
}

TEST_F(LeapCTrackingSourceTest, ServiceLossReleasesAllOwnedResourcesAndRediscoveryUsesFreshSdkIds) {
    connect();
    sdk.serviceLost();
    auto events = source->poll(10);
    EXPECT_EQ(count(events, TrackingEventKind::ServiceLost), 1u);
    EXPECT_TRUE(sdk.liveConnections.empty()); EXPECT_TRUE(sdk.liveDevices.empty());
    EXPECT_EQ(sdk.destroys, 1u);
    EXPECT_TRUE(source->poll(11).empty());
    EXPECT_EQ(sdk.destroys, 1u);
    sdk.serials[44] = "SDK-A"; sdk.serials[55] = "SDK-B"; sdk.present = {55, 44};
    ASSERT_TRUE(source->start(100)); sdk.serviceConnected();
    events = source->poll(100);
    EXPECT_EQ(count(events, TrackingEventKind::DeviceConnected), 2u);
    sdk.tracking(44, 1, 101000, {hand(44)}); sdk.tracking(55, 1, 101000, {hand(55)});
    events = source->poll(101);
    ASSERT_NE(find(events, TrackingEventKind::Frame, "SDK-A"), nullptr);
    ASSERT_NE(find(events, TrackingEventKind::Frame, "SDK-B"), nullptr);
    EXPECT_EQ(sdk.creates, 2u);
}

TEST_F(LeapCTrackingSourceTest, TimeoutIsNormalButProtocolErrorsCauseOneVisibleServiceLoss) {
    connect();
    EXPECT_TRUE(source->poll(10).empty());
    EXPECT_EQ(source->pollErrors(), 0u);
    sdk.pollFailure(eLeapRS_ProtocolError);
    const auto events = source->poll(11);
    EXPECT_EQ(count(events, TrackingEventKind::ServiceLost), 1u);
    EXPECT_EQ(source->pollErrors(), 1u); EXPECT_FALSE(source->lastError().empty());
    EXPECT_EQ(sdk.destroys, 1u); EXPECT_TRUE(source->poll(12).empty());
}

TEST_F(LeapCTrackingSourceTest, FailedOpenOrSerialReadIsolatedAndRetriedWithoutBusyLoop) {
    sdk.failOpen.insert(11);
    auto events = connect();
    EXPECT_EQ(count(events, TrackingEventKind::Subscribed, "SDK-B"), 1u);
    EXPECT_EQ(count(events, TrackingEventKind::DeviceConnected, "SDK-A"), 0u);
    const auto failedOpens = sdk.opens[11];
    source->poll(99); EXPECT_EQ(sdk.opens[11], failedOpens);
    sdk.failOpen.clear(); sdk.failInfo.insert(11);
    source->poll(100);
    EXPECT_EQ(sdk.opens[11], failedOpens + 1);
    EXPECT_EQ(sdk.closes[11], 1u);
    const auto infoFailureOpens = sdk.opens[11];
    source->poll(299); EXPECT_EQ(sdk.opens[11], infoFailureOpens);
    sdk.failInfo.clear();
    events = source->poll(300);
    EXPECT_EQ(count(events, TrackingEventKind::Subscribed, "SDK-A"), 1u);
    EXPECT_EQ(sdk.opens[22], 1u);
}

TEST_F(LeapCTrackingSourceTest, PollWorkIsBoundedAndStopIsIdempotentWithOrderedCleanup) {
    connect();
    const auto beforePolls = sdk.pollTimeouts.size();
    for (int index = 0; index < 20; ++index) sdk.tracking(11, index + 1, index * 1000, {hand(float(index))});
    const auto events = source->poll(50);
    EXPECT_EQ(sdk.pollTimeouts.size() - beforePolls, 8u);
    EXPECT_EQ(count(events, TrackingEventKind::Frame), 8u);
    EXPECT_EQ(sdk.queued.size(), 12u);
    source->stop(); source->stop();
    EXPECT_EQ(sdk.closes[11], 1u); EXPECT_EQ(sdk.closes[22], 1u);
    EXPECT_EQ(sdk.closesOfConnection, 1u); EXPECT_EQ(sdk.destroys, 1u);
    const auto close = std::find(sdk.calls.begin(), sdk.calls.end(), "close_connection");
    const auto destroy = std::find(sdk.calls.begin(), sdk.calls.end(), "destroy_connection");
    EXPECT_LT(close, destroy);
    EXPECT_LT(std::find(sdk.calls.begin(), sdk.calls.end(), "unsubscribe:11"), close);
    EXPECT_LT(std::find(sdk.calls.begin(), sdk.calls.end(), "close_device:22"), close);
}

TEST_F(LeapCTrackingSourceTest, ConnectionFailuresAndHandshakeDeadlineReleaseResourcesBeforeRetry) {
    sdk.createResult = eLeapRS_InsufficientResources;
    EXPECT_FALSE(source->start(0)); EXPECT_EQ(sdk.destroys, 0u);
    sdk.createResult = eLeapRS_Success; sdk.openResult = eLeapRS_NotConnected;
    EXPECT_FALSE(source->start(100)); EXPECT_EQ(sdk.destroys, 1u);
    sdk.openResult = eLeapRS_Success;
    ASSERT_TRUE(source->start(200));
    source->poll(3199); EXPECT_EQ(sdk.destroys, 1u);
    source->poll(3200); EXPECT_EQ(sdk.destroys, 2u);
    EXPECT_FALSE(source->lastError().empty());
    EXPECT_TRUE(source->start(3300));
    EXPECT_EQ(source->connectionAttempts(), 4u);
}

TEST_F(LeapCTrackingSourceTest, AdapterEventsDriveRuntimeLossZerosWhileOtherCameraKeepsSending) {
    ConfigManager config;
    std::vector<OscMessage> output;
    EngineRuntime runtime(*source, config, [&](const OscMessage& message) { output.push_back(message); return true; });
    runtime.start(0); sdk.serviceConnected();
    sdk.tracking(11, 1, 0, {hand(10)}); sdk.tracking(22, 1, 0, {hand(20)});
    runtime.tick(0); output.clear();
    sdk.present = {22}; sdk.deviceEvent(11, false); sdk.tracking(22, 2, 16000, {hand(21)});
    runtime.tick(16);
    EXPECT_FALSE(runtime.devices().at("SDK-A").present);
    EXPECT_TRUE(runtime.devices().at("SDK-B").framesFresh);
    EXPECT_EQ(runtime.devices().at("SDK-B").framesProcessed, 2u);
    EXPECT_EQ(std::count_if(output.begin(), output.end(), [](const OscMessage& message) {
        return message.address == "/leap/dev1/left/palm/tx" && message.values == std::vector<float>{0};
    }), 1);
    EXPECT_TRUE(std::any_of(output.begin(), output.end(), [](const OscMessage& message) {
        return message.address == "/leap/dev2/left/palm/tx" && message.values == std::vector<float>{21};
    }));
    runtime.stop();
}

TEST_F(LeapCTrackingSourceTest, RuntimeStaleRecoveryCyclesOnlyTheStalledCameraAndAcceptsRestartedFrameIds) {
    ConfigManager config;
    EngineRuntime runtime(*source, config, [](const OscMessage&) { return true; });
    runtime.start(0); sdk.serviceConnected();
    sdk.tracking(11, 1, 0, {hand(10)}); sdk.tracking(22, 1, 0, {hand(20)});
    runtime.tick(0);
    sdk.tracking(22, 2, 500000, {hand(21)}); runtime.tick(500);
    EXPECT_FALSE(runtime.devices().at("SDK-A").framesFresh);
    EXPECT_TRUE(runtime.devices().at("SDK-B").framesFresh);
    EXPECT_EQ(sdk.closes[11], 0u); // Recovery waits for the bounded runtime delay.
    sdk.tracking(22, 3, 600000, {hand(22)}); runtime.tick(600);
    EXPECT_EQ(sdk.unsubscribes[11], 1u); EXPECT_EQ(sdk.closes[11], 1u);
    EXPECT_EQ(sdk.opens[11], 2u); EXPECT_EQ(sdk.subscribes[11], 2u);
    EXPECT_EQ(sdk.opens[22], 1u); EXPECT_EQ(sdk.closes[22], 0u);
    sdk.tracking(11, 1, 601000, {hand(11)}); sdk.tracking(22, 4, 601000, {hand(23)});
    runtime.tick(601);
    EXPECT_TRUE(runtime.devices().at("SDK-A").framesFresh);
    EXPECT_EQ(runtime.devices().at("SDK-A").framesProcessed, 2u);
    EXPECT_EQ(runtime.devices().at("SDK-B").framesProcessed, 4u);
    EXPECT_EQ(runtime.devices().at("SDK-B").reconnectAttempts, 0u);
}

TEST_F(LeapCTrackingSourceTest, IncompleteApiTableIsRejectedBeforeAllocatingSdkResources) {
    auto incomplete = sdk.api(); incomplete.pollConnection = nullptr;
    EXPECT_THROW((void)LeapCTrackingSource{incomplete}, std::invalid_argument);
    incomplete = sdk.api(); incomplete.getVersion = nullptr;
    EXPECT_THROW((void)LeapCTrackingSource{incomplete}, std::invalid_argument);
    EXPECT_EQ(sdk.creates, 0u);
}
}
