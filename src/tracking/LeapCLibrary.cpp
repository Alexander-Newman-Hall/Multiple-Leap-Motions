#include "LeapCLibrary.hpp"
#include <stdexcept>
#ifdef _WIN32
#include <windows.h>
#include <ShlObj.h>
#include <filesystem>
#include <cstring>
#include <vector>

namespace {
namespace fs = std::filesystem;
std::wstring environment(const wchar_t* name) {
    const auto size = GetEnvironmentVariableW(name, nullptr, 0);
    if (!size) return {};
    std::vector<wchar_t> buffer(size);
    const auto read = GetEnvironmentVariableW(name, buffer.data(), size);
    if (!read || read >= size) return {};
    return std::wstring(buffer.data(), read);
}
fs::path absoluteLibrary(fs::path path) {
    if (!path.is_absolute()) throw std::invalid_argument("LeapC library paths must be absolute; relative/PATH DLL search is disabled.");
    return path.lexically_normal();
}
fs::path libraryPath(const std::string& explicitPath) {
    if (!explicitPath.empty()) return absoluteLibrary(fs::u8path(explicitPath));
    const auto configured = environment(L"HANDBRIDGE_LEAPC_LIBRARY");
    if (!configured.empty()) return absoluteLibrary(fs::path(configured));
    const auto sdk = environment(L"LEAP_SDK_ROOT");
    if (!sdk.empty()) return absoluteLibrary(fs::path(sdk) / L"lib" / L"x64" / L"LeapC.dll");
    PWSTR directory = nullptr;
    if (FAILED(SHGetKnownFolderPath(FOLDERID_ProgramFiles, KF_FLAG_DEFAULT, nullptr, &directory)))
        throw std::runtime_error("Cannot locate Program Files. Set an absolute --leapc-library path.");
    const fs::path programFiles(directory);
    CoTaskMemFree(directory);
    const auto installed = programFiles / L"Ultraleap" / L"LeapSDK" / L"lib" / L"x64" / L"LeapC.dll";
    std::error_code error;
    if (fs::is_regular_file(installed, error)) return installed;
    throw std::runtime_error("No installed Ultraleap LeapC.dll found at Program Files/Ultraleap/LeapSDK/lib/x64. "
        "Install the official Ultraleap runtime/SDK or set --leapc-library / HANDBRIDGE_LEAPC_LIBRARY to its absolute DLL path.");
}
std::string utf8(const fs::path& path) {
    const auto value = path.u8string();
    return std::string(value.begin(), value.end());
}
std::string fileVersion(const fs::path& path) {
    DWORD ignored = 0;
    const auto size = GetFileVersionInfoSizeW(path.c_str(), &ignored);
    if (!size || size > 1024 * 1024) return "unknown";
    std::vector<std::byte> buffer(size);
    if (!GetFileVersionInfoW(path.c_str(), 0, size, buffer.data())) return "unknown";
    void* data = nullptr;
    UINT length = 0;
    if (!VerQueryValueW(buffer.data(), L"\\", &data, &length) || length < sizeof(VS_FIXEDFILEINFO)) return "unknown";
    const auto* info = static_cast<const VS_FIXEDFILEINFO*>(data);
    if (info->dwSignature != 0xfeef04bd) return "unknown";
    return std::to_string(HIWORD(info->dwFileVersionMS)) + "." + std::to_string(LOWORD(info->dwFileVersionMS)) +
        "." + std::to_string(HIWORD(info->dwFileVersionLS)) + "." + std::to_string(LOWORD(info->dwFileVersionLS));
}
template<class Function> Function symbol(HMODULE module, const char* name) {
    const auto address = GetProcAddress(module, name);
    if (!address) throw std::runtime_error(std::string("LeapC.dll is missing required SDK export ") + name +
        "; use a compatible multi-device Ultraleap runtime.");
    static_assert(sizeof(Function) == sizeof(address));
    Function function = nullptr;
    std::memcpy(&function, &address, sizeof(function));
    return function;
}
}
#endif

namespace handbridge {
LeapCLibrary::LeapCLibrary(const std::string& path) {
#ifdef _WIN32
    const auto selected = libraryPath(path);
    // No CWD or PATH search. Dependencies may resolve beside the selected SDK
    // DLL, in the application folder, or in normal trusted Windows locations.
    const auto module = LoadLibraryExW(selected.c_str(), nullptr,
        LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
    if (!module) {
        const auto error = GetLastError();
        throw std::runtime_error("Cannot load LeapC.dll at " + utf8(selected) +
            " (Windows error " + std::to_string(error) + "). Check x64 architecture and installed runtime dependencies.");
    }
    try {
        api_.createConnection = symbol<decltype(api_.createConnection)>(module, "LeapCreateConnection");
        api_.openConnection = symbol<decltype(api_.openConnection)>(module, "LeapOpenConnection");
        api_.pollConnection = symbol<decltype(api_.pollConnection)>(module, "LeapPollConnection");
        api_.getDeviceList = symbol<decltype(api_.getDeviceList)>(module, "LeapGetDeviceList");
        api_.openDevice = symbol<decltype(api_.openDevice)>(module, "LeapOpenDevice");
        api_.getDeviceInfo = symbol<decltype(api_.getDeviceInfo)>(module, "LeapGetDeviceInfo");
        api_.subscribeEvents = symbol<decltype(api_.subscribeEvents)>(module, "LeapSubscribeEvents");
        api_.unsubscribeEvents = symbol<decltype(api_.unsubscribeEvents)>(module, "LeapUnsubscribeEvents");
        api_.closeDevice = symbol<decltype(api_.closeDevice)>(module, "LeapCloseDevice");
        api_.closeConnection = symbol<decltype(api_.closeConnection)>(module, "LeapCloseConnection");
        api_.destroyConnection = symbol<decltype(api_.destroyConnection)>(module, "LeapDestroyConnection");
        api_.getVersion = symbol<decltype(api_.getVersion)>(module, "LeapGetVersion");
        LEAP_VERSION version{};
        const auto result = api_.getVersion(nullptr, eLeapVersionPart_ClientLibrary, &version);
        if (result != eLeapRS_Success || version.major != 6 || version.minor < 2 || version.patch < 0)
            throw std::runtime_error("Hyperion v6.2 or newer within v6 is required. The selected LeapC client did not report a supported v6 API version.");
        const auto apiVersion = std::to_string(version.major) + "." + std::to_string(version.minor) + "." + std::to_string(version.patch);
        info_ = {utf8(selected), fileVersion(selected), apiVersion};
        module_ = module;
    } catch (...) {
        FreeLibrary(module);
        throw;
    }
#else
    (void)path;
    throw std::runtime_error("The real LeapC runtime loader requires Windows x64. This platform supports the SDK-header adapter tests and --simulate.");
#endif
}
LeapCLibrary::~LeapCLibrary() {
#ifdef _WIN32
    if (module_) FreeLibrary(static_cast<HMODULE>(module_));
#endif
}
}
