#pragma once
#include "core/FrameData.hpp"
#include <cstdint>
#include <string>
#include <vector>

namespace handbridge {
using Millis = std::uint64_t; // Injected monotonic clock, never LeapC's timestamp domain.
enum class TrackingEventKind { ServiceConnected, ServiceLost, DeviceConnected, DeviceLost,
                               Subscribed, SubscriptionFailed, Frame };
struct TrackingEvent {
    TrackingEventKind kind;
    std::string serial;
    std::uint64_t frameId = 0;
    Millis arrivalMs = 0;
    FrameData frame; // Owned raw observations; position mm, velocity mm/s.
};
// Methods are called by one runtime owner thread. poll must be bounded/nonblocking.
// Frame/device lifetime is owned by the returned values, never by SDK event buffers.
class ITrackingSource {
public:
    virtual ~ITrackingSource() = default;
    // A retry before ServiceConnected must be idempotent. A new service session
    // must discard old SDK buffers and rediscover devices before reporting frames.
    virtual bool start(Millis now) = 0;
    virtual std::vector<TrackingEvent> poll(Millis now) = 0;
    // true means a fresh subscription was established; frame delivery is separate.
    virtual bool restartDevice(const std::string& serial, Millis now) = 0;
    virtual void stop() noexcept = 0;
};
}
