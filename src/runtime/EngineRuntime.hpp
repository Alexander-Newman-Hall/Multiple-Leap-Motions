#pragma once
#include "TrackingSource.hpp"
#include "ControlSettings.hpp"
#include "Processing.hpp"
#include "core/ConfigManager.h"
#include "pipeline/03_DataProcessor.hpp"
#include <functional>
#include <map>
#include <optional>
#include <memory>
#include <set>

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
struct RuntimeOptions {
    Millis staleMs = 500, retryInitialMs = 100, retryMaxMs = 1000;
    Millis firstFrameTimeoutMs = 3000;
};
struct OutputStatus {
    std::uint64_t attempts = 0, successes = 0, errors = 0;
    bool degraded = false;
    Millis lastAttemptMs = 0, lastSuccessMs = 0;
};
struct FramePreview { FrameData raw, processed; Millis arrivalMs = 0; };
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
    void applySettings(const ControlSettings& settings);
    const ControlSettings& settings() const { return settings_; }
    const std::map<std::string, FramePreview>& previews() const { return previews_; }
    bool restartDevice(const std::string& serial, Millis now);
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
    void emitOutput(const OscMessage& message);
    void clearOutput(const std::string& serial);
    void processFrame(const std::string& serial, FrameData frame, Millis arrivalMs, Millis now);
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
    ControlSettings settings_;
    ProcessingPipeline processing_;
    std::unique_ptr<DataProcessor> processor_;
    std::map<std::string, FramePreview> previews_;
    std::map<std::string, Millis> lastOutputMs_;
    std::map<std::string, std::set<std::string>> emittedHands_;
    std::map<std::string, DeviceStatus> status_;
    std::map<std::string, Pending> pending_;
    bool running_ = false, serviceConnected_ = false;
    Millis serviceRetryMs_ = 0, serviceBackoffMs_ = 100, lastTickMs_ = 0;
    std::optional<Millis> lastOutputFailureMs_;
};
}
