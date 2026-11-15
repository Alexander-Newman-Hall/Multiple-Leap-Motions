#include <gtest/gtest.h>
#include "../src/core/DeviceAliasManager.hpp"

TEST(DeviceAliasManagerTest, AssignsUniqueAliases) {
    DeviceAliasManager manager;
    std::string alias1 = manager.getOrAssignAlias("serial123");
    std::string alias2 = manager.getOrAssignAlias("serial456");
    EXPECT_NE(alias1, alias2);
}

TEST(DeviceAliasManagerTest, ReturnsSameAliasForSameSerial) {
    DeviceAliasManager manager;
    std::string alias1 = manager.getOrAssignAlias("serialABC");
    std::string alias2 = manager.getOrAssignAlias("serialABC");
    EXPECT_EQ(alias1, alias2);
}

TEST(DeviceAliasManagerTest, ClearsAliases) {
    DeviceAliasManager manager;
    std::string alias1 = manager.getOrAssignAlias("serialXYZ");
    manager.clear();
    std::string alias2 = manager.getOrAssignAlias("serialXYZ");
    EXPECT_EQ(alias1, alias2); // After clear, same serial should get the same alias
}

TEST(DeviceAliasManagerTest, PreservesAliasesAcrossOppositeDiscoveryOrder) {
    DeviceAliasManager original;
    EXPECT_EQ(original.getOrAssignAlias("B"), "dev1");
    EXPECT_EQ(original.getOrAssignAlias("A"), "dev2");
    DeviceAliasManager loaded;
    loaded.loadFromJson(original.toJson());
    EXPECT_EQ(loaded.getOrAssignAlias("A"), "dev2");
    EXPECT_EQ(loaded.getOrAssignAlias("B"), "dev1");
    EXPECT_EQ(loaded.getOrAssignAlias("C"), "dev3");
}

TEST(DeviceAliasManagerTest, RejectsDuplicateAliasesWithoutLosingPreviousMapping) {
    DeviceAliasManager manager;
    EXPECT_EQ(manager.getOrAssignAlias("original"), "dev1");
    EXPECT_THROW(manager.loadFromJson({{"deviceAliases", {{"A", "dev2"}, {"B", "dev2"}}}}), std::invalid_argument);
    EXPECT_EQ(manager.getOrAssignAlias("original"), "dev1");
    EXPECT_EQ(manager.getOrAssignAlias("new"), "dev2");
}

TEST(DeviceAliasManagerTest, RejectsMalformedAliasWithoutLosingPreviousMapping) {
    DeviceAliasManager manager;
    manager.getOrAssignAlias("original");
    EXPECT_THROW(manager.loadFromJson({{"deviceAliases", {{"A", "dev-1"}}}}), std::invalid_argument);
    EXPECT_EQ(manager.getOrAssignAlias("original"), "dev1");
}

TEST(DeviceAliasManagerTest, HighLegacyAliasDoesNotOverflowNextIndex) {
    DeviceAliasManager manager;
    manager.loadFromJson({{"deviceAliases", {{"A", "dev2147483647"}}}});
    EXPECT_EQ(manager.getOrAssignAlias("A"), "dev2147483647");
    EXPECT_EQ(manager.getOrAssignAlias("B"), "dev2147483648");
}
