#pragma once
#include "LeapCApi.hpp"
#include "TrackingSourceFactory.hpp"

namespace handbridge {
// Declared before the source in its owning wrapper so the DLL outlives every
// connection/device handle and every function pointer copied from it.
class LeapCLibrary {
public:
    explicit LeapCLibrary(const std::string& path);
    ~LeapCLibrary();
    LeapCLibrary(const LeapCLibrary&) = delete;
    LeapCLibrary& operator=(const LeapCLibrary&) = delete;
    const LeapCApi& api() const noexcept { return api_; }
    const LeapCProbe& info() const noexcept { return info_; }
private:
    void* module_ = nullptr;
    LeapCApi api_{};
    LeapCProbe info_;
};
}
