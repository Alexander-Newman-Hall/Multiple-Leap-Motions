#include <gtest/gtest.h>
#include "../src/core/ConfigManager.h"
#include <filesystem>
#include <fstream>
#include <string>

namespace {
class ConfigManagerTest : public testing::Test {
protected:
    std::filesystem::path filename = "test_config_roundtrip.json";
    void TearDown() override { std::filesystem::remove(filename); }
    void write(const nlohmann::json& document) {
        std::ofstream file(filename);
        file << document;
    }
};
}

TEST_F(ConfigManagerTest, SafeDefaultsWithoutAConfigFile) {
    ConfigManager config;
    EXPECT_EQ(config.getOscIp(), "127.0.0.1");
    EXPECT_EQ(config.getOscPort(), 9000);
    EXPECT_FALSE(config.getLowLatencyMode());
    EXPECT_TRUE(config.isSendPalmEnabled());
    EXPECT_TRUE(config.isSendWristEnabled());
    EXPECT_TRUE(config.isSendThumbEnabled());
    EXPECT_TRUE(config.isSendIndexEnabled());
    EXPECT_TRUE(config.isSendMiddleEnabled());
    EXPECT_TRUE(config.isSendRingEnabled());
    EXPECT_TRUE(config.isSendPinkyEnabled());
    EXPECT_TRUE(config.isSendPinchStrengthEnabled());
    EXPECT_TRUE(config.isSendGrabStrengthEnabled());
    EXPECT_FALSE(config.isSendPalmOrientationEnabled());
    EXPECT_FALSE(config.isSendPalmVelocityEnabled());
    EXPECT_FALSE(config.isSendPalmNormalEnabled());
    EXPECT_FALSE(config.isSendVisibleTimeEnabled());
    EXPECT_FALSE(config.isSendFingerIsExtendedEnabled());
    EXPECT_FALSE(config.load("missing-handbridge-config.json"));
    EXPECT_EQ(config.getOscPort(), 9000);
}

TEST_F(ConfigManagerTest, SaveLoadPreservesAliasesWithoutDiscoveryOrderDependence) {
    ConfigManager config;
    config.setOscIp("127.0.0.2");
    config.setOscPort(7000);
    config.setLowLatencyMode(true);
    config.setDefaultHandAssignment("serialA", "LEFT");
    config.setDefaultHandAssignment("serialB", "RIGHT");
    EXPECT_EQ(config.getDeviceAliasManager().getOrAssignAlias("serialB"), "dev1");
    EXPECT_EQ(config.getDeviceAliasManager().getOrAssignAlias("serialA"), "dev2");
    config.setSendPalmEnabled(false);
    config.setSendWristEnabled(false);
    config.setSendThumbEnabled(false);
    config.setSendIndexEnabled(false);
    config.setSendMiddleEnabled(false);
    config.setSendRingEnabled(false);
    config.setSendPinkyEnabled(false);
    config.setSendPinchStrengthEnabled(false);
    config.setSendGrabStrengthEnabled(false);
    config.setSendPalmOrientationEnabled(true);
    config.setSendPalmVelocityEnabled(true);
    config.setSendPalmNormalEnabled(true);
    config.setSendVisibleTimeEnabled(true);
    config.setSendFingerIsExtendedEnabled(true);
    ASSERT_TRUE(config.save(filename.string()));

    nlohmann::json saved;
    std::ifstream file(filename);
    file >> saved;
    EXPECT_EQ(saved.at("device_aliases"), (nlohmann::json{{"serialA", "dev2"}, {"serialB", "dev1"}}));

    ConfigManager loaded;
    ASSERT_TRUE(loaded.load(filename.string()));
    EXPECT_EQ(loaded.getOscIp(), "127.0.0.2");
    EXPECT_EQ(loaded.getOscPort(), 7000);
    EXPECT_TRUE(loaded.getLowLatencyMode());
    EXPECT_EQ(loaded.getDefaultHandAssignment("serialA"), "LEFT");
    EXPECT_EQ(loaded.getDefaultHandAssignment("serialB"), "RIGHT");
    EXPECT_EQ(loaded.getDeviceAliasManager().getOrAssignAlias("serialA"), "dev2");
    EXPECT_EQ(loaded.getDeviceAliasManager().getOrAssignAlias("serialB"), "dev1");
    EXPECT_EQ(loaded.getDeviceAliasManager().getOrAssignAlias("serialC"), "dev3");
    EXPECT_FALSE(loaded.isSendPalmEnabled());
    EXPECT_FALSE(loaded.isSendWristEnabled());
    EXPECT_FALSE(loaded.isSendThumbEnabled());
    EXPECT_FALSE(loaded.isSendIndexEnabled());
    EXPECT_FALSE(loaded.isSendMiddleEnabled());
    EXPECT_FALSE(loaded.isSendRingEnabled());
    EXPECT_FALSE(loaded.isSendPinkyEnabled());
    EXPECT_FALSE(loaded.isSendPinchStrengthEnabled());
    EXPECT_FALSE(loaded.isSendGrabStrengthEnabled());
    EXPECT_TRUE(loaded.isSendPalmOrientationEnabled());
    EXPECT_TRUE(loaded.isSendPalmVelocityEnabled());
    EXPECT_TRUE(loaded.isSendPalmNormalEnabled());
    EXPECT_TRUE(loaded.isSendVisibleTimeEnabled());
    EXPECT_TRUE(loaded.isSendFingerIsExtendedEnabled());
}

TEST_F(ConfigManagerTest, AcceptsKnownLegacyAliasContainers) {
    const nlohmann::json aliases{{"serialA", "dev8"}, {"serialB", "dev3"}};
    const nlohmann::json documents[] = {
        {{"device_aliases", aliases}},
        {{"deviceAliases", aliases}},
        {{"device_aliases", {{"deviceAliases", aliases}}}}
    };
    for (const auto& document : documents) {
        write(document);
        ConfigManager loaded;
        ASSERT_TRUE(loaded.load(filename.string()));
        EXPECT_EQ(loaded.getOscPort(), 9000);
        EXPECT_EQ(loaded.getDeviceAliasManager().getOrAssignAlias("serialB"), "dev3");
        EXPECT_EQ(loaded.getDeviceAliasManager().getOrAssignAlias("serialA"), "dev8");
        EXPECT_EQ(loaded.getDeviceAliasManager().getOrAssignAlias("new-serial"), "dev9");
    }
}

TEST_F(ConfigManagerTest, BrokenNullAliasOutputLoadsSafelyButCannotRecoverLostMapping) {
    write({{"osc_port", 7000}, {"device_aliases", nullptr}});
    ConfigManager config;
    ASSERT_TRUE(config.load(filename.string()));
    EXPECT_EQ(config.getOscPort(), 7000);
    EXPECT_EQ(config.getDeviceAliasManager().getOrAssignAlias("unknown"), "dev1");
}

TEST_F(ConfigManagerTest, RejectsAmbiguousAliasContainers) {
    write({{"device_aliases", {{"A", "dev1"}}}, {"deviceAliases", {{"A", "dev2"}}}});
    ConfigManager config;
    EXPECT_FALSE(config.load(filename.string()));
}

TEST_F(ConfigManagerTest, RejectsProposedVersionedSchemaWithoutChangingLiveSettings) {
    write({{"schema_version", 2}, {"osc", {{"host", "127.0.0.1"}, {"port", 8000}}}});
    ConfigManager config;
    config.setOscPort(7000);
    EXPECT_FALSE(config.load(filename.string()));
    EXPECT_EQ(config.getOscPort(), 7000);
}

TEST_F(ConfigManagerTest, RejectsInvalidPortAndRoot) {
    const nlohmann::json invalidDocuments[] = {
        {{"osc_port", 0}}, {{"osc_port", -1}}, {{"osc_port", 65536}},
        {{"osc_port", 9000.5}}, {{"osc_port", 4294976296ULL}},
        nlohmann::json::array(), nullptr
    };
    ConfigManager config;
    config.setOscPort(7000);
    for (const auto& document : invalidDocuments) {
        write(document);
        EXPECT_FALSE(config.load(filename.string()));
        EXPECT_EQ(config.getOscPort(), 7000);
    }
}

TEST_F(ConfigManagerTest, RejectsInvalidLateFieldWithoutPartiallyApplyingAliasesOrPort) {
    ConfigManager config;
    config.setOscPort(7000);
    config.setDefaultHandAssignment("old", "LEFT");
    config.getDeviceAliasManager().getOrAssignAlias("old");
    write({{"osc_port", 8000}, {"hand_assignments", {{"new", "RIGHT"}}},
           {"device_aliases", {{"new", "dev9"}}}, {"booleanSettings", {{"sendPalm", "invalid"}}}});
    EXPECT_FALSE(config.load(filename.string()));
    EXPECT_EQ(config.getOscPort(), 7000);
    EXPECT_EQ(config.getDefaultHandAssignment("old"), "LEFT");
    EXPECT_EQ(config.getDefaultHandAssignment("new"), "");
    EXPECT_EQ(config.getDeviceAliasManager().getOrAssignAlias("old"), "dev1");
    EXPECT_EQ(config.getDeviceAliasManager().getOrAssignAlias("new"), "dev2");
    EXPECT_TRUE(config.isSendPalmEnabled());
}

TEST_F(ConfigManagerTest, RejectsInvalidHandAssignmentsTransactionally) {
    ConfigManager config;
    config.setDefaultHandAssignment("old", "LEFT");
    write({{"osc_port", 8000}, {"hand_assignments", {{"new", "RIGHTT"}}}});
    EXPECT_FALSE(config.load(filename.string()));
    EXPECT_EQ(config.getOscPort(), 9000);
    EXPECT_EQ(config.getDefaultHandAssignment("old"), "LEFT");
    EXPECT_EQ(config.getDefaultHandAssignment("new"), "");
}

TEST_F(ConfigManagerTest, PreservesSupportedHandAssignmentCaseAndSemantics) {
    const std::string assignments[] = {"", "left", "LEFT", "right", "RIGHT", "both", "BOTH", "none", "NONE", "disabled", "DISABLED"};
    for (const auto& assignment : assignments) {
        write({{"hand_assignments", {{"A", assignment}}}});
        ConfigManager config;
        ASSERT_TRUE(config.load(filename.string()));
        EXPECT_EQ(config.getDefaultHandAssignment("A"), assignment);
    }
}
