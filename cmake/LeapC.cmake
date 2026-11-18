# Development headers stay outside git. The adapter uses the exact declarations
# in this header, then loads the user's installed runtime explicitly at runtime.
if(NOT LEAP_SDK_ROOT)
    message(FATAL_ERROR "HANDBRIDGE_WITH_LEAPC requires LEAP_SDK_ROOT. Supply your installed SDK, or run scripts/fetch-leapc-sdk.py for the documented development baseline. No SDK is fetched by CMake.")
endif()
# Never keep an include directory from a different SDK root across reconfigure.
unset(LEAPC_INCLUDE_DIR CACHE)
unset(LEAPC_INCLUDE_DIR)
find_path(LEAPC_INCLUDE_DIR NAMES LeapC.h
    PATHS "${LEAP_SDK_ROOT}/include" "${LEAP_SDK_ROOT}/Include"
    NO_DEFAULT_PATH NO_CACHE REQUIRED)
file(SHA256 "${LEAPC_INCLUDE_DIR}/LeapC.h" HANDBRIDGE_LEAPC_HEADER_SHA256)
message(STATUS "LeapC header: ${LEAPC_INCLUDE_DIR}/LeapC.h (SHA256 ${HANDBRIDGE_LEAPC_HEADER_SHA256})")
include(CheckCXXSourceCompiles)
set(CMAKE_REQUIRED_INCLUDES "${LEAPC_INCLUDE_DIR}")
unset(HANDBRIDGE_LEAPC_HAS_VERSION_API CACHE)
check_cxx_source_compiles("#include <LeapC.h>
    static_assert(sizeof(decltype(&LeapGetVersion)) > 0);
    int main() { LEAP_VERSION v{}; return v.major; }" HANDBRIDGE_LEAPC_HAS_VERSION_API)
unset(CMAKE_REQUIRED_INCLUDES)
if(NOT HANDBRIDGE_LEAPC_HAS_VERSION_API)
    message(FATAL_ERROR "A Hyperion v6.2 SDK header with LeapGetVersion is required. Replace the older SDK at LEAP_SDK_ROOT; see docs/LEAPC_SETUP.md.")
endif()
if(APPLE AND CMAKE_OSX_ARCHITECTURES STREQUAL "arm64")
    message(STATUS "Building the native Apple Silicon Hyperion runtime loader.")
elseif(NOT WIN32)
    message(STATUS "LeapC adapter compiles for SDK-independent API tests here.")
elseif(NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(FATAL_ERROR "The LeapC hardware provider requires Windows x64.")
endif()
target_sources(handbridge-tracking PRIVATE
    src/tracking/LeapCTrackingSource.cpp src/tracking/LeapCLibrary.cpp)
target_include_directories(handbridge-tracking SYSTEM PRIVATE "${LEAPC_INCLUDE_DIR}")
target_compile_definitions(handbridge-tracking PRIVATE HANDBRIDGE_WITH_LEAPC=1)
if(WIN32)
    target_link_libraries(handbridge-tracking PRIVATE version)
endif()
