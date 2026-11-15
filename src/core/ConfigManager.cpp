#include "ConfigManager.h"
#include "json.hpp" // Use nlohmann::json
#include "Log.hpp"
#include <fstream>
#include <filesystem> // For path manipulation
#ifdef _WIN32
#include <shlobj.h>   // For SHGetFolderPath
#include <windows.h>  // For PWSTR
#include <KnownFolders.h> // For FOLDERID_LocalAppData
#endif
#include <iomanip> // For std::setw
#include <stdexcept>
#include <algorithm>
#include <cctype>

using json = nlohmann::json;

// Helper function to get %LOCALAPPDATA% path (Windows specific)
std::string getLocalAppDataPath() {
#ifdef _WIN32
    PWSTR path = NULL;
    HRESULT hr = SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, NULL, &path);
    std::string appDataPath;
    if (SUCCEEDED(hr)) {
        // Convert WCHAR* to std::string (assuming UTF-8 is okay here)
        // For robust conversion, consider WideCharToMultiByte
        std::wstring widePath(path);
        int size_needed = WideCharToMultiByte(CP_UTF8, 0, &widePath[0], (int)widePath.size(), NULL, 0, NULL, NULL);
        std::string narrowPath(size_needed, 0);
        WideCharToMultiByte(CP_UTF8, 0, &widePath[0], (int)widePath.size(), &narrowPath[0], size_needed, NULL, NULL);
        appDataPath = narrowPath;
    } else {
        // Fallback or error handling
        LOG_ERR("Failed to get LocalAppData path.");
    }
    CoTaskMemFree(path);
    return appDataPath;
#else
    // SDK-free builds use an explicit filename; do not invent a platform config path.
    return {};
#endif
}

// Constructor - Initialize members with defaults
ConfigManager::ConfigManager()
    : baseGain_(1.0f), midGain_(1.0f), maxGain_(1.0f), lowSpeedThreshold_(0.0f), midSpeedThreshold_(0.0f)
{
    // Constructor body if needed
}

float ConfigManager::getBaseGain() const { return baseGain_; }
float ConfigManager::getMidGain() const { return midGain_; }
float ConfigManager::getMaxGain() const { return maxGain_; }
float ConfigManager::getLowSpeedThreshold() const { return lowSpeedThreshold_; }
float ConfigManager::getMidSpeedThreshold() const { return midSpeedThreshold_; }
void ConfigManager::setGainParams(float base, float mid, float max, float lowThresh, float midThresh) {
    baseGain_ = base;
    midGain_ = mid;
    maxGain_ = max;
    lowSpeedThreshold_ = lowThresh;
    midSpeedThreshold_ = midThresh;
}

// Get config path helper (moved inside)
std::filesystem::path getConfigPath() {
    std::string localAppData = getLocalAppDataPath();
    if (localAppData.empty()) {
        LOG_ERR("Could not determine LocalAppData path. Config saving/loading might fail.");
        return {}; // Return empty path on failure
    }
    return std::filesystem::path(localAppData) / "LeapApp" / "config.json";
}

// Load configuration implementation (public, no args)
bool ConfigManager::loadConfig() {
    return loadConfig(getConfigPath().string());
}

// Load configuration implementation (public, with filename)
bool ConfigManager::loadConfig(const std::string& filename) {
    if (filename.empty()) return false;
    std::ifstream ifs(filename);
    if (!ifs.is_open()) {
        LOG("Config file not found: " << filename << ". Using defaults.");
        return false;
    }
    try {
        json j;
        ifs >> j;
        if (!j.is_object()) throw std::invalid_argument("Config root must be an object");
        if (j.contains("schema_version")) {
            throw std::invalid_argument("Versioned configuration is not implemented; use the legacy flat schema");
        }

        // Validate a complete candidate before mutating the live configuration.
        ConfigManager next;
        next.oscIp = j.value("osc_ip", "127.0.0.1");
        if (j.contains("osc_port") && (!j.at("osc_port").is_number_integer() ||
            j.at("osc_port") < 1 || j.at("osc_port") > 65535)) {
            throw std::invalid_argument("osc_port must be an integer between 1 and 65535");
        }
        next.oscPort = j.value("osc_port", 9000);
        next.lowLatencyMode = j.value("low_latency_mode", false);
        next.deviceHandAssignments = deviceHandAssignments;
        if (j.contains("hand_assignments")) {
            next.deviceHandAssignments = j.at("hand_assignments").get<std::map<std::string, std::string>>();
        }
        for (const auto& [serial, assignment] : next.deviceHandAssignments) {
            std::string normalized = assignment;
            std::transform(normalized.begin(), normalized.end(), normalized.begin(),
                           [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
            if (!normalized.empty() && normalized != "LEFT" && normalized != "RIGHT" &&
                normalized != "BOTH" && normalized != "NONE" && normalized != "DISABLED") {
                throw std::invalid_argument("Unsupported hand assignment for " + serial);
            }
        }
        next.deviceAliasManager.loadFromJson(deviceAliasManager.toJson());

        // Canonical on-disk key is snake_case; the alias manager's legacy
        // API uses a camelCase wrapper. Accept both historical containers.
        if (j.contains("device_aliases") && !j["device_aliases"].is_null()) {
            if (j.contains("deviceAliases")) {
                throw std::invalid_argument("Ambiguous alias containers: device_aliases and deviceAliases");
            }
            const auto& aliases = j.at("device_aliases");
            if (aliases.is_object() && aliases.contains("deviceAliases")) {
                if (aliases.size() != 1) throw std::invalid_argument("Mixed alias containers");
                next.deviceAliasManager.loadFromJson(aliases);
            } else {
                next.deviceAliasManager.loadFromJson(json{{"deviceAliases", aliases}});
            }
        } else if (j.contains("deviceAliases")) {
            next.deviceAliasManager.loadFromJson(j);
        }
        // A null device_aliases was produced by the old broken saver. It
        // contains no recoverable mapping; retain any existing in-memory map.
        const auto settings = j.value("booleanSettings", json::object());
        if (!settings.is_object()) throw std::invalid_argument("booleanSettings must be an object");
        next.sendPalm_ = settings.value("sendPalm", sendPalm_);
        next.sendWrist_ = settings.value("sendWrist", sendWrist_);
        next.sendThumb_ = settings.value("sendThumb", sendThumb_);
        next.sendIndex_ = settings.value("sendIndex", sendIndex_);
        next.sendMiddle_ = settings.value("sendMiddle", sendMiddle_);
        next.sendRing_ = settings.value("sendRing", sendRing_);
        next.sendPinky_ = settings.value("sendPinky", sendPinky_);
        next.sendPalmOrientation_ = settings.value("sendPalmOrientation", sendPalmOrientation_);
        next.sendPalmVelocity_ = settings.value("sendPalmVelocity", sendPalmVelocity_);
        next.sendPalmNormal_ = settings.value("sendPalmNormal", sendPalmNormal_);
        next.sendVisibleTime_ = settings.value("sendVisibleTime", sendVisibleTime_);
        next.sendFingerIsExtended_ = settings.value("sendFingerIsExtended", sendFingerIsExtended_);
        next.sendPinchStrength_ = settings.value("sendPinchStrength", sendPinchStrength_);
        next.sendGrabStrength_ = settings.value("sendGrabStrength", sendGrabStrength_);

        // The alias loader also validates before swapping. Remaining moves and
        // scalar assignments do not allocate or parse additional input.
        deviceAliasManager.loadFromJson(next.deviceAliasManager.toJson());
        oscIp = std::move(next.oscIp);
        oscPort = next.oscPort;
        lowLatencyMode = next.lowLatencyMode;
        deviceHandAssignments = std::move(next.deviceHandAssignments);
        sendPalm_ = next.sendPalm_;
        sendWrist_ = next.sendWrist_;
        sendThumb_ = next.sendThumb_;
        sendIndex_ = next.sendIndex_;
        sendMiddle_ = next.sendMiddle_;
        sendRing_ = next.sendRing_;
        sendPinky_ = next.sendPinky_;
        sendPalmOrientation_ = next.sendPalmOrientation_;
        sendPalmVelocity_ = next.sendPalmVelocity_;
        sendPalmNormal_ = next.sendPalmNormal_;
        sendVisibleTime_ = next.sendVisibleTime_;
        sendFingerIsExtended_ = next.sendFingerIsExtended_;
        sendPinchStrength_ = next.sendPinchStrength_;
        sendGrabStrength_ = next.sendGrabStrength_;
        LOG("Configuration loaded successfully from " << filename);
        return true;
    } catch (const std::exception& e) {
        (void)e;
        LOG_ERR("Error parsing config file " << filename << ": " << e.what());
        return false;
    }
}

// Save configuration implementation (public, no args)
bool ConfigManager::saveConfig() {
    // Calls the IConfigStore::save override below
    return save(getConfigPath().string()); 
}

// Private helper definition (renamed and const)
bool ConfigManager::saveConfigToFile(const std::string& filename) const { 
    if (filename.empty()) {
        LOG_ERR("Config filename is empty, cannot save.");
        return false;
    }
    // Ensure directory exists
    std::filesystem::path configPath(filename);
    std::filesystem::path configDir = configPath.parent_path();
    try {
        if (!configDir.empty() && !std::filesystem::exists(configDir)) {
            std::filesystem::create_directories(configDir);
            LOG("Created config directory: " << configDir.string());
        }
    } catch (const std::filesystem::filesystem_error& e) {
        (void)e;
        LOG_ERR("Error creating config directory " << configDir.string() << ": " << e.what());
        return false;
    }

    json j;
    // Save OSC settings
    j["osc_ip"] = this->oscIp; 
    j["osc_port"] = this->oscPort;
    j["low_latency_mode"] = this->lowLatencyMode;
    // Save Hand Assignments
    j["hand_assignments"] = this->deviceHandAssignments;
    // Save Aliases
    j["device_aliases"] = deviceAliasManager.toJson().at("deviceAliases");
    // Save Filter Settings
    json booleanSettings;
    booleanSettings["sendPalm"] = this->sendPalm_;
    booleanSettings["sendWrist"] = this->sendWrist_;
    booleanSettings["sendThumb"] = this->sendThumb_;
    booleanSettings["sendIndex"] = this->sendIndex_;
    booleanSettings["sendMiddle"] = this->sendMiddle_;
    booleanSettings["sendRing"] = this->sendRing_;
    booleanSettings["sendPinky"] = this->sendPinky_;
    booleanSettings["sendPalmOrientation"] = this->sendPalmOrientation_;
    booleanSettings["sendPalmVelocity"] = this->sendPalmVelocity_;
    booleanSettings["sendPalmNormal"] = this->sendPalmNormal_;
    booleanSettings["sendVisibleTime"] = this->sendVisibleTime_;
    booleanSettings["sendFingerIsExtended"] = this->sendFingerIsExtended_;
    booleanSettings["sendPinchStrength"] = this->sendPinchStrength_;
    booleanSettings["sendGrabStrength"] = this->sendGrabStrength_;
    j["booleanSettings"] = booleanSettings;

    std::ofstream ofs(filename);
    if (!ofs.is_open()) {
        LOG_ERR("Could not open config file for writing: " << filename);
        return false;
    }
    try {
        ofs << std::setw(4) << j << std::endl;
        ofs.flush();
        if (!ofs) {
            LOG_ERR("Error writing config file " << filename);
            return false;
        }
        LOG("Configuration saved successfully to " << filename);
        return true;
    } catch (const std::exception& e) {
        (void)e;
        LOG_ERR("Error writing config file " << filename << ": " << e.what());
        return false;
    }
}

// Device preference implementations
void ConfigManager::setDefaultHandAssignment(const std::string& serialNumber, const std::string& hand) {
    if (hand.empty() || hand == "NONE") {
        this->deviceHandAssignments.erase(serialNumber);
    } else {
        this->deviceHandAssignments[serialNumber] = hand;
    }
}

void ConfigManager::setAllDefaultHandAssignments(const std::map<std::string, std::string>& assignments) {
    this->deviceHandAssignments = assignments;
}

std::string ConfigManager::getDefaultHandAssignment(const std::string& serialNumber) const {
    auto it = this->deviceHandAssignments.find(serialNumber);
    if (it != this->deviceHandAssignments.end()) {
        return it->second;
    }
    return ""; // Return empty string if not found
}

std::map<std::string, std::string> ConfigManager::getAllDefaultHandAssignments() const {
    return this->deviceHandAssignments;
}

DeviceAliasManager& ConfigManager::getDeviceAliasManager() { return deviceAliasManager; }
const DeviceAliasManager& ConfigManager::getDeviceAliasManager() const { return deviceAliasManager; }

// Implement IConfigStore pure virtuals
bool ConfigManager::load(const std::string& filename) {
    return loadConfig(filename);
}
// IConfigStore::save override (must be const)
bool ConfigManager::save(const std::string& filename) const {
    // Calls the const helper method defined above
    return saveConfigToFile(filename); 
}


std::string ConfigManager::getOscIp() const { return oscIp; }
int ConfigManager::getOscPort() const { return oscPort; }
void ConfigManager::setOscIp(const std::string& ip) { oscIp = ip; }
void ConfigManager::setOscPort(int port) { oscPort = port; }

bool ConfigManager::getLowLatencyMode() const { return lowLatencyMode; }
void ConfigManager::setLowLatencyMode(bool enabled) { lowLatencyMode = enabled; }

bool ConfigManager::isSendPalmEnabled() const { return sendPalm_; }
bool ConfigManager::isSendWristEnabled() const { return sendWrist_; }
bool ConfigManager::isSendThumbEnabled() const { return sendThumb_; }
bool ConfigManager::isSendIndexEnabled() const { return sendIndex_; }
bool ConfigManager::isSendMiddleEnabled() const { return sendMiddle_; }
bool ConfigManager::isSendRingEnabled() const { return sendRing_; }
bool ConfigManager::isSendPinkyEnabled() const { return sendPinky_; }
bool ConfigManager::isSendPalmOrientationEnabled() const { return sendPalmOrientation_; }
bool ConfigManager::isSendPalmVelocityEnabled() const { return sendPalmVelocity_; }
bool ConfigManager::isSendPalmNormalEnabled() const { return sendPalmNormal_; }
bool ConfigManager::isSendVisibleTimeEnabled() const { return sendVisibleTime_; }
bool ConfigManager::isSendFingerIsExtendedEnabled() const { return sendFingerIsExtended_; }
bool ConfigManager::isSendPinchStrengthEnabled() const { return sendPinchStrength_; }
bool ConfigManager::isSendGrabStrengthEnabled() const { return sendGrabStrength_; }

void ConfigManager::setSendPalmEnabled(bool enabled) { sendPalm_ = enabled; }
void ConfigManager::setSendWristEnabled(bool enabled) { sendWrist_ = enabled; }
void ConfigManager::setSendThumbEnabled(bool enabled) { sendThumb_ = enabled; }
void ConfigManager::setSendIndexEnabled(bool enabled) { sendIndex_ = enabled; }
void ConfigManager::setSendMiddleEnabled(bool enabled) { sendMiddle_ = enabled; }
void ConfigManager::setSendRingEnabled(bool enabled) { sendRing_ = enabled; }
void ConfigManager::setSendPinkyEnabled(bool enabled) { sendPinky_ = enabled; }
void ConfigManager::setSendPalmOrientationEnabled(bool enabled) { sendPalmOrientation_ = enabled; }
void ConfigManager::setSendPalmVelocityEnabled(bool enabled) { sendPalmVelocity_ = enabled; }
void ConfigManager::setSendPalmNormalEnabled(bool enabled) { sendPalmNormal_ = enabled; }
void ConfigManager::setSendVisibleTimeEnabled(bool enabled) { sendVisibleTime_ = enabled; }
void ConfigManager::setSendFingerIsExtendedEnabled(bool enabled) { sendFingerIsExtended_ = enabled; }
void ConfigManager::setSendPinchStrengthEnabled(bool enabled) { sendPinchStrength_ = enabled; }
void ConfigManager::setSendGrabStrengthEnabled(bool enabled) { sendGrabStrength_ = enabled; }