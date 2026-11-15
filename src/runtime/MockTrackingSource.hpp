#pragma once
#include "TrackingSource.hpp"
#include <array>

namespace handbridge {
class MockTrackingSource final : public ITrackingSource {
public:
    explicit MockTrackingSource(bool stallScenario = false) : stallScenario_(stallScenario) {}
    bool start(Millis now) override;
    std::vector<TrackingEvent> poll(Millis now) override;
    bool restartDevice(const std::string& serial, Millis now) override;
    void stop() noexcept override { running_ = false; }
private:
    bool running_ = false, announce_ = false, stallScenario_ = false;
    Millis started_ = 0, nextFrame_ = 0;
    std::array<std::uint64_t, 2> sequence_{};
};
}
