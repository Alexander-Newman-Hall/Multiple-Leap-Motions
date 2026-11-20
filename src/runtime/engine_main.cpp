#include "EngineRuntime.hpp"
#include "tracking/TrackingSourceFactory.hpp"
#include "pipeline/04_OscSender.hpp"
#include "json.hpp"
#include <algorithm>
#include <charconv>
#include <cctype>
#include <chrono>
#include <csignal>
#include <iostream>
#include <filesystem>
#include <fstream>
#include <optional>
#include <thread>

namespace {
volatile std::sig_atomic_t stopped = 0;
void interrupt(int) { stopped = 1; }
std::uint64_t number(const std::string& value) {
    std::uint64_t parsed = 0;
    auto [end, error] = std::from_chars(value.data(), value.data() + value.size(), parsed);
    if (error != std::errc{} || end != value.data() + value.size())
        throw std::invalid_argument("expected unsigned integer: " + value);
    return parsed;
}
bool ipv4(const std::string& host) {
    std::size_t start = 0;
    for (int index = 0; index < 4; ++index) {
        const auto end = host.find('.', start);
        if ((index == 3) != (end == std::string::npos)) return false;
        const auto part = host.substr(start, end == std::string::npos ? end : end - start);
        if (part.empty() || number(part) > 255) return false;
        start = end + 1;
    }
    return true;
}
}
int main(int argc, char** argv) {
    try {
        bool simulate = false, stall = false, checkLeapC = false;
        std::uint64_t duration = 0;
        std::optional<std::string> configPath, host;
        std::string leapcLibrary;
        std::optional<int> port;
        for (int index = 1; index < argc; ++index) {
            const std::string flag = argv[index];
            auto value = [&]() -> std::string {
                if (++index >= argc) throw std::invalid_argument("missing value for " + flag);
                return argv[index];
            };
            if (flag == "--help") {
                std::cout << "handbridge-engine [--simulate] [--headless] [--config <schema-2-or-legacy.json>]\n"
                    "  [--host 127.0.0.1] [--port 9000] [--duration-ms N] [--scenario normal|stall]\n"
                    "  [--leapc-library <absolute installed-runtime path>] [--check-leapc]\n"
                    "Default: LeapC tracking when enabled at build time. --simulate selects mock cameras.\n"
                    "N=0 runs until Ctrl-C. UNTESTED ON HARDWARE.\n";
                return 0;
            } else if (flag == "--version") { std::cout << "Hand Bridge 2.0.7\n"; return 0; }
            else if (flag == "--simulate") simulate = true;
            else if (flag == "--check-leapc") checkLeapC = true;
            else if (flag == "--headless") {} // Always headless, accepted explicitly for scripts.
            else if (flag == "--config") configPath = value();
            else if (flag == "--host") host = value();
            else if (flag == "--leapc-library") leapcLibrary = value();
            else if (flag == "--port") {
                const auto parsed = number(value());
                if (parsed < 1 || parsed > 65535) throw std::invalid_argument("port must be 1..65535");
                port = static_cast<int>(parsed);
            } else if (flag == "--duration-ms") duration = number(value());
            else if (flag == "--scenario") {
                const auto scenario = value();
                if (scenario != "normal" && scenario != "stall") throw std::invalid_argument("unknown scenario");
                stall = scenario == "stall";
            } else throw std::invalid_argument("unknown argument: " + flag);
        }
        if (!simulate && stall) throw std::invalid_argument("--scenario stall requires --simulate");
        if (!leapcLibrary.empty() && (!std::filesystem::path(leapcLibrary).is_absolute() || simulate))
            throw std::invalid_argument("--leapc-library requires an absolute installed-library path and real tracking mode");
        if (checkLeapC) {
            if (simulate) throw std::invalid_argument("--check-leapc cannot be combined with --simulate");
            const auto probe = handbridge::probeLeapC(leapcLibrary);
            std::cout << nlohmann::json{{"provider", "leapc"}, {"library", probe.path},
                {"version", probe.version}, {"client_api_version", probe.apiVersion}, {"hardware_verified", false}}.dump() << '\n';
            return 0;
        }
        ConfigManager config;
        std::optional<handbridge::ControlSettings> profile;
        if (configPath) {
            std::ifstream input(*configPath);
            if (!input) throw std::invalid_argument("cannot read config: " + *configPath);
            const auto document = nlohmann::json::parse(input);
            if (document.contains("schema_version")) {
                profile = handbridge::ControlSettings::fromJson(document);
                config.setOscIp(profile->osc.host);
                config.setOscPort(profile->osc.port);
            } else if (!config.loadConfig(*configPath)) {
                throw std::invalid_argument("cannot load legacy config: " + *configPath);
            }
        }
        if (host) config.setOscIp(*host);
        if (port) config.setOscPort(*port);
        if (!ipv4(config.getOscIp()) || config.getOscPort() < 1 || config.getOscPort() > 65535)
            throw std::invalid_argument("OSC requires numeric IPv4 host and port 1..65535");
        for (const auto& [serial, assignment] : config.getAllDefaultHandAssignments()) {
            auto normalized = assignment;
            std::transform(normalized.begin(), normalized.end(), normalized.begin(), [](unsigned char c) { return char(std::tolower(c)); });
            if (!normalized.empty() && normalized != "left" && normalized != "right" && normalized != "both" &&
                normalized != "none" && normalized != "disabled")
                throw std::invalid_argument("invalid hand assignment for " + serial);
        }
        OscSender sender(config.getOscIp(), config.getOscPort());
        if (!sender.isInitialized()) throw std::runtime_error("cannot initialize OSC socket");
        auto source = handbridge::createTrackingSource(simulate, stall, leapcLibrary);
        if (!source) throw std::runtime_error("Tracking provider could not be created");
        handbridge::EngineRuntime runtime(*source, config,
            [&](const OscMessage& message) { return sender.sendOscMessageChecked(message); }, {},
            [](const std::string& message) { std::cerr << message << '\n'; });
        if (profile) {
            profile->osc.host = config.getOscIp();
            profile->osc.port = config.getOscPort();
            runtime.applySettings(*profile);
        }
        std::cerr << handbridge::trackingSourceName(simulate)
                  << " tracking; UNTESTED ON HARDWARE; UDP send success is not receiver acknowledgement\n";
        std::signal(SIGINT, interrupt); std::signal(SIGTERM, interrupt);
        const auto started = std::chrono::steady_clock::now();
        runtime.start(0);
        handbridge::Millis elapsed = 0;
        std::string lastTrackingError;
        while (!stopped) {
            elapsed = static_cast<handbridge::Millis>(std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - started).count());
            if (duration && elapsed >= duration) break;
            runtime.tick(elapsed);
            const auto tracking = source->diagnostics();
            if (tracking.lastError != lastTrackingError) {
                std::cerr << (tracking.lastError.empty() ? "tracking diagnostic cleared" : "tracking error=" + tracking.lastError) << '\n';
                lastTrackingError = tracking.lastError;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }
        // Summary describes live state just before clean shutdown; shutdown emits legacy loss once.
        nlohmann::json summary = {{"source", handbridge::trackingSourceName(simulate)}, {"simulated", simulate}, {"hardware_verified", false},
            {"service_connected", runtime.serviceConnected()}, {"destination", config.getOscIp()}, {"port", config.getOscPort()}};
        for (const auto& [serial, status] : runtime.devices()) {
            summary["devices"][serial] = {{"alias", status.alias}, {"state", handbridge::stateName(status.state)},
                {"present", status.present}, {"subscribed", status.subscribed}, {"frames_fresh", status.framesFresh},
                {"hands_visible", status.handsVisible}, {"frames_in", status.framesIn}, {"frames_processed", status.framesProcessed},
                {"queue_drops", status.queueDrops}, {"reconnect_attempts", status.reconnectAttempts}, {"reason", status.reason}};
            summary["devices"][serial]["last_frame_age_ms"] = status.lastFrameMs ? nlohmann::json(elapsed - *status.lastFrameMs) : nlohmann::json(nullptr);
            summary["devices"][serial]["last_frame_id"] = status.lastFrameId ? nlohmann::json(*status.lastFrameId) : nlohmann::json(nullptr);
            summary["devices"][serial]["last_source_timestamp_us"] = status.lastSourceTimestampUs;
        }
        const auto tracking = source->diagnostics();
        summary["tracking"] = {{"last_error", tracking.lastError}, {"library_path", tracking.libraryPath},
            {"library_version", tracking.libraryVersion}, {"service_version", tracking.serviceVersion},
            {"connection_attempts", tracking.connectionAttempts}, {"poll_errors", tracking.pollErrors}};
        runtime.stop();
        summary["output"] = {{"attempts", runtime.outputStatus().attempts}, {"successes", runtime.outputStatus().successes},
                              {"errors", runtime.outputStatus().errors}, {"degraded", runtime.outputStatus().degraded},
                              {"last_attempt_ms", runtime.outputStatus().lastAttemptMs}, {"last_success_ms", runtime.outputStatus().lastSuccessMs}};
        std::cout << summary.dump() << '\n';
        return runtime.outputStatus().errors ? 2 : 0;
    } catch (const std::exception& error) {
        std::cerr << "handbridge-engine: " << error.what() << '\n'; return 1;
    }
}
