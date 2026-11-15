#include "EngineRuntime.hpp"
#include <algorithm>
#include <cctype>
#include <stdexcept>

namespace handbridge {
const char* stateName(DeviceState state) {
    switch (state) {
    case DeviceState::Disconnected: return "disconnected";
    case DeviceState::Subscribed: return "subscribed";
    case DeviceState::Streaming: return "streaming";
    case DeviceState::Recovering: return "recovering";
    }
    return "unknown";
}
EngineRuntime::EngineRuntime(ITrackingSource& source, ConfigManager& config, Output output,
                             RuntimeOptions options, Log log)
    : source_(source), config_(config), output_(std::move(output)), options_(options), log_(std::move(log)),
      processor_(config.getDeviceAliasManager(), [this](const OscMessage& message) {
          ++outputStatus_.attempts; outputStatus_.lastAttemptMs = lastTickMs_;
          bool success = false;
          try { success = output_(message); } catch (const std::exception&) {}
          if (success) {
              ++outputStatus_.successes; outputStatus_.lastSuccessMs = lastTickMs_;

          } else {
              ++outputStatus_.errors;

          }

      }, [](const FrameData&) {}, nullptr) {
    if (!output_ || !options.staleMs || !options.retryInitialMs || options.retryMaxMs < options.retryInitialMs)
        throw std::invalid_argument("invalid runtime options/output");
    for (const auto& [serial, assignment] : config.getAllDefaultHandAssignments()) {
        auto filter = assignment;
        std::transform(filter.begin(), filter.end(), filter.begin(), [](unsigned char c) { return char(std::tolower(c)); });
        if (!filter.empty() && filter != "left" && filter != "right" && filter != "both" &&
            filter != "none" && filter != "disabled")
            throw std::invalid_argument("invalid hand assignment for " + serial);
    }
    processor_.setFilterSettings(config.isSendPalmEnabled(), config.isSendWristEnabled(),
        config.isSendThumbEnabled(), config.isSendIndexEnabled(), config.isSendMiddleEnabled(),
        config.isSendRingEnabled(), config.isSendPinkyEnabled(), config.isSendPalmOrientationEnabled(),
        config.isSendPalmVelocityEnabled(), config.isSendPalmNormalEnabled(), config.isSendVisibleTimeEnabled(),
        config.isSendFingerIsExtendedEnabled(), config.isSendPinchStrengthEnabled(), config.isSendGrabStrengthEnabled());
}
EngineRuntime::~EngineRuntime() { stop(); }
void EngineRuntime::start(Millis now) {
    if (running_) return;
    status_.clear(); pending_.clear(); outputStatus_ = {}; lastOutputFailureMs_.reset();
    running_ = true; serviceConnected_ = false; lastTickMs_ = now;
    serviceBackoffMs_ = options_.retryInitialMs;
    serviceRetryMs_ = now + serviceBackoffMs_;
    source_.start(now); // Connectivity is acknowledged by ServiceConnected, never inferred.
}
void EngineRuntime::transition(const std::string& serial, DeviceState state, const std::string& reason, Millis now) {
    auto& status = status_.at(serial);
    if (status.state == state && status.reason == reason) return;
    status.state = state; status.reason = reason; status.lastTransitionMs = now; ++status.transitions;
    if (log_) log_("device=" + status.alias + " state=" + stateName(state) + " reason=" + reason);
}
void EngineRuntime::invalidate(const std::string& serial) {
    auto& status = status_.at(serial);
    pending_.at(serial).frame.reset();
    status.framesFresh = status.handsVisible = false;
    FrameData lost; lost.deviceId = serial;
    processor_.processData(serial, lost); // Existing legacy once-per-hand loss packet contract.
}
FrameData EngineRuntime::selected(FrameData frame) const {
    auto filter = config_.getDefaultHandAssignment(frame.deviceId);
    std::transform(filter.begin(), filter.end(), filter.begin(), [](unsigned char c) { return char(std::tolower(c)); });
    // Legacy NONE/empty means unassigned/pass-through; disabled is explicit in this host.
    if (filter == "disabled") frame.hands.clear();
    else if (filter == "left" || filter == "right")
        std::erase_if(frame.hands, [&](const HandData& hand) { return hand.handType != filter; });
    return frame;
}
void EngineRuntime::event(const TrackingEvent& input, Millis now) {
    if (input.kind == TrackingEventKind::ServiceConnected) {
        serviceConnected_ = true; serviceBackoffMs_ = options_.retryInitialMs; return;
    }
    if (input.kind == TrackingEventKind::ServiceLost) {
        if (!serviceConnected_) return;
        serviceConnected_ = false; serviceRetryMs_ = now + options_.retryInitialMs;
        for (auto& [serial, status] : status_) {
            invalidate(serial); status.present = status.subscribed = false;
            pending_.at(serial).lastFrameId.reset();
            pending_.at(serial).activityMs = now;
            transition(serial, DeviceState::Recovering, "service_lost", now);
        }
        source_.stop(); return;
    }
    if (!serviceConnected_ || input.serial.empty()) return;
    if (!status_.contains(input.serial)) {
        if (input.kind != TrackingEventKind::DeviceConnected || status_.size() >= 64) return;
        status_[input.serial].alias = config_.getDeviceAliasManager().getOrAssignAlias(input.serial);
        pending_[input.serial].retryMs = options_.retryInitialMs;
    }
    auto& status = status_.at(input.serial);
    auto& pending = pending_.at(input.serial);
    switch (input.kind) {
    case TrackingEventKind::DeviceConnected:
        if (status.present) break; // Duplicate connect must not reset live frame sequence/subscription.
        status.present = true; pending.activityMs = now; pending.lastFrameId.reset();
        pending.nextRetryMs = now + options_.retryInitialMs;
        transition(input.serial, DeviceState::Recovering, "device_connected", now); break;
    case TrackingEventKind::DeviceLost:
        if (!status.present) break;
        invalidate(input.serial); status.present = status.subscribed = false;
        transition(input.serial, DeviceState::Disconnected, "device_lost", now); break;
    case TrackingEventKind::Subscribed:
        if (!status.present || status.subscribed) break;
        status.subscribed = true; pending.activityMs = now; pending.lastFrameId.reset();
        pending.retryMs = options_.retryInitialMs;
        transition(input.serial, DeviceState::Subscribed, "subscribed", now); break;
    case TrackingEventKind::SubscriptionFailed:
        invalidate(input.serial); status.subscribed = false;
        pending.nextRetryMs = now + options_.retryInitialMs;
        transition(input.serial, DeviceState::Recovering, "subscription_failed", now); break;
    case TrackingEventKind::Frame:
        if (!status.present || !status.subscribed) break;
        if (input.arrivalMs > now || now - input.arrivalMs >= options_.staleMs) { ++status.queueDrops; break; }
        if (pending.lastFrameId && input.frameId <= *pending.lastFrameId) { ++status.duplicateFrames; break; }
        pending.lastFrameId = input.frameId;
        status.lastFrameMs = input.arrivalMs; pending.activityMs = input.arrivalMs;
        status.lastFrameId = input.frameId; status.lastSourceTimestampUs = input.frame.timestamp;
        ++status.framesIn;
        if (pending.frame) ++status.queueDrops;
        pending.frame = input; break;
    default: break;
    }
}
void EngineRuntime::updateOutputHealth(std::uint64_t previousErrors) {
    if (outputStatus_.errors != previousErrors) lastOutputFailureMs_ = lastTickMs_;
    // Connected UDP can alternate success/ECONNREFUSED per packet. Aggregate at tick
    // boundaries and require 250ms without an error before reporting recovery.
    const bool degraded = lastOutputFailureMs_ &&
        (outputStatus_.lastSuccessMs < *lastOutputFailureMs_ ||
         outputStatus_.lastSuccessMs - *lastOutputFailureMs_ < 250);
    if (degraded != outputStatus_.degraded && log_)
        log_(degraded ? "output state=degraded reason=send_failed" : "output state=recovered");
    outputStatus_.degraded = degraded;
}
void EngineRuntime::tick(Millis now) {
    if (!running_) return;
    if (now < lastTickMs_) throw std::invalid_argument("monotonic time moved backwards");
    lastTickMs_ = now;
    const auto previousErrors = outputStatus_.errors;
    for (const auto& input : source_.poll(now)) event(input, now);
    if (!serviceConnected_) {
        if (now >= serviceRetryMs_) {
            source_.start(now);
            serviceBackoffMs_ = std::min(options_.retryMaxMs, serviceBackoffMs_ * 2);
            serviceRetryMs_ = now + serviceBackoffMs_;
        }
        updateOutputHealth(previousErrors);
        return;
    }
    for (auto& [serial, status] : status_) {
        auto& pending = pending_.at(serial);
        if (pending.frame) {
            auto frame = std::move(pending.frame->frame); pending.frame.reset();
            frame.deviceId = serial;
            status.handsVisible = !frame.hands.empty(); status.framesFresh = true;
            processor_.processData(serial, selected(std::move(frame))); ++status.framesProcessed;
            transition(serial, DeviceState::Streaming, "frames_fresh", now);
        }
        if (status.present && status.subscribed && now - pending.activityMs >= options_.staleMs) {
            invalidate(serial); status.subscribed = false;
            pending.nextRetryMs = now + options_.retryInitialMs;
            transition(serial, DeviceState::Recovering, "frame_stale", now);
        }
        if (status.present && !status.subscribed && now >= pending.nextRetryMs) {
            ++status.reconnectAttempts;
            if (source_.restartDevice(serial, now)) {
                status.subscribed = true; pending.lastFrameId.reset(); pending.activityMs = now;
                pending.retryMs = options_.retryInitialMs;
                transition(serial, DeviceState::Subscribed, "resubscribed", now);
            } else {
                pending.nextRetryMs = now + pending.retryMs;
                pending.retryMs = std::min(options_.retryMaxMs, pending.retryMs * 2);
                transition(serial, DeviceState::Recovering, "retry_failed", now);
            }
        }
    }
    updateOutputHealth(previousErrors);
}
void EngineRuntime::stop() {
    if (!running_) return;
    const auto previousErrors = outputStatus_.errors;
    for (auto& [serial, status] : status_) {
        invalidate(serial); status.subscribed = false;
        transition(serial, DeviceState::Disconnected, "stopped", lastTickMs_);
    }
    updateOutputHealth(previousErrors);
    source_.stop(); running_ = serviceConnected_ = false;
}
}
