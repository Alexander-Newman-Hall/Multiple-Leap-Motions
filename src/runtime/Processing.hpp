#pragma once
#include "ControlSettings.hpp"
#include "TrackingSource.hpp"
#include "transport/osc/OscMessage.hpp"
#include <map>
#include <set>
#include <vector>

namespace handbridge {
// Stateful positional filters are isolated by serial/hand/channel. Source DTOs
// currently lack native hand IDs; loss/restart/settings changes reset all state.
class ProcessingPipeline {
public:
    void setSettings(const ProcessingSettings& settings);
    FrameData process(const FrameData& raw, Millis arrivalMs);
    std::vector<OscMessage> gestures(const FrameData& frame, const std::string& alias);
    void reset(const std::string& serial);
    void reset();
private:
    struct HandState {
        Millis lastMs = 0;
        bool initialized = false;
        std::map<std::string, Vector3> positions;
    };
    struct GestureState { bool pinch = false, grab = false; };
    ProcessingSettings settings_;
    std::map<std::string, std::map<std::string, HandState>> hands_;
    std::map<std::string, std::map<std::string, GestureState>> gestures_;
};
}
