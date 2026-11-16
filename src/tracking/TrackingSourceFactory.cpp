#include "TrackingSourceFactory.hpp"
#include "runtime/MockTrackingSource.hpp"
#include <stdexcept>
#ifdef HANDBRIDGE_WITH_LEAPC
#include "LeapCLibrary.hpp"
#include "LeapCTrackingSource.hpp"
#endif

namespace handbridge {
namespace {
#ifdef HANDBRIDGE_WITH_LEAPC
class LoadedLeapCSource final : public ITrackingSource {
public:
    explicit LoadedLeapCSource(const std::string& path)
        : library_(path), source_(library_.api()) {}
    bool start(Millis now) override { return source_.start(now); }
    std::vector<TrackingEvent> poll(Millis now) override { return source_.poll(now); }
    bool restartDevice(const std::string& serial, Millis now) override {
        return source_.restartDevice(serial, now);
    }
    void stop() noexcept override { source_.stop(); }
    TrackingSourceDiagnostics diagnostics() const override {
        TrackingSourceDiagnostics value;
        value.lastError = source_.lastError();
        value.libraryPath = library_.info().path;
        value.libraryVersion = library_.info().version;
        value.serviceVersion = source_.serviceVersion();
        value.connectionAttempts = source_.connectionAttempts();
        value.pollErrors = source_.pollErrors();
        return value;
    }
private:
    LeapCLibrary library_;
    LeapCTrackingSource source_;
};
#endif
[[noreturn, maybe_unused]] void unavailable() {
    throw std::runtime_error("This build has no LeapC adapter. Configure HANDBRIDGE_WITH_LEAPC=ON "
        "with LEAP_SDK_ROOT pointing to the installed SDK, or use --simulate explicitly.");
}
}
const char* trackingSourceName(bool simulate) noexcept { return simulate ? "simulated" : "leapc"; }
bool leapCCompiled() noexcept {
#ifdef HANDBRIDGE_WITH_LEAPC
    return true;
#else
    return false;
#endif
}
LeapCProbe probeLeapC(const std::string& path) {
#ifdef HANDBRIDGE_WITH_LEAPC
    return LeapCLibrary(path).info();
#else
    (void)path;
    unavailable();
#endif
}
std::unique_ptr<ITrackingSource> createTrackingSource(bool simulate, bool stall,
                                                    const std::string& path) {
    if (simulate) {
        if (!path.empty()) throw std::invalid_argument("--leapc-library cannot be used with --simulate");
        return std::make_unique<MockTrackingSource>(stall);
    }
    if (stall) throw std::invalid_argument("The stall scenario requires --simulate");
#ifdef HANDBRIDGE_WITH_LEAPC
    return std::make_unique<LoadedLeapCSource>(path);
#else
    (void)path;
    unavailable();
#endif
}
}
