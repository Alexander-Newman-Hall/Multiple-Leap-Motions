#pragma once
#include "TrackingSource.hpp"
#include "core/ConfigManager.h"
#include "pipeline/03_DataProcessor.hpp"
#include <functional>
#include <map>
#include <optional>

namespace handbridge {
enum class DeviceState { Disconnected, Subscribed, Streaming, Recovering };
const char* stateName(DeviceState state);
struct DeviceStatus {
    std::string alias, reason;
    DeviceState state = DeviceState::Disconnected;
    bool present = false, subscribed = false, framesFresh = false, handsVisible = false;
    std::optional<Millis> lastFrameMs;
    std::optional<std::uint64_t> lastFrameId;
    std::uint64_t lastSourceTimestampUs = 0;
    Millis lastTransitionMs = 0;
    std::uint64_t framesIn = 0, framesProcessed = 0, queueDrops = 0, duplicateFrames = 0;
    std::uint64_t reconnectAttempts = 0, transitions = 0;
};
struct RuntimeOptions { Millis staleMs = 500, retryInitialMs = 100, retryMaxMs = 1000; };
struct OutputStatus {
    std::uint64_t attempts = 0, successes = 0, errors = 0;
    bool degraded = false;
    Millis lastAttemptMs = 0, lastSuccessMs = 0;
};
class EngineRuntime {
public:
    using Output = std::function<bool(const OscMessage&)>;
    using Log = std::function<void(const std::string&)>;
    EngineRuntime(ITrackingSource& source, ConfigManager& config, Output output,
                  RuntimeOptions options = {}, Log log = {});
    ~EngineRuntime();
    void start(Millis now);
    void tick(Millis now);
    void stop();
    bool serviceConnected() const { return serviceConnected_; }
    const std::map<std::string, DeviceStatus>& devices() const { return status_; }
    const OutputStatus& outputStatus() const { return outputStatus_; }
private:
    struct Pending {
        std::optional<TrackingEvent> frame;
        std::optional<std::uint64_t> lastFrameId;
        Millis activityMs = 0, nextRetryMs = 0, retryMs = 100;
    };
    void event(const TrackingEvent& event, Millis now);
    void updateOutputHealth(std::uint64_t previousErrors);
    void invalidate(const std::string& serial);
    void transition(const std::string& serial, DeviceState state, const std::string& reason, Millis now);
    FrameData selected(FrameData frame) const;
    ITrackingSource& source_;
    ConfigManager& config_;
    Output output_;
    RuntimeOptions options_;
    Log log_;
    OutputStatus outputStatus_;
    DataProcessor processor_;
    std::map<std::string, DeviceStatus> status_;
    std::map<std::string, Pending> pending_;
    bool running_ = false, serviceConnected_ = false;
    Millis serviceRetryMs_ = 0, serviceBackoffMs_ = 100, lastTickMs_ = 0;
    std::optional<Millis> lastOutputFailureMs_;
};
}
