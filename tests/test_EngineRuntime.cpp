#include <gtest/gtest.h>
#include "runtime/EngineRuntime.hpp"
#include "runtime/MockTrackingSource.hpp"
#include <algorithm>
#include <memory>

using namespace handbridge;
namespace {
class ScriptedSource : public ITrackingSource {
public:
    std::vector<TrackingEvent> ready;
    std::vector<Millis> restartTimes;
    unsigned starts = 0, stops = 0;
    bool restartSucceeds = true;
    bool start(Millis) override { ++starts; return true; }
    std::vector<TrackingEvent> poll(Millis) override { auto result = std::move(ready); ready.clear(); return result; }
    bool restartDevice(const std::string&, Millis now) override { restartTimes.push_back(now); return restartSucceeds; }
    void stop() noexcept override { ++stops; }
};
TrackingEvent signal(TrackingEventKind kind, const std::string& serial = "") { return {kind, serial}; }
TrackingEvent frame(const std::string& serial, std::uint64_t id, Millis now, bool hands = true) {
    FrameData sample; sample.deviceId = serial; sample.timestamp = now * 1000;
    if (hands) { HandData hand; hand.handType = "left"; hand.palm.position = {10,20,30}; sample.hands.push_back(hand); }
    return {TrackingEventKind::Frame, serial, id, now, sample};
}
class RuntimeTest : public testing::Test {
protected:
    ConfigManager config;
    ScriptedSource source;
    std::vector<OscMessage> messages;
    std::vector<std::string> logs;
    bool outputWorks = true;
    std::unique_ptr<EngineRuntime> runtime;
    void SetUp() override {
        runtime = std::make_unique<EngineRuntime>(source, config,
            [this](const OscMessage& message) { messages.push_back(message); return outputWorks; }, RuntimeOptions{},
            [this](const std::string& text) { logs.push_back(text); });
        runtime->start(0);
    }
    void TearDown() override { runtime->stop(); }
    void connect(bool second = true) {
        source.ready = {signal(TrackingEventKind::ServiceConnected), signal(TrackingEventKind::DeviceConnected, "A"),
                        signal(TrackingEventKind::Subscribed, "A")};
        if (second) {
            source.ready.push_back(signal(TrackingEventKind::DeviceConnected, "B"));
            source.ready.push_back(signal(TrackingEventKind::Subscribed, "B"));
        }
        runtime->tick(0);
    }
    unsigned zeros(const std::string& alias) const {
        return static_cast<unsigned>(std::count_if(messages.begin(), messages.end(), [&](const OscMessage& msg) {
            return msg.address == "/leap/" + alias + "/left/palm/tx" && msg.values.at(0) == 0.f;
        }));
    }
};
TEST_F(RuntimeTest, StaleAEmitsLossOnceWhileBContinuesAndRecovers) {
    connect(); source.ready = {frame("A",1,0),frame("B",1,0)}; runtime->tick(0); messages.clear();
    source.ready = {frame("B",2,500)}; runtime->tick(500);
    EXPECT_FALSE(runtime->devices().at("A").framesFresh);
    EXPECT_EQ(runtime->devices().at("A").reason, "frame_stale");
    EXPECT_TRUE(runtime->devices().at("B").framesFresh);
    EXPECT_EQ(zeros("dev1"), 1u);
    source.ready = {frame("B",3,599)}; runtime->tick(599);
    EXPECT_TRUE(source.restartTimes.empty()); EXPECT_EQ(zeros("dev1"), 1u);
    runtime->tick(600); ASSERT_EQ(source.restartTimes.size(), 1u);
    source.ready = {frame("A",1,601),frame("B",4,601)}; runtime->tick(601);
    EXPECT_EQ(runtime->devices().at("A").state, DeviceState::Streaming);
    EXPECT_EQ(runtime->devices().at("A").framesProcessed, 2u);
    EXPECT_EQ(runtime->devices().at("B").framesProcessed, 4u);
}
TEST_F(RuntimeTest, EmptyFramesAreHealthyAndNoHandsIsNotFailure) {
    connect(false);
    for (unsigned id = 1; id <= 20; ++id) {
        source.ready = {frame("A",id,id*100,false)}; runtime->tick(id*100);
    }
    const auto& status = runtime->devices().at("A");
    EXPECT_TRUE(status.framesFresh); EXPECT_FALSE(status.handsVisible);
    EXPECT_EQ(status.framesProcessed, 20u); EXPECT_EQ(status.reconnectAttempts, 0u);
    EXPECT_TRUE(messages.empty());
}
TEST_F(RuntimeTest, LatestFrameMailboxCountsOverwritesAndRejectsDuplicateOrOldFrames) {
    connect(false); source.ready = {frame("A",1,0),frame("A",2,0),frame("A",3,0)}; runtime->tick(0);
    const auto& status = runtime->devices().at("A");
    EXPECT_EQ(status.framesIn, 3u); EXPECT_EQ(status.framesProcessed,1u); EXPECT_EQ(status.queueDrops,2u);
    source.ready = {signal(TrackingEventKind::DeviceConnected,"A"),signal(TrackingEventKind::Subscribed,"A"),
                    frame("A",3,100),frame("A",2,100)}; runtime->tick(100);
    EXPECT_EQ(runtime->devices().size(),1u); EXPECT_EQ(status.duplicateFrames,2u); EXPECT_EQ(status.framesProcessed,1u);
    source.ready = {frame("A",4,1)}; runtime->tick(501); // Old queued sample must not restore a frozen pose.
    EXPECT_EQ(status.queueDrops,3u); EXPECT_FALSE(status.framesFresh);
}
TEST_F(RuntimeTest, SubscriptionFailureRetriesWithCappedBackoff) {
    source.restartSucceeds = false;
    source.ready = {signal(TrackingEventKind::ServiceConnected),signal(TrackingEventKind::DeviceConnected,"A"),
                    signal(TrackingEventKind::SubscriptionFailed,"A")}; runtime->tick(0);
    for (auto now : {99u,100u,199u,200u,399u,400u,799u,800u,1599u,1600u,2599u,2600u}) runtime->tick(now);
    EXPECT_EQ(source.restartTimes, (std::vector<Millis>{100,200,400,800,1600,2600}));
    EXPECT_FALSE(runtime->devices().at("A").subscribed);
    EXPECT_EQ(runtime->devices().at("A").reason,"retry_failed");
    EXPECT_LT(logs.size(),5u); // Repeated retry failure is a single meaningful transition.
}
TEST_F(RuntimeTest, ServiceRecoveryRequiresRediscoveryAndDoesNotInventMissingDevice) {
    connect(); source.ready = {frame("A",1,0),frame("B",1,0)}; runtime->tick(0); messages.clear();
    source.ready = {signal(TrackingEventKind::ServiceLost)}; runtime->tick(1);
    EXPECT_FALSE(runtime->serviceConnected()); EXPECT_EQ(zeros("dev1"),1u); EXPECT_EQ(zeros("dev2"),1u);
    EXPECT_FALSE(runtime->devices().at("A").present);
    runtime->tick(100); EXPECT_EQ(source.starts,1u);
    runtime->tick(101); EXPECT_EQ(source.starts,2u);
    source.ready = {signal(TrackingEventKind::ServiceConnected),signal(TrackingEventKind::DeviceConnected,"B"),
                    signal(TrackingEventKind::Subscribed,"B"),frame("B",1,102),frame("A",2,102)};
    runtime->tick(102);
    EXPECT_FALSE(runtime->devices().at("A").present); EXPECT_FALSE(runtime->devices().at("A").framesFresh);
    EXPECT_TRUE(runtime->devices().at("B").framesFresh);
    EXPECT_EQ(runtime->devices().at("B").alias,"dev2");
}
TEST_F(RuntimeTest, DeviceLossAndOneHundredReattachmentsKeepAliasAndOtherStream) {
    connect(); source.ready = {frame("A",1,0),frame("B",1,0)}; runtime->tick(0);
    for (unsigned cycle = 1; cycle <= 100; ++cycle) {
        const Millis now = cycle * 2;
        source.ready = {signal(TrackingEventKind::DeviceLost,"A"),frame("B",cycle+1,now)}; runtime->tick(now);
        EXPECT_FALSE(runtime->devices().at("A").framesFresh); EXPECT_TRUE(runtime->devices().at("B").framesFresh);
        source.ready = {signal(TrackingEventKind::DeviceConnected,"A"),signal(TrackingEventKind::Subscribed,"A"),frame("A",1,now+1)};
        runtime->tick(now+1);
        EXPECT_EQ(runtime->devices().at("A").alias,"dev1");
    }
    EXPECT_EQ(runtime->devices().size(),2u); EXPECT_EQ(runtime->devices().at("B").framesProcessed,101u);
}
TEST_F(RuntimeTest, OutputFailureIsVisibleAndDoesNotStopTrackingOrLaterSends) {
    connect(); outputWorks = false;
    source.ready = {frame("A",1,0),frame("B",1,0)}; runtime->tick(0);
    const auto errors = runtime->outputStatus().errors;
    EXPECT_GT(errors,0u); EXPECT_TRUE(runtime->outputStatus().degraded);
    EXPECT_EQ(std::count(logs.begin(),logs.end(),"output state=degraded reason=send_failed"),1);
    outputWorks = true; source.ready = {frame("A",2,250),frame("B",2,250)}; runtime->tick(250);
    EXPECT_EQ(runtime->outputStatus().errors, errors); EXPECT_FALSE(runtime->outputStatus().degraded);
    EXPECT_GT(runtime->outputStatus().successes,0u);
    EXPECT_EQ(runtime->devices().at("A").framesProcessed,2u); EXPECT_EQ(runtime->devices().at("B").framesProcessed,2u);
    EXPECT_EQ(std::count(logs.begin(),logs.end(),"output state=recovered"),1);
}
TEST_F(RuntimeTest, OutputDoesNotRecoverWithoutAnotherSuccessfulSend) {
    connect(false); outputWorks = false;
    source.ready = {frame("A",1,0)}; runtime->tick(0);
    outputWorks = true;
    runtime->tick(250); // No outgoing attempt; silence must not become a success.
    EXPECT_TRUE(runtime->outputStatus().degraded);
    EXPECT_EQ(std::count(logs.begin(),logs.end(),"output state=recovered"),0);
    source.ready = {frame("A",2,251)}; runtime->tick(251);
    EXPECT_FALSE(runtime->outputStatus().degraded);
}
TEST_F(RuntimeTest, UnacknowledgedServiceStartUsesCappedBackoff) {
    for (auto now : {99u,100u,299u,300u,699u,700u,1499u,1500u,2499u,2500u}) runtime->tick(now);
    EXPECT_FALSE(runtime->serviceConnected());
    EXPECT_EQ(source.starts,6u); // Initial + 100,300,700,1500,2500; no busy-wait restart loop.
    EXPECT_TRUE(runtime->devices().empty());
}
TEST_F(RuntimeTest, StopStartClearsPendingFramesAndKeepsPersistentAliases) {
    connect(); source.ready = {frame("A",1,0)}; runtime->tick(0);
    runtime->stop(); EXPECT_FALSE(runtime->serviceConnected());
    runtime->start(100);
    source.ready = {signal(TrackingEventKind::ServiceConnected),signal(TrackingEventKind::DeviceConnected,"A"),
                    signal(TrackingEventKind::Subscribed,"A"),frame("A",1,100)};
    runtime->tick(100);
    EXPECT_EQ(runtime->devices().at("A").alias,"dev1"); EXPECT_EQ(runtime->devices().at("A").framesProcessed,1u);
}
TEST_F(RuntimeTest, HandAssignmentAndChannelFiltersAffectRealMessages) {
    runtime->stop(); config.setDefaultHandAssignment("A","RIGHT"); config.setSendWristEnabled(false);
    runtime = std::make_unique<EngineRuntime>(source, config,[this](const OscMessage& m) { messages.push_back(m); return true; });
    runtime->start(0); connect(false);
    source.ready = {frame("A",1,0)}; runtime->tick(0); EXPECT_TRUE(messages.empty());
    auto right = frame("A",2,1); right.frame.hands[0].handType = "right";
    source.ready = {right}; runtime->tick(1);
    EXPECT_FALSE(messages.empty());
    EXPECT_TRUE(std::all_of(messages.begin(),messages.end(),[](const OscMessage& m) {
        return m.address.find("/right/") != std::string::npos && m.address.find("/wrist/") == std::string::npos;
    }));
}
TEST_F(RuntimeTest, InvalidClockAndOptionsRejected) {
    connect(false); runtime->tick(10); EXPECT_THROW(runtime->tick(9),std::invalid_argument);
    EXPECT_THROW(EngineRuntime(source,config,[](const OscMessage&) { return true; },RuntimeOptions{0,1,1}),std::invalid_argument);
    config.setDefaultHandAssignment("A","maybe");
    EXPECT_THROW(EngineRuntime(source,config,[](const OscMessage&) { return true; }),std::invalid_argument);
    config.setDefaultHandAssignment("A","");
}
TEST(MockTrackingSource, DeterministicTwoCameraFramesAndBoundedPoll) {
    MockTrackingSource first, second;
    first.start(0); second.start(0);
    for (auto now : {0u,16u,32u,1000000u}) {
        const auto a = first.poll(now), b = second.poll(now);
        ASSERT_EQ(a.size(),b.size()); EXPECT_LE(a.size(),7u);
        for (std::size_t index = 0; index < a.size(); ++index) {
            EXPECT_EQ(a[index].serial,b[index].serial); EXPECT_EQ(a[index].frameId,b[index].frameId);
            EXPECT_EQ(a[index].arrivalMs,b[index].arrivalMs);
        }
    }
}
}
