#pragma once
#include "LeapCApi.hpp"
#include "runtime/TrackingSource.hpp"
#include <functional>
#include <map>
#include <string>
#include <thread>

namespace handbridge {
class LeapCTrackingSource final : public ITrackingSource {
public:
    using Log = std::function<void(const std::string&)>;
    explicit LeapCTrackingSource(LeapCApi api, Log log = {});
    ~LeapCTrackingSource() override;
    LeapCTrackingSource(const LeapCTrackingSource&) = delete;
    LeapCTrackingSource& operator=(const LeapCTrackingSource&) = delete;

    bool start(Millis now) override;
    std::vector<TrackingEvent> poll(Millis now) override;
    bool restartDevice(const std::string& serial, Millis now) override;
    void stop() noexcept override;
    const std::string& lastError() const noexcept { return lastError_; }
    const std::string& serviceVersion() const noexcept { return serviceVersion_; }
    std::uint64_t connectionAttempts() const noexcept { return connectionAttempts_; }
    std::uint64_t pollErrors() const noexcept { return pollErrors_; }

private:
    struct Device {
        LEAP_DEVICE_REF reference{};
        LEAP_DEVICE handle = nullptr;
        std::string serial;
        bool subscribed = false, announced = false, duplicate = false;
        Millis nextRetryMs = 0, backoffMs = 100;
    };
    bool owner(const char* operation);
    void error(const std::string& operation, eLeapRS result);
    void error(const std::string& message);
    void release(Device& device) noexcept;
    void cleanup() noexcept;
    void loseService(const std::string& reason);
    void loseDevice(std::uint32_t id);
    bool enumerate(Millis now);
    bool verifyServiceVersion();
    bool openAndSubscribe(std::uint32_t id, Millis now);
    std::string serial(LEAP_DEVICE handle);
    bool copyTracking(const LEAP_TRACKING_EVENT& tracking, const std::string& serial, Millis now);
    void add(TrackingEventKind kind, const std::string& serial = {});

    LeapCApi api_;
    Log log_;
    LEAP_CONNECTION connection_ = nullptr;
    std::map<std::uint32_t, Device> devices_;
    std::vector<TrackingEvent> events_;
    std::thread::id owner_{};
    bool connected_ = false, opening_ = false;
    Millis openedMs_ = 0, nextEnumerationMs_ = 0;
    std::string lastError_, serviceVersion_;
    std::uint64_t connectionAttempts_ = 0, pollErrors_ = 0;
};
}
