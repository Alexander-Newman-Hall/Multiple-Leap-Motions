#include "DeviceAliasManager.hpp"

AssignedHand DeviceAliasManager::getAssignedHand(const std::string& alias) const {
    // TODO: Replace with actual assignment logic if available
    // For now, always return Both (default)
    return AssignedHand::Both;
}

#include <sstream>
#include <stdexcept>
#include <string_view>
#include <iostream>
#include <mutex>
#include <memory>
#include <thread>
#include <shared_mutex>
#include <set>
#include <limits>
#include <algorithm>

void DeviceAliasManager::validateAlias(const std::string& alias) {
    if (alias.size() < 4 || alias.compare(0, 3, "dev") != 0 ||
        !std::all_of(alias.begin() + 3, alias.end(), [](unsigned char c) { return c >= '0' && c <= '9'; })) {
        throw std::invalid_argument("Invalid alias format: expected dev followed by decimal digits");
    }
    // Leave room for the next automatic alias; unlike signed int arithmetic,
    // this check cannot overflow while recovering a persisted high index.
    const auto index = std::stoull(alias.substr(3));
    if (index == std::numeric_limits<std::uint64_t>::max()) {
        throw std::invalid_argument("Alias index exhausted");
    }
}

std::string DeviceAliasManager::getOrAssignAlias(const std::string& serial) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = serialToAlias_.find(serial);
    if (it != serialToAlias_.end()) {
        return it->second;
    }
    std::string alias = generateAlias();
    validateAlias(alias);
    serialToAlias_[serial] = alias;
    ++nextAliasIndex_;
    return alias;
}

void DeviceAliasManager::loadFromJson(const nlohmann::json& j) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!j.is_object() || !j.contains("deviceAliases") || !j.at("deviceAliases").is_object()) {
        throw std::invalid_argument("Expected deviceAliases object");
    }

    std::map<std::string, std::string> nextAliases;
    std::set<std::string> assignedAliases;
    std::uint64_t nextIndex = 1;
    for (auto it = j.at("deviceAliases").begin(); it != j.at("deviceAliases").end(); ++it) {
        const auto alias = it.value().get<std::string>();
        validateAlias(alias);
        if (!assignedAliases.insert(alias).second) {
            throw std::invalid_argument("Duplicate device alias: " + alias);
        }
        nextAliases.emplace(it.key(), alias);
        nextIndex = std::max(nextIndex, static_cast<std::uint64_t>(std::stoull(alias.substr(3))) + 1);
    }
    // Commit only after the entire alias map validates.
    serialToAlias_ = std::move(nextAliases);
    nextAliasIndex_ = nextIndex;
}

nlohmann::json DeviceAliasManager::toJson() const {
    std::lock_guard<std::mutex> lock(mutex_);
    nlohmann::json j;
    j["deviceAliases"] = serialToAlias_;
    return j;
}

void DeviceAliasManager::clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    serialToAlias_.clear();
    nextAliasIndex_ = 1;
}

std::string DeviceAliasManager::generateAlias() {
    std::ostringstream oss;
    oss << "dev" << nextAliasIndex_;
    return oss.str();
}
