#pragma once
#include <LeapC.h>

namespace handbridge {
// Signatures and calling conventions come exclusively from the installed/pinned
// official SDK header. The owner of the loaded library must outlive this table
// and every LeapCTrackingSource that uses it. No SDK implementation is emulated.
struct LeapCApi {
    decltype(&::LeapCreateConnection) createConnection = nullptr;
    decltype(&::LeapOpenConnection) openConnection = nullptr;
    decltype(&::LeapPollConnection) pollConnection = nullptr;
    decltype(&::LeapGetVersion) getVersion = nullptr;
    decltype(&::LeapGetDeviceList) getDeviceList = nullptr;
    decltype(&::LeapOpenDevice) openDevice = nullptr;
    decltype(&::LeapGetDeviceInfo) getDeviceInfo = nullptr;
    decltype(&::LeapSubscribeEvents) subscribeEvents = nullptr;
    decltype(&::LeapUnsubscribeEvents) unsubscribeEvents = nullptr;
    decltype(&::LeapCloseDevice) closeDevice = nullptr;
    decltype(&::LeapCloseConnection) closeConnection = nullptr;
    decltype(&::LeapDestroyConnection) destroyConnection = nullptr;

    bool complete() const noexcept {
        return createConnection && openConnection && pollConnection && getVersion && getDeviceList && openDevice &&
               getDeviceInfo && subscribeEvents && unsubscribeEvents && closeDevice && closeConnection && destroyConnection;
    }
};
}
