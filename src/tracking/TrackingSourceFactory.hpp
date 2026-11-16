#pragma once
#include "runtime/TrackingSource.hpp"
#include <memory>
#include <string>

namespace handbridge {
struct LeapCProbe {
    std::string path;
    std::string version;
    std::string apiVersion;
};
// Simulation is always explicit. A hardware build loads the user's installed
// LeapC client; no proprietary DLL is embedded in Hand Bridge.
std::unique_ptr<ITrackingSource> createTrackingSource(bool simulate, bool stall = false,
                                                    const std::string& leapcLibrary = {});
const char* trackingSourceName(bool simulate) noexcept;
bool leapCCompiled() noexcept;
LeapCProbe probeLeapC(const std::string& leapcLibrary = {});
}
