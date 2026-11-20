#pragma once
#include <cstdint>
#include <string>
#include <variant>
#include <vector>

using OscArgument = std::variant<std::int32_t, std::int64_t, float>;

struct OscMessage {
    std::string address;
    std::vector<float> values; // Legacy: each value goes in a separate datagram.
    std::vector<OscArgument> arguments{}; // Typed: all arguments go in one datagram.
};
