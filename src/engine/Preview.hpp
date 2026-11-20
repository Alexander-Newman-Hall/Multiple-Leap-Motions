#pragma once
#include "core/FrameData.hpp"
#include <QJsonArray>
#include <QJsonObject>

namespace handbridge::engine {
inline QJsonArray previewHands(const FrameData& frame) {
    const auto vector = [](const Vector3& value) { return QJsonArray{value.x, value.y, value.z}; };
    QJsonArray hands;
    for (const auto& hand : frame.hands) {
        QJsonArray tips;
        for (const auto& finger : hand.fingers) {
            if (finger.isValid() && finger.bones.size() > 3 && finger.bones[3].isValid())
                tips.push_back(vector(finger.bones[3].nextJoint));
        }
        hands.push_back(QJsonObject{{"palm", vector(hand.palm.position)},
            {"wrist", hand.arm.isValid() ? QJsonValue(vector(hand.arm.wristPosition)) : QJsonValue(QJsonValue::Null)},
            {"tips", tips}, {"hand", QString::fromStdString(hand.handType)},
            {"pinch", hand.pinchStrength}, {"grab", hand.grabStrength}});
    }
    return hands;
}
}
