#include "EngineRuntime.hpp"
#include "transport/osc/SkeletonOsc.hpp"
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
    : source_(source), config_(config), output_(std::move(output)), options_(options), log_(std::move(log)) {
    if (!output_ || !options.staleMs || !options.firstFrameTimeoutMs ||
        !options.retryInitialMs || options.retryMaxMs < options.retryInitialMs)
        throw std::invalid_argument("invalid runtime options/output");
    applySettings(ControlSettings::fromLegacy(config));
}
void EngineRuntime::emitOutput(const OscMessage& message) {
    if (!settings_.osc.enabled) return;
    ++outputStatus_.attempts; outputStatus_.lastAttemptMs = lastTickMs_;
    bool success = false;
    try { success = output_(message); } catch (const std::exception&) {}
    if (success) { ++outputStatus_.successes; outputStatus_.lastSuccessMs = lastTickMs_; }
    else { ++outputStatus_.errors; lastOutputFailureMs_ = lastTickMs_; }
}
void EngineRuntime::emitSkeletonLoss(const DeviceStatus& device, const SkeletonHand& hand) {
    emitOutput(skeletonMessage(device.alias, hand.first, device.lastFrameId.value(),
                               device.lastSourceTimestampUs, hand.second, nullptr));
}
void EngineRuntime::clearLegacyOutput(const std::string& serial) {
    FrameData lost; lost.deviceId = serial;
    if (processor_) processor_->processData(serial, lost);
    const auto alias = config_.getDeviceAliasManager().getOrAssignAlias(serial);
    for (const auto& message : processing_.gestures(lost, alias)) emitOutput(message);
    processing_.reset(serial);
    emittedHands_.erase(serial); lastOutputMs_.erase(serial);
}
void EngineRuntime::clearSkeletonOutput(const std::string& serial) {
    if (const auto skeletons = emittedSkeletons_.find(serial); skeletons != emittedSkeletons_.end()) {
        for (const auto& hand : skeletons->second) emitSkeletonLoss(status_.at(serial), hand);
        emittedSkeletons_.erase(skeletons);
    }
}
void EngineRuntime::applySettings(const ControlSettings& requested) {
    const auto previousErrors = outputStatus_.errors;
    ControlSettings next = requested;
    // Profiles may omit physical identities. Preserve active bindings, never
    // silently assign a running camera another camera's OSC address.
    for (const auto& [serial, status] : status_) {
        if (!next.devices.contains(serial)) {
            const auto existing = settings_.devices.find(serial);
            next.devices[serial] = existing == settings_.devices.end()
                ? DeviceSettings{status.alias,true,"both"} : existing->second;
        }
    }
    next.validate();
    nlohmann::json aliases = nlohmann::json::object();
    std::map<std::string,std::string> assignments;
    for (const auto& [serial, device] : next.devices) {
        aliases[serial] = device.alias;
        assignments[serial] = device.enabled ? device.handFilter : "disabled";
    }
    DeviceAliasManager candidateAliases;
    candidateAliases.loadFromJson({{"deviceAliases",aliases}});
    auto nextProcessor = std::make_unique<DataProcessor>(config_.getDeviceAliasManager(),
        [this](const OscMessage& message) { emitOutput(message); }, [](const FrameData&) {}, nullptr);
    const auto& c = next.channels;
    nextProcessor->setFilterSettings(c.sendPalm,c.sendWrist,c.sendThumb,c.sendIndex,c.sendMiddle,c.sendRing,c.sendPinky,
        c.sendPalmOrientation,c.sendPalmVelocity,c.sendPalmNormal,c.sendVisibleTime,c.sendFingerIsExtended,
        c.sendPinchStrength,c.sendGrabStrength);
    // All validation/allocation above precedes observable changes. Routing/mute
    // changes clear old addresses through the old sender before the host swaps it.
    const bool globalRoutingChanged = settings_.osc.enabled != next.osc.enabled ||
        settings_.osc.host != next.osc.host || settings_.osc.port != next.osc.port;
    auto previousScalarChannels = settings_.channels;
    previousScalarChannels.sendFullSkeleton = c.sendFullSkeleton;
    const bool scalarChannelsChanged = previousScalarChannels != c;
    const bool skeletonDisabled = settings_.channels.sendFullSkeleton && !c.sendFullSkeleton;
    const bool processingChanged = settings_.processing != next.processing;
    for (const auto& [serial, status] : status_) {
        (void)status;
        const auto oldDevice = settings_.devices.find(serial);
        const auto newDevice = next.devices.find(serial);
        const bool routingChanged = oldDevice == settings_.devices.end() || newDevice == next.devices.end() ||
            oldDevice->second.alias != newDevice->second.alias || oldDevice->second.enabled != newDevice->second.enabled ||
            oldDevice->second.handFilter != newDevice->second.handFilter;
        if (globalRoutingChanged || routingChanged || skeletonDisabled) clearSkeletonOutput(serial);
        if (globalRoutingChanged || routingChanged || scalarChannelsChanged ||
            (settings_.processing.gesturesEnabled && !next.processing.gesturesEnabled))
            clearLegacyOutput(serial);
        else if (processingChanged && settings_.processing.gesturesEnabled) {
            FrameData empty; empty.deviceId = serial;
            for (const auto& message : processing_.gestures(empty,status.alias)) emitOutput(message);
        }
    }
    config_.getDeviceAliasManager().loadFromJson({{"deviceAliases",aliases}});
    config_.setAllDefaultHandAssignments(assignments);
    config_.setOscIp(next.osc.host); config_.setOscPort(next.osc.port);
    config_.setSendPalmEnabled(c.sendPalm); config_.setSendWristEnabled(c.sendWrist);
    config_.setSendThumbEnabled(c.sendThumb); config_.setSendIndexEnabled(c.sendIndex);
    config_.setSendMiddleEnabled(c.sendMiddle); config_.setSendRingEnabled(c.sendRing); config_.setSendPinkyEnabled(c.sendPinky);
    config_.setSendPalmOrientationEnabled(c.sendPalmOrientation); config_.setSendPalmVelocityEnabled(c.sendPalmVelocity);
    config_.setSendPalmNormalEnabled(c.sendPalmNormal); config_.setSendVisibleTimeEnabled(c.sendVisibleTime);
    config_.setSendFingerIsExtendedEnabled(c.sendFingerIsExtended); config_.setSendPinchStrengthEnabled(c.sendPinchStrength);
    config_.setSendGrabStrengthEnabled(c.sendGrabStrength);
    settings_ = std::move(next);
    if (!processor_) processor_ = std::move(nextProcessor);
    else {
        const auto& active = settings_.channels;
        processor_->setFilterSettings(active.sendPalm,active.sendWrist,active.sendThumb,active.sendIndex,active.sendMiddle,active.sendRing,active.sendPinky,
            active.sendPalmOrientation,active.sendPalmVelocity,active.sendPalmNormal,active.sendVisibleTime,active.sendFingerIsExtended,
            active.sendPinchStrength,active.sendGrabStrength);
    }
    if (processingChanged) processing_.setSettings(settings_.processing);
    lastOutputMs_.clear(); previews_.clear();
    for (auto& [serial, status] : status_) status.alias = settings_.devices.at(serial).alias;
    updateOutputHealth(previousErrors);
}
void EngineRuntime::processFrame(const std::string& serial, FrameData frame, Millis arrivalMs, Millis now) {
    const auto selectedRaw = selected(frame);
    FrameData processed = processing_.process(selectedRaw,arrivalMs);
    previews_[serial] = {std::move(frame),processed,arrivalMs};
    if (!settings_.osc.enabled) return;
    std::set<std::string> current;
    for (const auto& hand : processed.hands) current.insert(hand.handType);
    const auto& previous = emittedHands_[serial];
    const bool lostHand = std::any_of(previous.begin(),previous.end(),[&](const auto& hand) { return !current.contains(hand); });
    const auto last = lastOutputMs_.find(serial);
    const bool due = settings_.osc.rateHz == 0 || last == lastOutputMs_.end() ||
        double(now-last->second)*settings_.osc.rateHz >= 1000;
    const auto& device = status_.at(serial);
    if (settings_.channels.sendFullSkeleton) {
        SkeletonHands currentSkeletons;
        for (const auto& hand : selectedRaw.hands)
            if (hand.isValid()) currentSkeletons.emplace(hand.handType, hand.nativeHandId);
        auto& previousSkeletons = emittedSkeletons_[serial];
        bool lostSkeleton = false;
        for (const auto& hand : previousSkeletons) {
            if (!currentSkeletons.contains(hand)) {
                emitSkeletonLoss(device, hand);
                lostSkeleton = true;
            }
        }
        // Deliver a replacement identity immediately, even within a rate-limited interval.
        if (due || lostHand || lostSkeleton) {
            for (const auto& hand : selectedRaw.hands) {
                if (hand.isValid()) emitOutput(skeletonMessage(device.alias, hand.handType, device.lastFrameId.value(),
                    selectedRaw.timestamp, hand.nativeHandId, &hand));
            }
            previousSkeletons = std::move(currentSkeletons);
        }
    }
    if (!due && !lostHand) return;
    processor_->processData(serial,processed);
    for (const auto& message : processing_.gestures(processed,status_.at(serial).alias)) emitOutput(message);
    emittedHands_[serial] = std::move(current); lastOutputMs_[serial] = now;
}
bool EngineRuntime::restartDevice(const std::string& serial, Millis now) {
    if (now < lastTickMs_) throw std::invalid_argument("monotonic time moved backwards");
    const auto found = status_.find(serial);
    if (!running_ || !serviceConnected_ || found == status_.end() || !found->second.present) return false;
    lastTickMs_ = now;
    const auto previousErrors = outputStatus_.errors;
    invalidate(serial);
    auto& status = found->second;
    auto& pending = pending_.at(serial);
    status.subscribed = false; ++status.reconnectAttempts;
    pending.lastFrameId.reset(); pending.activityMs = now; pending.retryMs = options_.retryInitialMs;
    pending.nextRetryMs = now + options_.retryInitialMs;
    const bool restored = source_.restartDevice(serial,now);
    status.subscribed = restored;
    transition(serial,restored ? DeviceState::Subscribed : DeviceState::Recovering,
        restored ? "manual_restart_subscribed" : "manual_restart_failed",now);
    updateOutputHealth(previousErrors);
    return restored;
}
EngineRuntime::~EngineRuntime() { stop(); }
void EngineRuntime::start(Millis now) {
    if (running_) return;
    status_.clear(); pending_.clear(); previews_.clear(); processing_.reset();
    emittedHands_.clear(); emittedSkeletons_.clear(); lastOutputMs_.clear(); outputStatus_ = {}; lastOutputFailureMs_.reset();
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
    clearLegacyOutput(serial);
    clearSkeletonOutput(serial);
    FrameData empty; empty.deviceId = serial;
    previews_[serial] = {empty,empty,lastTickMs_};
}
FrameData EngineRuntime::selected(FrameData frame) const {
    const auto device = settings_.devices.find(frame.deviceId);
    if (device != settings_.devices.end() && !device->second.enabled) { frame.hands.clear(); return frame; }
    auto filter = device == settings_.devices.end() ? config_.getDefaultHandAssignment(frame.deviceId) : device->second.handFilter;
    std::transform(filter.begin(),filter.end(),filter.begin(),[](unsigned char c) { return char(std::tolower(c)); });
    if (filter == "disabled") frame.hands.clear();
    else if (filter == "left" || filter == "right")
        std::erase_if(frame.hands,[&](const HandData& hand) { return hand.handType != filter; });
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
        if (!settings_.devices.contains(input.serial)) {
            auto hand = config_.getDefaultHandAssignment(input.serial);
            std::transform(hand.begin(),hand.end(),hand.begin(),[](unsigned char c) { return char(std::tolower(c)); });
            if (hand.empty() || hand == "none") hand = "both";
            settings_.devices[input.serial] = {status_.at(input.serial).alias,true,hand};
        }
        pending_.try_emplace(input.serial);
    }
    auto& status = status_.at(input.serial);
    auto& pending = pending_.at(input.serial);
    switch (input.kind) {
    case TrackingEventKind::DeviceConnected:
        if (status.present) break; // Duplicate connect must not reset live frame sequence/subscription.
        status.present = true; pending.activityMs = now; pending.lastFrameId.reset();
        pending.retryMs = options_.retryInitialMs;
        pending.nextRetryMs = now + pending.retryMs;
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
            const auto arrivalMs = pending.frame->arrivalMs;
            auto frame = std::move(pending.frame->frame); pending.frame.reset();
            frame.deviceId = serial;
            status.handsVisible = !frame.hands.empty(); status.framesFresh = true;
            processFrame(serial,std::move(frame),arrivalMs,now); ++status.framesProcessed;
            transition(serial, DeviceState::Streaming, "frames_fresh", now);
        }
        const bool awaitingFirstFrame = !pending.lastFrameId.has_value();
        const Millis frameTimeoutMs = awaitingFirstFrame ? options_.firstFrameTimeoutMs : options_.staleMs;
        if (status.present && status.subscribed && now - pending.activityMs >= frameTimeoutMs) {
            invalidate(serial); status.subscribed = false;
            pending.nextRetryMs = now + options_.retryInitialMs;
            transition(serial, DeviceState::Recovering, awaitingFirstFrame ? "first_frame_timeout" : "frame_stale", now);
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
