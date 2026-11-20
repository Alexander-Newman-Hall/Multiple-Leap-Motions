#include "SkeletonOsc.hpp"
#include <cmath>

namespace handbridge {
namespace {
using Arguments = std::vector<OscArgument>;

bool finite(Vector3 v) {
    return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
}

bool validRotation(Quaternion q) {
    // LeapC supplies unit quaternions. Preserve its components and tolerate rounding.
    const double normSquared = double(q.w)*q.w + double(q.x)*q.x + double(q.y)*q.y + double(q.z)*q.z;
    return std::abs(normSquared - 1.0) <= 0.001;
}

float widthOrZero(float width) {
    return std::isfinite(width) && width >= 0 ? width : 0.f;
}

void appendRotation(Arguments& args, Quaternion q) {
    if (validRotation(q)) args.insert(args.end(), {q.w, q.x, q.y, q.z});
    else args.insert(args.end(), 4, 0.f);
}

void appendSegment(Arguments& args, Vector3 start, Vector3 end, Quaternion q, float width) {
    args.insert(args.end(), {start.x, start.y, start.z, end.x, end.y, end.z});
    appendRotation(args, q);
    args.emplace_back(widthOrZero(width));
}

float finiteOrZero(float value) { return std::isfinite(value) ? value : 0.f; }

void appendPalmVector(Arguments& args, Vector3 v) {
    if (!finite(v)) v = {};
    args.insert(args.end(), {v.x, v.y, v.z});
}

void appendDigit(Arguments& args, const FingerData* digit) {
    args.emplace_back(std::int32_t(digit ? digit->fingerId : -1));
    args.emplace_back(std::int32_t(digit && digit->isExtended));
    const auto maskIndex = args.size();
    std::int32_t mask = 0;
    args.emplace_back(mask);
    for (int i = 0; i < 4; ++i) {
        if (digit && static_cast<std::size_t>(i) < digit->bones.size()) {
            const auto& bone = digit->bones[i];
            if (bone.isValid() && finite(bone.prevJoint) && finite(bone.nextJoint)) {
                mask |= 1 << i;
                appendSegment(args, bone.prevJoint, bone.nextJoint, bone.rotation, bone.width);
                continue;
            }
        }
        args.insert(args.end(), 11, 0.f);
    }
    args[maskIndex] = mask;
}
}

OscMessage skeletonMessage(const std::string& alias, const std::string& side,
                           std::int64_t frameId, std::int64_t timestampUs,
                           std::optional<std::uint32_t> handId, const HandData* hand) {
    OscMessage message;
    message.address = "/handbridge/v2/" + alias + "/" + side + "/skeleton";
    auto& args = message.arguments;
    args.reserve(hand ? 278 : 4);
    args.emplace_back(std::int32_t(hand != nullptr));
    args.emplace_back(frameId);
    args.emplace_back(timestampUs);
    args.emplace_back(handId ? std::int64_t(*handId) : std::int64_t(-1));
    if (!hand) return message;

    args.emplace_back(static_cast<std::int64_t>(hand->visibleTime));
    args.emplace_back(static_cast<std::int32_t>(hand->flags));
    args.insert(args.end(), {finiteOrZero(hand->confidence), finiteOrZero(hand->pinchDistance),
                             finiteOrZero(hand->grabAngle), finiteOrZero(hand->pinchStrength),
                             finiteOrZero(hand->grabStrength)});
    const auto& palm = hand->palm;
    appendPalmVector(args, palm.position);
    appendPalmVector(args, palm.stabilizedPosition);
    appendPalmVector(args, palm.velocity);
    appendPalmVector(args, palm.normal);
    appendPalmVector(args, palm.direction);
    appendRotation(args, palm.orientation);
    args.emplace_back(widthOrZero(palm.width));

    const auto& arm = hand->arm;
    const bool armValid = arm.isValid() && finite(arm.elbowPosition) && finite(arm.wristPosition);
    args.emplace_back(std::int32_t(armValid));
    if (armValid) appendSegment(args, arm.elbowPosition, arm.wristPosition, arm.rotation, arm.width);
    else args.insert(args.end(), 11, 0.f);
    for (std::size_t i = 0; i < 5; ++i) {
        const auto* digit = i < hand->fingers.size() && hand->fingers[i].isValid() ? &hand->fingers[i] : nullptr;
        appendDigit(args, digit);
    }
    return message;
}
}
