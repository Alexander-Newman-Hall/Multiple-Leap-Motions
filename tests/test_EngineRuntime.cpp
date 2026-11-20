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
    static TrackingEvent observed(const std::string& serial, Millis time,
                                  std::initializer_list<std::pair<std::string, float>> palms) {
        auto observation = frame(serial, time, time, false);
        for (const auto& [side, palmX] : palms) {
            HandData hand;
            hand.handType = side;
            hand.nativeHandId = side == "left" ? 7 : 8;
            hand.palm.position.x = palmX;
            observation.frame.hands.push_back(hand);
        }
        return observation;
    }
    void checkPalms(const std::map<std::string, std::optional<float>>& expected) {
        std::map<std::string, float> expectedScalars, scalars;
        std::map<std::string, std::optional<float>> expectedSkeletons, skeletons;
        for (const auto& [route, palmX] : expected) {
            expectedScalars["/leap/" + route + "/palm/tx"] = palmX.value_or(0);
            expectedSkeletons["/handbridge/v2/" + route + "/skeleton"] = palmX;
        }
        for (const auto& message : messages) {
            if (message.address.ends_with("/palm/tx")) {
                EXPECT_TRUE(scalars.emplace(message.address, message.values.at(0)).second);
            } else if (message.address.ends_with("/skeleton")) {
                const auto& args = message.arguments;
                EXPECT_EQ(std::get<std::int64_t>(args.at(3)), message.address.ends_with("/left/skeleton") ? 7 : 8);
                std::optional<float> palmX;
                if (std::get<std::int32_t>(args.at(0)) == 1) palmX = std::get<float>(args.at(11));
                EXPECT_TRUE(skeletons.emplace(message.address, palmX).second);
            }
        }
        EXPECT_EQ(scalars, expectedScalars);
        EXPECT_EQ(skeletons, expectedSkeletons);
        messages.clear();
    }
    unsigned zeros(const std::string& alias) const {
        return static_cast<unsigned>(std::count_if(messages.begin(), messages.end(), [&](const OscMessage& msg) {
            return msg.address == "/leap/" + alias + "/left/palm/tx" && msg.values.at(0) == 0.f;
        }));
    }
};
TEST_F(RuntimeTest, SlowFirstFrameGetsTimeToArriveThenStreamingWatchdogApplies) {
    connect(false);
    runtime->tick(500);
    runtime->tick(600);
    const auto& camera = runtime->devices().at("A");
    EXPECT_EQ(camera.state, DeviceState::Subscribed);
    EXPECT_FALSE(camera.framesFresh);
    EXPECT_TRUE(source.restartTimes.empty());

    source.ready = {frame("A", 0, 878, false)};
    runtime->tick(878);
    ASSERT_TRUE(camera.framesFresh);
    EXPECT_FALSE(camera.handsVisible);
    runtime->tick(1377);
    EXPECT_TRUE(camera.framesFresh);
    runtime->tick(1378);
    EXPECT_FALSE(camera.framesFresh);
    EXPECT_EQ(camera.reason, "frame_stale");
}
TEST_F(RuntimeTest, SilentSubscriptionRetriesAfterDeadlineAndAllowsWarmupAgain) {
    connect();
    for (Millis now = 0; now < 3000; now += 100) {
        source.ready = {frame("B", now, now, false)};
        runtime->tick(now);
    }
    const auto& silent = runtime->devices().at("A");
    const auto& healthy = runtime->devices().at("B");
    runtime->tick(2999);
    EXPECT_EQ(silent.state, DeviceState::Subscribed);
    runtime->tick(3000);
    EXPECT_EQ(silent.reason, "first_frame_timeout");
    runtime->tick(3099);
    EXPECT_TRUE(source.restartTimes.empty());
    runtime->tick(3100);
    EXPECT_EQ(silent.state, DeviceState::Subscribed);
    for (Millis now = 3200; now <= 4000; now += 100) {
        source.ready = {frame("B", now, now, false)};
        runtime->tick(now);
    }
    source.ready = {frame("A", 0, 4001)};
    runtime->tick(4001);
    EXPECT_TRUE(silent.framesFresh);
    EXPECT_TRUE(healthy.framesFresh);
    EXPECT_EQ(source.restartTimes, (std::vector<Millis>{3100}));
    EXPECT_EQ(healthy.reconnectAttempts, 0u);
}
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
TEST_F(RuntimeTest, SameLeftHandIdOnTwoCamerasKeepsIndependentPosesWhenOneDisconnects) {
    connect();
    auto settings = runtime->settings();
    settings.channels.sendFullSkeleton = true;
    runtime->applySettings(settings);

    source.ready = {observed("B", 1, {{"left",200}}), observed("A", 1, {{"left",10}})};
    runtime->tick(1);
    checkPalms({{"dev1/left",10}, {"dev2/left",200}});

    source.ready = {observed("B", 2, {{"left",201}}), signal(TrackingEventKind::DeviceLost, "A")};
    runtime->tick(2);
    checkPalms({{"dev1/left",std::nullopt}, {"dev2/left",201}});

    source.ready = {observed("B", 3, {{"left",202}})};
    runtime->tick(3);
    checkPalms({{"dev2/left",202}});
    EXPECT_TRUE(runtime->devices().at("B").framesFresh);
    EXPECT_EQ(runtime->devices().at("B").reconnectAttempts, 0u);
}
TEST_F(RuntimeTest, SingleCameraEmitsBothHandsAndKeepsSurvivingSideWhenOneLeaves) {
    connect(false);
    auto settings = runtime->settings();
    settings.channels.sendFullSkeleton = true;
    ASSERT_EQ(settings.devices.at("A").handFilter, "both");
    runtime->applySettings(settings);
    source.ready = {observed("A", 1, {{"left",10}})};
    runtime->tick(1);
    checkPalms({{"dev1/left",10}});

    source.ready = {observed("A", 2, {{"left",11}, {"right",200}})};
    runtime->tick(2);
    checkPalms({{"dev1/left",11}, {"dev1/right",200}});

    source.ready = {observed("A", 3, {{"right",201}, {"left",12}})};
    runtime->tick(3);
    checkPalms({{"dev1/left",12}, {"dev1/right",201}});

    source.ready = {observed("A", 4, {{"right",202}})};
    runtime->tick(4);
    checkPalms({{"dev1/left",std::nullopt}, {"dev1/right",202}});

    source.ready = {observed("A", 5, {{"left",13}})};
    runtime->tick(5);
    checkPalms({{"dev1/left",13}, {"dev1/right",std::nullopt}});
    EXPECT_EQ(runtime->devices().at("A").framesProcessed, 5u);
}
TEST_F(RuntimeTest, LiveHandFilterReroutesOnlyItsOwnFreshCameraAndPreservesRemainingHands) {
    connect();
    auto settings = runtime->settings();
    settings.channels.sendFullSkeleton = true;
    settings.osc.rateHz = 1;
    runtime->applySettings(settings);
    source.ready = {observed("A", 1, {{"left",10}, {"right",20}}),
                    observed("B", 1, {{"left",30}, {"right",40}})};
    runtime->tick(1);
    checkPalms({{"dev1/left",10}, {"dev1/right",20}, {"dev2/left",30}, {"dev2/right",40}});

    settings.devices.at("A").handFilter = "right";
    runtime->applySettings(settings);
    checkPalms({{"dev1/left",std::nullopt}, {"dev1/right",20}});
    const auto& preview = runtime->previews().at("A");
    ASSERT_EQ(preview.processed.hands.size(), 1u);
    EXPECT_EQ(preview.processed.hands.front().handType, "right");
    EXPECT_EQ(preview.raw.hands.size(), 2u);
    EXPECT_EQ(runtime->previews().at("B").processed.hands.size(), 2u);

    source.ready = {observed("A", 2, {{"left",11}, {"right",21}}),
                    observed("B", 2, {{"left",31}, {"right",41}})};
    runtime->tick(2);
    EXPECT_TRUE(messages.empty()); // Both cameras keep their configured 1 Hz rate.
    settings.devices.at("A").handFilter = "left";
    runtime->applySettings(settings);
    checkPalms({{"dev1/left",11}, {"dev1/right",std::nullopt}});
    settings.devices.at("A").handFilter = "both";
    runtime->applySettings(settings);
    checkPalms({{"dev1/left",11}, {"dev1/right",21}});
    EXPECT_EQ(runtime->devices().at("A").framesProcessed, 2u);

    runtime->tick(502); // The cached camera samples have expired.
    messages.clear();
    settings.devices.at("A").handFilter = "right";
    runtime->applySettings(settings);
    EXPECT_TRUE(messages.empty());
    EXPECT_FALSE(runtime->devices().at("A").framesFresh);
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

    source.ready = {signal(TrackingEventKind::DeviceLost, "A")};
    runtime->tick(2601);
    source.ready = {signal(TrackingEventKind::DeviceConnected, "A"),
                    signal(TrackingEventKind::SubscriptionFailed, "A")};
    runtime->tick(2700);
    source.restartTimes.clear();
    runtime->tick(2800);
    runtime->tick(2900);
    EXPECT_EQ(source.restartTimes, (std::vector<Millis>{2800, 2900}));
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
TEST_F(RuntimeTest, SkeletonsRetainDistinctLeftHandIdentitiesAndClearOldRoutesOnce) {
    connect(false);
    auto settings = runtime->settings();
    settings.processing.gesturesEnabled = true;
    settings.osc.rateHz = 1;
    runtime->applySettings(settings);

    auto observe = [&](Millis time, std::initializer_list<std::uint32_t> ids) {
        auto sample = frame("A", time, time, false);
        for (const auto id : ids) {
            HandData hand;
            hand.handType = "left";
            hand.nativeHandId = id;
            sample.frame.hands.push_back(hand);
        }
        source.ready = {std::move(sample)};
        runtime->tick(time);
    };
    using Events = std::vector<std::pair<std::int32_t, std::int64_t>>;
    auto takeEvents = [&](const std::string& alias) {
        Events result;
        for (const auto& message : messages) {
            if (!message.address.starts_with("/handbridge/v2/")) continue;
            EXPECT_EQ(message.address, "/handbridge/v2/" + alias + "/left/skeleton");
            result.emplace_back(std::get<std::int32_t>(message.arguments.at(0)),
                                std::get<std::int64_t>(message.arguments.at(3)));
        }
        messages.clear();
        return result;
    };

    observe(0, {30, 40}); // Legacy and gesture streams already live before v2 is enabled.
    EXPECT_FALSE(messages.empty());
    EXPECT_TRUE(takeEvents("dev1").empty());
    settings.channels.sendFullSkeleton = true;
    runtime->applySettings(settings);
    EXPECT_TRUE(messages.empty());
    observe(1, {30, 40});
    EXPECT_EQ(takeEvents("dev1"), (Events{{1, 30}, {1, 40}}));
    observe(2, {30, 40});
    EXPECT_TRUE(takeEvents("dev1").empty()); // Still within the one-second rate limit.
    observe(3, {30, 50});
    EXPECT_EQ(takeEvents("dev1"), (Events{{0, 40}, {1, 30}, {1, 50}}));
    observe(4, {30, 50});
    EXPECT_TRUE(takeEvents("dev1").empty());

    settings.channels.sendFullSkeleton = false;
    runtime->applySettings(settings);
    EXPECT_EQ(messages.size(), 2u); // Only the two v2 losses, no scalar or gesture clears.
    EXPECT_EQ(takeEvents("dev1"), (Events{{0, 30}, {0, 50}}));
    settings.channels.sendFullSkeleton = true;
    runtime->applySettings(settings);
    EXPECT_TRUE(messages.empty());
    observe(5, {30, 50});
    EXPECT_EQ(takeEvents("dev1"), (Events{{1, 30}, {1, 50}}));

    settings.channels.sendWrist = false;
    runtime->applySettings(settings);
    EXPECT_EQ(zeros("dev1"), 1u);
    EXPECT_TRUE(takeEvents("dev1").empty());
    observe(6, {30, 50});
    EXPECT_EQ(takeEvents("dev1"), (Events{{1, 30}, {1, 50}}));
    settings.processing.gesturesEnabled = false;
    runtime->applySettings(settings);
    EXPECT_FALSE(messages.empty());
    EXPECT_TRUE(takeEvents("dev1").empty());

    settings.devices.at("A").alias = "dev7";
    runtime->applySettings(settings);
    EXPECT_EQ(takeEvents("dev1"), (Events{{0, 30}, {0, 50}}));
    runtime->applySettings(settings);
    EXPECT_TRUE(takeEvents("dev7").empty());
    observe(7, {30});
    EXPECT_EQ(takeEvents("dev7"), (Events{{1, 30}}));
    settings.devices.at("A").enabled = false;
    runtime->applySettings(settings);
    EXPECT_EQ(takeEvents("dev7"), (Events{{0, 30}}));
    observe(8, {30});
    EXPECT_TRUE(takeEvents("dev7").empty());
    settings.devices.at("A").enabled = true;
    runtime->applySettings(settings);
    observe(9, {50});
    EXPECT_EQ(takeEvents("dev7"), (Events{{1, 50}}));
    settings.channels.sendFullSkeleton = false;
    runtime->applySettings(settings);
    EXPECT_EQ(takeEvents("dev7"), (Events{{0, 50}}));
    observe(10, {50});
    runtime->stop();
    EXPECT_TRUE(takeEvents("dev7").empty());
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
