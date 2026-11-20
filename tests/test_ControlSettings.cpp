#include <gtest/gtest.h>
#include "runtime/ControlSettings.hpp"
#include <limits>
using namespace handbridge;
TEST(ControlSettings, DefaultsKeepLegacyDestinationFlagsAndBypass) {
    ControlSettings settings; settings.validate();
    EXPECT_EQ(settings.osc.host,"127.0.0.1"); EXPECT_EQ(settings.osc.port,9000); EXPECT_EQ(settings.osc.rateHz,0);
    EXPECT_TRUE(settings.processing.bypass); EXPECT_EQ(settings.processing.outputUnits,"mm");
    EXPECT_FALSE(settings.processing.gesturesEnabled);
    EXPECT_TRUE(settings.channels.sendPalm); EXPECT_TRUE(settings.channels.sendIndex);
    EXPECT_FALSE(settings.channels.sendPalmOrientation);
    EXPECT_FALSE(settings.channels.sendFullSkeleton);
    auto previousProfile = settings.toJson();
    previousProfile.at("channels").erase("sendFullSkeleton");
    EXPECT_FALSE(ControlSettings::fromJson(previousProfile).channels.sendFullSkeleton);
    EXPECT_EQ(ControlSettings::fromJson(settings.toJson()).toJson(),settings.toJson());
}
TEST(ControlSettings, RoundTripAllSettingsIncludingDevicesAndOptionalChannels) {
    ControlSettings s;
    s.osc = {false,"127.0.0.2",7000,30};
    s.processing = {false,75,2,{2,-1,3},{10,20,30},{90,45,-30},"m",true,.9,.7,.8,.5};
    s.channels.sendPalm = false; s.channels.sendPalmOrientation = true; s.channels.sendPalmVelocity = true;
    s.channels.sendFingerIsExtended = true; s.channels.sendVisibleTime = true;
    s.channels.sendFullSkeleton = true;
    s.devices = {{"A",{"dev7",false,"left"}},{"B",{"dev8",true,"right"}}};
    EXPECT_TRUE(s.toJson().at("channels").at("sendFullSkeleton").get<bool>());
    EXPECT_TRUE(ControlSettings::fromJson(s.toJson()).channels.sendFullSkeleton);
    EXPECT_EQ(ControlSettings::fromJson(s.toJson()).toJson(),s.toJson());
}
TEST(ControlSettings, ImportLegacyPreservesAliasesPortChannelsAndAssignments) {
    ConfigManager legacy; legacy.setOscPort(7000); legacy.setSendWristEnabled(false);
    legacy.getDeviceAliasManager().loadFromJson({{"deviceAliases",{{"A","dev9"},{"B","dev2"}}}});
    legacy.setDefaultHandAssignment("A","RIGHT"); legacy.setDefaultHandAssignment("B","none");
    legacy.setDefaultHandAssignment("C","LEFT");
    const auto s = ControlSettings::fromLegacy(legacy);
    EXPECT_FALSE(s.channels.sendFullSkeleton);
    EXPECT_EQ(s.osc.port,7000); EXPECT_FALSE(s.channels.sendWrist); EXPECT_EQ(s.devices.at("A").alias,"dev9");
    EXPECT_EQ(s.devices.at("A").handFilter,"right"); EXPECT_EQ(s.devices.at("B").handFilter,"both");
    EXPECT_EQ(s.devices.at("C").handFilter,"left"); EXPECT_NE(s.devices.at("C").alias,"dev2");
}
TEST(ControlSettings, InvalidVersionsTypesRangesAndUnknownFieldsReject) {
    const auto base = ControlSettings{}.toJson();
    for (const auto& mutation : std::vector<nlohmann::json>{
        {{"schema_version",3}}, {{"schema_version","2"}}, {{"unexpected",true}},
        {{"osc",{{"port",0}}}}, {{"osc",{{"port",9000.5}}}}, {{"osc",{{"port",65536}}}},
        {{"osc",{{"host","localhost"}}}}, {{"osc",{{"rate_hz",.5}}}}, {{"osc",{{"enabled",1}}}},
        {{"channels",{{"sendPlam",true}}}}, {{"channels",{{"sendPalm",1}}}},
        {{"channels",{{"sendFullSkeleton","true"}}}},
        {{"processing",{{"scale",{1,2}}}}}, {{"processing",{{"scale",{1,"bad",3}}}}},
        {{"processing",{{"output_units","cm"}}}}, {{"processing",{{"smoothing_ms",-1}}}},
        {{"processing",{{"pinch_enter",.4},{"pinch_exit",.8}}}},
        {{"devices",{{"A",{{"alias","dev1"},{"hand_filter","RIGHTT"}}}}}},
        {{"devices",{{"A",{{"alias","dev1"}}},{"B",{{"alias","dev1"}}}}}}
    }) {
        auto value = base; value.update(mutation);
        EXPECT_THROW(ControlSettings::fromJson(value),std::exception) << value.dump();
    }
    ControlSettings nan; nan.processing.smoothingMs = std::numeric_limits<double>::quiet_NaN();
    EXPECT_THROW(nan.validate(),std::invalid_argument);
    nan = {}; nan.processing.scale[0] = std::numeric_limits<double>::infinity();
    EXPECT_THROW(nan.validate(),std::invalid_argument);
}
