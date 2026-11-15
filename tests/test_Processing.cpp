#include <gtest/gtest.h>
#include "runtime/Processing.hpp"
#include "runtime/EngineRuntime.hpp"
#include <algorithm>
#include <cmath>
using namespace handbridge;
namespace {
FrameData sample(float x = 10, std::string side = "left") {
    FrameData frame; frame.deviceId = "A";
    HandData hand; hand.handType = std::move(side); hand.palm.position = {x,20,30};
    hand.palm.velocity = {1,0,0}; hand.palm.normal = {1,0,0}; hand.palm.direction = {1,0,0};
    hand.arm.wristPosition = {4,5,6}; hand.fingers.resize(1); hand.fingers[0].bones.resize(4);
    hand.fingers[0].bones[3].nextJoint = {7,8,9}; frame.hands.push_back(hand); return frame;
}
class Source : public ITrackingSource {
public:
    std::vector<TrackingEvent> events;
    unsigned restarts = 0;
    bool start(Millis) override { return true; }
    std::vector<TrackingEvent> poll(Millis) override { auto next = std::move(events); events.clear(); return next; }
    bool restartDevice(const std::string&,Millis) override { ++restarts; return true; }
    void stop() noexcept override {}
};
class SettingsRuntime : public testing::Test {
protected:
    ConfigManager config; Source source; std::vector<OscMessage> sent;
    EngineRuntime runtime{source,config,[this](const OscMessage& message) { sent.push_back(message); return true; }};
    void SetUp() override {
        runtime.start(0);
        source.events = {{TrackingEventKind::ServiceConnected},{TrackingEventKind::DeviceConnected,"A"},
                         {TrackingEventKind::Subscribed,"A"}};
        runtime.tick(0);
    }
    void frame(std::uint64_t id,Millis time,float x = 10) { source.events = {{TrackingEventKind::Frame,"A",id,time,sample(x)}}; runtime.tick(time); }
    std::vector<float> values(const std::string& address) const {
        std::vector<float> result;
        for (const auto& message : sent) if (message.address == address) result.push_back(message.values.at(0));
        return result;
    }
};
}
TEST(Processing, BypassIsExactAndDoesNotApplyHiddenUnitsOrTransforms) {
    ProcessingPipeline pipeline; ProcessingSettings p; p.outputUnits = "m"; p.offsetMm = {100,0,0}; p.smoothingMs = 100;
    pipeline.setSettings(p); const auto raw = sample(); const auto processed = pipeline.process(raw,0);
    EXPECT_FLOAT_EQ(processed.hands[0].palm.position.x,10); EXPECT_FLOAT_EQ(processed.hands[0].palm.velocity.x,1);
}
TEST(Processing, TransformOrderAndUnitsAlsoAffectWristTipsVelocityNormalsAndOrientation) {
    ProcessingPipeline pipeline; ProcessingSettings p; p.bypass = false; p.scale = {2,3,4};
    p.rotationDeg = {0,0,90}; p.offsetMm = {100,200,300}; p.outputUnits = "m"; pipeline.setSettings(p);
    const auto out = pipeline.process(sample(),0).hands.at(0);
    EXPECT_NEAR(out.palm.position.x,.04,1e-6); EXPECT_NEAR(out.palm.position.y,.22,1e-6); EXPECT_NEAR(out.palm.position.z,.42,1e-6);
    EXPECT_NEAR(out.arm.wristPosition.x,.085,1e-6); EXPECT_NEAR(out.fingers[0].bones[3].nextJoint.x,.076,1e-6);
    EXPECT_NEAR(out.palm.velocity.x,0,1e-6); EXPECT_NEAR(out.palm.velocity.y,.002,1e-6);
    EXPECT_NEAR(out.palm.normal.x,0,1e-6); EXPECT_NEAR(out.palm.normal.y,1,1e-6);
    EXPECT_NEAR(out.palm.orientation.w,std::sqrt(.5),1e-6); EXPECT_NEAR(out.palm.orientation.z,std::sqrt(.5),1e-6);
}
TEST(Processing, TimeBasedSmoothingDeadzoneIsolationAndLossReset) {
    ProcessingPipeline pipeline; ProcessingSettings p; p.bypass = false; p.smoothingMs = 100; p.deadzoneMm = 2; pipeline.setSettings(p);
    EXPECT_FLOAT_EQ(pipeline.process(sample(10),0).hands[0].palm.position.x,10);
    EXPECT_FLOAT_EQ(pipeline.process(sample(11),100).hands[0].palm.position.x,10);
    EXPECT_NEAR(pipeline.process(sample(110),200).hands[0].palm.position.x,10+100*(1-std::exp(-1.)),1e-4);
    auto b = sample(500); b.deviceId = "B";
    EXPECT_FLOAT_EQ(pipeline.process(b,200).hands[0].palm.position.x,500);
    FrameData empty; empty.deviceId = "A"; pipeline.process(empty,201);
    EXPECT_FLOAT_EQ(pipeline.process(sample(900),202).hands[0].palm.position.x,900);
}
TEST(Processing, GestureHysteresisAndHandLossAreExplicitWithoutChangingNativeStrength) {
    ProcessingPipeline pipeline; ProcessingSettings p; p.gesturesEnabled = true; pipeline.setSettings(p);
    auto input = sample(); input.hands[0].pinchStrength = .9f;
    auto raw = pipeline.process(input,0); EXPECT_FLOAT_EQ(raw.hands[0].pinchStrength,.9f);
    EXPECT_FLOAT_EQ(pipeline.gestures(raw,"dev1")[0].values[0],1);
    input.hands[0].pinchStrength = .7f; EXPECT_FLOAT_EQ(pipeline.gestures(input,"dev1")[0].values[0],1);
    input.hands[0].pinchStrength = .5f; EXPECT_FLOAT_EQ(pipeline.gestures(input,"dev1")[0].values[0],0);
    input.hands.clear(); auto loss = pipeline.gestures(input,"dev1"); ASSERT_EQ(loss.size(),2u);
    EXPECT_EQ(loss[0].address,"/leap/dev1/left/pinchActive"); EXPECT_FLOAT_EQ(loss[0].values[0],0);
    EXPECT_TRUE(pipeline.gestures(input,"dev1").empty());
}
TEST(Processing, NewNativeHandIdentityResetsOnlyItsOwnFiltersAndGestureHysteresis) {
    ProcessingPipeline pipeline;
    ProcessingSettings settings;
    settings.bypass = false;
    settings.smoothingMs = 100;
    settings.deadzoneMm = 20;
    settings.gesturesEnabled = true;
    pipeline.setSettings(settings);
    auto a = sample(10);
    a.hands[0].nativeHandId = 10;
    a.hands[0].pinchStrength = a.hands[0].grabStrength = .9f;
    auto right = sample(100, "right").hands[0];
    right.nativeHandId = 20;
    right.pinchStrength = right.grabStrength = .9f;
    a.hands.push_back(right);
    auto b = sample(500);
    b.deviceId = "B";
    b.hands[0].nativeHandId = 30;
    b.hands[0].pinchStrength = b.hands[0].grabStrength = .9f;
    pipeline.gestures(pipeline.process(a, 0), "dev1");
    pipeline.gestures(pipeline.process(b, 0), "dev2");

    // The replacement hand arrives without an empty frame. Its old palm would
    // be blended by smoothing and its tip held by the prior hand's dead zone.
    a.hands[0].nativeHandId = 11;
    a.hands[0].palm.position.x = 1000;
    a.hands[0].fingers[0].bones[3].nextJoint.x = 8;
    a.hands[1].palm.position.x = 200;
    for (auto& hand : a.hands) hand.pinchStrength = hand.grabStrength = .7f;
    b.hands[0].palm.position.x = 600;
    b.hands[0].pinchStrength = b.hands[0].grabStrength = .7f;
    const auto outA = pipeline.process(a, 100);
    const auto outB = pipeline.process(b, 100);
    const auto alpha = 1 - std::exp(-1.);
    EXPECT_FLOAT_EQ(outA.hands[0].palm.position.x, 1000);
    EXPECT_FLOAT_EQ(outA.hands[0].fingers[0].bones[3].nextJoint.x, 8);
    EXPECT_NEAR(outA.hands[1].palm.position.x, 100 + 100 * alpha, 1e-4);
    EXPECT_NEAR(outB.hands[0].palm.position.x, 500 + 100 * alpha, 1e-4);
    const auto gesturesA = pipeline.gestures(outA, "dev1");
    const auto gesturesB = pipeline.gestures(outB, "dev2");
    ASSERT_EQ(gesturesA.size(), 4u);
    EXPECT_EQ(gesturesA[0].address, "/leap/dev1/left/pinchActive");
    EXPECT_FLOAT_EQ(gesturesA[0].values[0], 0);
    EXPECT_FLOAT_EQ(gesturesA[1].values[0], 0);
    EXPECT_EQ(gesturesA[2].address, "/leap/dev1/right/pinchActive");
    EXPECT_FLOAT_EQ(gesturesA[2].values[0], 1);
    EXPECT_FLOAT_EQ(gesturesA[3].values[0], 1);
    ASSERT_EQ(gesturesB.size(), 2u);
    EXPECT_FLOAT_EQ(gesturesB[0].values[0], 1);
    EXPECT_FLOAT_EQ(gesturesB[1].values[0], 1);

    // The same new identity keeps normal time-based filter state afterwards.
    a.hands[0].palm.position.x = 1100;
    EXPECT_NEAR(pipeline.process(a, 200).hands[0].palm.position.x, 1000 + 100 * alpha, 1e-4);
}
TEST(Processing, KnownNativeIdentityChangeResetsGestureEntryEvenWithRawBypass) {
    ProcessingPipeline pipeline;
    ProcessingSettings settings;
    settings.gesturesEnabled = true;
    pipeline.setSettings(settings);
    auto input = sample();
    input.hands[0].nativeHandId = 1;
    input.hands[0].pinchStrength = input.hands[0].grabStrength = .9f;
    const auto before = pipeline.gestures(pipeline.process(input, 0), "dev1");
    ASSERT_EQ(before.size(), 2u);
    EXPECT_FLOAT_EQ(before[0].values[0], 1);
    input.hands[0].nativeHandId = 2;
    input.hands[0].pinchStrength = input.hands[0].grabStrength = .7f;
    const auto after = pipeline.gestures(pipeline.process(input, 1), "dev1");
    ASSERT_EQ(after.size(), 2u);
    EXPECT_FLOAT_EQ(after[0].values[0], 0);
    EXPECT_FLOAT_EQ(after[1].values[0], 0);
    EXPECT_FLOAT_EQ(input.hands[0].pinchStrength, .7f);
}
TEST_F(SettingsRuntime, EverySettingFamilyChangesRealMessagesAndPreview) {
    frame(1,0); auto settings = runtime.settings(); settings.processing.bypass = false;
    settings.processing.scale = {2,1,1}; settings.processing.offsetMm = {5,0,0}; settings.channels.sendWrist = false;
    settings.processing.gesturesEnabled = true; runtime.applySettings(settings); sent.clear(); frame(2,16);
    ASSERT_EQ(values("/leap/dev1/left/palm/tx").size(),1u); EXPECT_FLOAT_EQ(values("/leap/dev1/left/palm/tx")[0],25);
    EXPECT_TRUE(values("/leap/dev1/left/wrist/tx").empty()); EXPECT_FALSE(values("/leap/dev1/left/pinchActive").empty());
    EXPECT_FLOAT_EQ(runtime.previews().at("A").raw.hands[0].palm.position.x,10);
    EXPECT_FLOAT_EQ(runtime.previews().at("A").processed.hands[0].palm.position.x,25);
    settings.osc.enabled = false; runtime.applySettings(settings); sent.clear(); frame(3,32); EXPECT_TRUE(sent.empty());
    EXPECT_TRUE(runtime.devices().at("A").framesFresh);
}
TEST_F(SettingsRuntime, RateLimitNeverDelaysHandLossAndAliasesClearAtOldAddress) {
    auto settings = runtime.settings(); settings.osc.rateHz = 1; runtime.applySettings(settings);
    frame(1,0); sent.clear(); frame(2,16); EXPECT_TRUE(sent.empty());
    auto empty = sample(); empty.hands.clear(); source.events = {{TrackingEventKind::Frame,"A",3,17,empty}}; runtime.tick(17);
    EXPECT_EQ(values("/leap/dev1/left/palm/tx"),(std::vector<float>{0}));
    sent.clear(); frame(4,18); // Reacquisition is rate-limited until next output interval.
    settings.devices["A"].alias = "dev7"; runtime.applySettings(settings); sent.clear(); frame(5,32);
    EXPECT_FALSE(values("/leap/dev7/left/palm/tx").empty());
    sent.clear(); settings.devices["A"].alias = "dev8"; runtime.applySettings(settings);
    EXPECT_EQ(values("/leap/dev7/left/palm/tx"),(std::vector<float>{0})); sent.clear(); frame(6,48);
    EXPECT_FALSE(values("/leap/dev8/left/palm/tx").empty());
}
TEST_F(SettingsRuntime, DisableAndHandFilterAreOutputControlsNotDeviceFailure) {
    frame(1,0); auto s = runtime.settings(); s.devices["A"].enabled = false; runtime.applySettings(s);
    EXPECT_EQ(values("/leap/dev1/left/palm/tx").back(),0); sent.clear(); frame(2,16);
    EXPECT_TRUE(sent.empty()); EXPECT_TRUE(runtime.devices().at("A").present); EXPECT_TRUE(runtime.devices().at("A").framesFresh);
    s.devices["A"].enabled = true; s.devices["A"].handFilter = "right"; runtime.applySettings(s); sent.clear(); frame(3,32);
    EXPECT_TRUE(sent.empty());
}
TEST_F(SettingsRuntime, InvalidApplyIsTransactionalAndManualRestartReallyCallsSource) {
    frame(1,0); const auto before = runtime.settings().toJson(); auto invalid = runtime.settings(); invalid.osc.port = 0;
    const auto count = sent.size(); EXPECT_THROW(runtime.applySettings(invalid),std::invalid_argument);
    EXPECT_EQ(runtime.settings().toJson(),before); EXPECT_EQ(sent.size(),count);
    EXPECT_TRUE(runtime.restartDevice("A",1)); EXPECT_EQ(source.restarts,1u); EXPECT_FALSE(runtime.devices().at("A").framesFresh);
    EXPECT_FALSE(runtime.restartDevice("absent",1));
}
TEST_F(SettingsRuntime, ProcessingApplyWithoutAnotherFrameStillClearsPoseWhenStale) {
    frame(1,0); auto s = runtime.settings(); s.processing.bypass = false; s.processing.smoothingMs = 50;
    runtime.applySettings(s); sent.clear(); runtime.tick(500);
    EXPECT_EQ(values("/leap/dev1/left/palm/tx"),(std::vector<float>{0}));
}

TEST(Processing, InvalidJointGapResetsItsFilterWithoutResettingTheOtherValidJoints) {
    ProcessingPipeline pipeline; ProcessingSettings p; p.bypass = false; p.smoothingMs = 100;
    pipeline.setSettings(p); auto input = sample(); pipeline.process(input,0);
    input.hands[0].fingers[0].valid = false; input.hands[0].palm.position.x = 110;
    const auto intermediate = pipeline.process(input,100);
    EXPECT_GT(intermediate.hands[0].palm.position.x,10);
    input.hands[0].fingers[0].valid = true;
    input.hands[0].fingers[0].bones[3].nextJoint.x = 1000;
    const auto fresh = pipeline.process(input,101);
    EXPECT_FLOAT_EQ(fresh.hands[0].fingers[0].bones[3].nextJoint.x,1000);
    EXPECT_LT(fresh.hands[0].palm.position.x,110);
}
TEST(SettingsOutputHealth, FailedLossSendDuringApplyIsImmediatelyVisible) {
    ConfigManager config; Source source; bool works = true;
    EngineRuntime runtime(source,config,[&](const OscMessage&) { return works; }); runtime.start(0);
    source.events = {{TrackingEventKind::ServiceConnected},{TrackingEventKind::DeviceConnected,"A"},
        {TrackingEventKind::Subscribed,"A"},{TrackingEventKind::Frame,"A",1,0,sample()}};
    runtime.tick(0); works = false;
    auto s = runtime.settings(); s.osc.enabled = false; runtime.applySettings(s);
    EXPECT_GT(runtime.outputStatus().errors,0u); EXPECT_TRUE(runtime.outputStatus().degraded);
}
