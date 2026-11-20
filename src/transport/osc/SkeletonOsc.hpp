#pragma once
#include "core/HandData.hpp"
#include "transport/osc/OscMessage.hpp"

namespace handbridge {
// Encode one raw LeapC observation. A null hand explicitly closes this identity.
OscMessage skeletonMessage(const std::string& alias, const std::string& side,
                           std::int64_t frameId, std::int64_t timestampUs,
                           std::optional<std::uint32_t> handId, const HandData* hand);
}
