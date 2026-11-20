#include "transport/osc/SkeletonOsc.hpp"
#include <gtest/gtest.h>
#include <limits>

TEST(SkeletonOsc, MissingOrMalformedSegmentsKeepTheFollowingBoneInItsSlot) {
    HandData hand;
    hand.arm.width = -1;
    hand.arm.elbowPosition = {1, 2, 3};
    hand.arm.wristPosition = {4, 5, 6};
    hand.arm.rotation = {3, 0, 0, 0};
    hand.fingers.resize(2);
    auto& index = hand.fingers[1];
    index.fingerId = 42;
    index.isExtended = true;
    index.bones.resize(4);
    index.bones[0].prevJoint = {8, 9, 10};
    index.bones[0].nextJoint = {11, 12, 13};
    index.bones[0].width = 4;
    index.bones[0].rotation = {0, 0, 0, 0};
    index.bones[1].prevJoint.y = std::numeric_limits<float>::infinity();
    index.bones[2].width = std::numeric_limits<float>::quiet_NaN();
    index.bones[2].nextJoint = {11, 12, 13};
    index.bones[3].prevJoint = {1, 2, 3};
    index.bones[3].nextJoint = {4, 5, 6};
    index.bones[3].rotation = {.5f, .5f, -.5f, .5f};
    index.bones[3].width = 7;

    auto message = handbridge::skeletonMessage("dev1", "right", 1, 2, 3, &hand);
    const auto& args = message.arguments;
    ASSERT_EQ(args.size(), 278u);
    EXPECT_EQ(std::get<std::int32_t>(args[31]), 1);
    const float arm[] = {1, 2, 3, 4, 5, 6, 0, 0, 0, 0, 0};
    for (std::size_t i = 0; i < 11; ++i) EXPECT_EQ(std::get<float>(args[32 + i]), arm[i]);
    const std::size_t indexStart = 43 + 47;
    EXPECT_EQ(std::get<std::int32_t>(args[indexStart]), 42);
    EXPECT_EQ(std::get<std::int32_t>(args[indexStart + 1]), 1);
    EXPECT_EQ(std::get<std::int32_t>(args[indexStart + 2]), 13);
    const float metacarpal[] = {8, 9, 10, 11, 12, 13, 0, 0, 0, 0, 4};
    for (std::size_t i = 0; i < 11; ++i) EXPECT_EQ(std::get<float>(args[indexStart + 3 + i]), metacarpal[i]);
    for (std::size_t i = indexStart + 14; i < indexStart + 25; ++i) EXPECT_EQ(std::get<float>(args[i]), 0.f);
    const float intermediate[] = {0, 0, 0, 11, 12, 13, 1, 0, 0, 0, 0};
    for (std::size_t i = 0; i < 11; ++i) EXPECT_EQ(std::get<float>(args[indexStart + 25 + i]), intermediate[i]);
    const float distal[] = {1, 2, 3, 4, 5, 6, .5f, .5f, -.5f, .5f, 7};
    for (std::size_t i = 0; i < 11; ++i) EXPECT_EQ(std::get<float>(args[indexStart + 36 + i]), distal[i]);
    const auto middleStart = indexStart + 47;
    EXPECT_EQ(std::get<std::int32_t>(args[middleStart]), -1);
    EXPECT_EQ(std::get<std::int32_t>(args[middleStart + 1]), 0);
    EXPECT_EQ(std::get<std::int32_t>(args[middleStart + 2]), 0);
}

TEST(SkeletonOsc, UnavailablePalmComponentsDoNotDiscardOtherRawFields) {
    HandData hand;
    hand.palm.position = {11, 12, 13};
    hand.palm.stabilizedPosition = {21, 22, 23};
    hand.palm.velocity = {1, std::numeric_limits<float>::quiet_NaN(), 3};
    hand.palm.normal = {0, -1, 0};
    hand.palm.direction = {0, 0, -1};
    hand.palm.orientation = {std::numeric_limits<float>::infinity(), 0, 0, 0};
    hand.palm.width = 81;
    hand.arm.valid = false;
    hand.arm.wristPosition = {91, 92, 93};
    const auto message = handbridge::skeletonMessage("dev1", "left", 1, 2, std::nullopt, &hand);
    const auto& args = message.arguments;
    ASSERT_EQ(args.size(), 278u);
    const float palm[] = {11, 12, 13, 21, 22, 23, 0, 0, 0, 0, -1, 0, 0, 0, -1, 0, 0, 0, 0, 81};
    for (std::size_t i = 0; i < 20; ++i) EXPECT_EQ(std::get<float>(args[11 + i]), palm[i]);
    EXPECT_EQ(std::get<std::int32_t>(args[31]), 0);
    for (std::size_t i = 32; i < 43; ++i) EXPECT_EQ(std::get<float>(args[i]), 0.f);
}

TEST(SkeletonOsc, LossCarriesOnlyFrameTimestampAndLastIdentity) {
    constexpr std::int64_t frame = 12'345'678'901;
    constexpr std::int64_t timestamp = 8'000'000'000'000;
    auto loss = handbridge::skeletonMessage("dev2", "left", frame, timestamp, 4'000'000'000u, nullptr);
    ASSERT_EQ(loss.address, "/handbridge/v2/dev2/left/skeleton");
    ASSERT_EQ(loss.arguments.size(), 4u);
    EXPECT_EQ(std::get<std::int32_t>(loss.arguments[0]), 0);
    EXPECT_EQ(std::get<std::int64_t>(loss.arguments[1]), frame);
    EXPECT_EQ(std::get<std::int64_t>(loss.arguments[2]), timestamp);
    EXPECT_EQ(std::get<std::int64_t>(loss.arguments[3]), 4'000'000'000);
    loss = handbridge::skeletonMessage("dev2", "left", -1, -1, std::nullopt, nullptr);
    EXPECT_EQ(std::get<std::int64_t>(loss.arguments[3]), -1);
}
