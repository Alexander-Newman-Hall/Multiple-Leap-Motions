// Owned feasibility-only client; no vendor sample implementation is copied.
#include <LeapC.h>
#include <dlfcn.h>
#include <algorithm>
#include <chrono>
#include <csignal>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <map>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace {
using Clock = std::chrono::steady_clock;
volatile std::sig_atomic_t interrupted = 0;
void interrupt(int) { interrupted = 1; }
struct Library {
    void* handle = nullptr;
    explicit Library(const char* path) {
        if (!std::filesystem::path(path).is_absolute())
            throw std::runtime_error("library path must be absolute");
        handle = dlopen(path, RTLD_NOW | RTLD_LOCAL);
        if (!handle) throw std::runtime_error(dlerror());
    }
    ~Library() { if (handle) dlclose(handle); }
    Library(const Library&) = delete;
    Library& operator=(const Library&) = delete;
    template<class T> T symbol(const char* name) {
        auto raw = dlsym(handle, name);
        if (!raw) throw std::runtime_error(std::string("missing export: ") + name);
        T function{};
        static_assert(sizeof(function) == sizeof(raw));
        std::memcpy(&function, &raw, sizeof(raw));
        return function;
    }
};
#define API(name) decltype(&::name) name = library.symbol<decltype(&::name)>(#name)
struct Api {
    Library& library;
    API(LeapCreateConnection); API(LeapOpenConnection); API(LeapPollConnection);
    API(LeapGetVersion); API(LeapGetDeviceList); API(LeapOpenDevice);
    API(LeapGetDeviceInfo); API(LeapSubscribeEvents); API(LeapUnsubscribeEvents);
    API(LeapCloseDevice); API(LeapCloseConnection); API(LeapDestroyConnection);
};
#undef API
bool accepted(const LEAP_VERSION& v) { return v.major == 6 && v.minor >= 2 && v.patch >= 0; }
void version(const char* label, eLeapRS result, const LEAP_VERSION& v) {
    std::cout << label << " result=" << static_cast<unsigned>(result);
    if (result == eLeapRS_Success)
        std::cout << " version=" << v.major << '.' << v.minor << '.' << v.patch;
    else std::cout << " version=unavailable";
    std::cout << '\n';
}
struct Device {
    LEAP_DEVICE handle{};
    std::string serial;
    bool subscribed = false;
    std::uint64_t frames = 0;
    std::uint32_t hands = 0;
    Clock::time_point last{};
};
struct Session {
    Api& api;
    LEAP_CONNECTION connection{};
    std::map<std::uint32_t, Device> devices;
    explicit Session(Api& functions) : api(functions) {}
    ~Session() {
        clear();
        if (connection) {
            api.LeapCloseConnection(connection);
            api.LeapDestroyConnection(connection);
        }
    }
    void remove(std::uint32_t id) {
        const auto it = devices.find(id);
        if (it == devices.end()) return;
        if (it->second.subscribed) {
            const auto result = api.LeapUnsubscribeEvents(connection, it->second.handle);
            if (result != eLeapRS_Success) std::cout << "unsubscribe error=" << static_cast<unsigned>(result) << '\n';
        }
        if (it->second.handle) api.LeapCloseDevice(it->second.handle);
        devices.erase(it);
    }
    void clear() { while (!devices.empty()) remove(devices.begin()->first); }
    void add(LEAP_DEVICE_REF ref) {
        if (devices.contains(ref.id) || devices.size() >= 64) return;
        // Allocate the owning record before opening an SDK handle. Allocation
        // failure must not leave a live handle outside Session's cleanup scope.
        auto& owned = devices.try_emplace(ref.id).first->second;
        const auto opened = api.LeapOpenDevice(ref, &owned.handle);
        if (opened != eLeapRS_Success || !owned.handle) {
            std::cout << "open id=" << ref.id << " error=" << static_cast<unsigned>(opened) << '\n';
            remove(ref.id);
            return;
        }
        eLeapRS result{};
        LEAP_DEVICE_INFO info{};
        info.size = sizeof(info);
        result = api.LeapGetDeviceInfo(owned.handle, &info);
        if ((result != eLeapRS_Success && result != eLeapRS_InsufficientBuffer) ||
            info.serial_length < 2 || info.serial_length > 4096) {
            std::cout << "serial-size error=" << static_cast<unsigned>(result) << '\n';
            remove(ref.id); return;
        }
        std::vector<char> serial(info.serial_length, '\0');
        info.serial = serial.data();
        result = api.LeapGetDeviceInfo(owned.handle, &info);
        const auto end = std::find(serial.begin(), serial.end(), '\0');
        if (result != eLeapRS_Success || end == serial.begin() || end == serial.end()) {
            std::cout << "serial-read error=" << static_cast<unsigned>(result) << '\n';
            remove(ref.id); return;
        }
        owned.serial.assign(serial.begin(), end);
        for (const auto& [id, other] : devices) {
            if (id != ref.id && other.serial == owned.serial) {
                std::cout << "duplicate serial rejected id=" << ref.id << '\n';
                remove(ref.id); return;
            }
        }
        result = api.LeapSubscribeEvents(connection, owned.handle);
        owned.subscribed = result == eLeapRS_Success;
        std::cout << "device id=" << ref.id << " serial=" << owned.serial
                  << " subscribe_result=" << static_cast<unsigned>(result) << '\n';
    }
    void enumerate() {
        std::uint32_t count = 0;
        auto result = api.LeapGetDeviceList(connection, nullptr, &count);
        if ((result != eLeapRS_Success && result != eLeapRS_InsufficientBuffer) || count > 64) {
            std::cout << "enumeration-size error=" << static_cast<unsigned>(result) << " count=" << count << '\n';
            return;
        }
        if (!count) return;
        std::vector<LEAP_DEVICE_REF> refs(count);
        result = api.LeapGetDeviceList(connection, refs.data(), &count);
        if (result != eLeapRS_Success || count > refs.size()) {
            std::cout << "enumeration error=" << static_cast<unsigned>(result) << '\n'; return;
        }
        for (std::uint32_t i = 0; i < count; ++i) add(refs[i]);
    }
    void report() {
        for (const auto& [id, d] : devices) {
            std::cout << "device id=" << id << " serial=" << d.serial
                      << " subscribed=" << d.subscribed << " frames=" << d.frames
                      << " hands=" << d.hands << " age_ms=";
            if (d.frames) std::cout << std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now()-d.last).count();
            else std::cout << "unavailable";
            std::cout << '\n';
        }
    }
};
}
int main(int argc, char** argv) {
    try {
        if (argc < 2 || argc > 3 || (argc == 3 && std::string(argv[2]) != "--load-only")) {
            std::cerr << "usage: sdk-diagnostic /absolute/libLeapC.dylib [--load-only]\n"; return 2;
        }
        std::cout.setf(std::ios::unitbuf);
        Library library(argv[1]);
        Api api{library};
        LEAP_VERSION client{};
        const auto result = api.LeapGetVersion(nullptr, eLeapVersionPart_ClientLibrary, &client);
        version("client", result, client);
        if (result != eLeapRS_Success || !accepted(client)) return 4;
        if (argc == 3) return 0;
        std::signal(SIGINT, interrupt);
        std::signal(SIGTERM, interrupt);
        Session session{api};
        LEAP_CONNECTION_CONFIG config{};
        config.size = sizeof(config);
        config.flags = eLeapConnectionConfig_MultiDeviceAware;
        auto status = api.LeapCreateConnection(&config, &session.connection);
        if (status != eLeapRS_Success || !session.connection) {
            std::cout << "create error=" << static_cast<unsigned>(status) << '\n'; return 5;
        }
        status = api.LeapOpenConnection(session.connection);
        if (status != eLeapRS_Success) {
            std::cout << "open error=" << static_cast<unsigned>(status) << '\n'; return 5;
        }
        const auto deadline = Clock::now() + std::chrono::seconds(5);
        auto nextReport = Clock::now();
        bool connected = false, everConnected = false;
        unsigned errors = 0;
        while (!interrupted && Clock::now() < deadline) {
            LEAP_CONNECTION_MESSAGE msg{};
            msg.size = sizeof(msg);
            status = api.LeapPollConnection(session.connection, 10, &msg);
            if (status != eLeapRS_Success && status != eLeapRS_Timeout) {
                if (errors++ < 5) std::cout << "poll error=" << static_cast<unsigned>(status) << '\n';
                if (status != eLeapRS_NotConnected && status != eLeapRS_HandshakeIncomplete) return 5;
            } else if (status == eLeapRS_Success) {
                if (msg.type == eLeapEventType_Connection) {
                    LEAP_VERSION server{};
                    const auto vr = api.LeapGetVersion(session.connection, eLeapVersionPart_ServerLibrary, &server);
                    version("service", vr, server);
                    if (vr != eLeapRS_Success || !accepted(server)) return 4;
                    connected = everConnected = true;
                    session.enumerate();
                } else if (msg.type == eLeapEventType_ConnectionLost) {
                    connected = false;
                    session.clear();
                    std::cout << "service lost\n";
                } else if (connected && msg.type == eLeapEventType_Device && msg.device_event) {
                    session.add(msg.device_event->device);
                } else if (connected && msg.type == eLeapEventType_DeviceLost && msg.device_event) {
                    session.remove(msg.device_event->device.id);
                } else if (connected && msg.type == eLeapEventType_Tracking && msg.tracking_event) {
                    auto it = session.devices.find(msg.device_id);
                    if (it != session.devices.end() && it->second.subscribed) {
                        ++it->second.frames;
                        it->second.hands = msg.tracking_event->nHands;
                        it->second.last = Clock::now();
                    }
                }
            }
            if (Clock::now() >= nextReport) {
                session.report();
                nextReport = Clock::now() + std::chrono::seconds(1);
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }
        session.report();
        std::cout << "summary ever_connected=" << everConnected << " connected=" << connected
                  << " devices=" << session.devices.size() << " poll_errors=" << errors
                  << " interrupted=" << interrupted << '\n';
        if (!everConnected) std::cout << "service version=unavailable (no connection event within 5s)\n";
        return everConnected ? 0 : 3;
    } catch (const std::exception& e) {
        std::cerr << "diagnostic error: " << e.what() << '\n'; return 2;
    }
}
