#pragma once
#include "ControlSettings.hpp"
#include "TrackingSource.hpp"
#include "transport/osc/OscMessage.hpp"
#include <map>
#include <optional>
#include <set>
#include <vector>

namespace handbridge {
// Stateful filters are isolated by serial/hand/channel. A change between known
// native hand IDs resets only that hand; unknown legacy/simulated IDs preserve
// existing behavior. Loss/restart/settings changes also reset state.
class ProcessingPipeline {
public:
    void setSettings(const ProcessingSettings& settings);
    FrameData process(const FrameData& raw, Millis arrivalMs);
    std::vector<OscMessage> gestures(const FrameData& frame, const std::string& alias);
    void reset(const std::string& serial);
    void reset();
private:
    struct HandState {
        std::optional<std::uint32_t> nativeId;
        Millis lastMs = 0;
        bool initialized = false;
        std::map<std::string, Vector3> positions;
    };
    struct GestureState {
        std::optional<std::uint32_t> nativeId;
        bool pinch = false, grab = false;
    };
    ProcessingSettings settings_;
    std::map<std::string, std::map<std::string, HandState>> hands_;
    std::map<std::string, std::map<std::string, GestureState>> gestures_;
};
}
