cmake_minimum_required(VERSION 3.24)
project(HandBridge VERSION 2.0.3 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")

option(HANDBRIDGE_BUILD_CONTROL "Build the optional native Qt controller and local IPC engine host" OFF)
option(HANDBRIDGE_BUILD_LEGACY_WINDOWS "Build original Windows GUI/LeapC application (hardware unverified)" OFF)
option(HANDBRIDGE_WITH_LEAPC "Compile the real LeapC adapter against an explicitly supplied SDK header" OFF)
option(HANDBRIDGE_ENABLE_SANITIZERS "Enable AddressSanitizer and UndefinedBehaviorSanitizer on GCC/Clang" OFF)
set(LEAP_SDK_ROOT "$ENV{LEAP_SDK_ROOT}" CACHE PATH "Installed Ultraleap SDK root containing include/LeapC.h and lib/x64/LeapC.lib")

include(CTest)
include(FetchContent)
find_package(Threads REQUIRED)

# Exact source revisions + archive digests; no proprietary SDK is downloaded.
FetchContent_Declare(nlohmann_json
    URL https://codeload.github.com/nlohmann/json/tar.gz/9cca280a4d0ccf0c08f47a99aa71d1b0e52f8d03
    URL_HASH SHA256=0dbc5e40a01ff142e7e68c03e85247a4dcede2f592d12d3677dee3664d17975a
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
set(JSON_BuildTests OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(nlohmann_json)

FetchContent_Declare(oscpack_source
    URL https://codeload.github.com/RossBencina/oscpack/tar.gz/3e7e3a102e316d75fe176885a6dd08f216f2eac0
    URL_HASH SHA256=df5fe1a7a50fe3114471f9a7fc3f20d81f8ee2d8691ad695e0584f1813948012
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    SOURCE_SUBDIR handbridge-no-upstream-cmake)
FetchContent_MakeAvailable(oscpack_source)
if(WIN32)
    set(OSCPACK_PLATFORM win32)
else()
    set(OSCPACK_PLATFORM posix)
endif()
add_library(oscpack_lib STATIC
    ${oscpack_source_SOURCE_DIR}/ip/IpEndpointName.cpp
    ${oscpack_source_SOURCE_DIR}/ip/${OSCPACK_PLATFORM}/NetworkingUtils.cpp
    ${oscpack_source_SOURCE_DIR}/ip/${OSCPACK_PLATFORM}/UdpSocket.cpp
    ${oscpack_source_SOURCE_DIR}/osc/OscOutboundPacketStream.cpp
    ${oscpack_source_SOURCE_DIR}/osc/OscPrintReceivedElements.cpp
    ${oscpack_source_SOURCE_DIR}/osc/OscReceivedElements.cpp
    ${oscpack_source_SOURCE_DIR}/osc/OscTypes.cpp)
target_include_directories(oscpack_lib SYSTEM PUBLIC ${oscpack_source_SOURCE_DIR})
if(WIN32)
    target_link_libraries(oscpack_lib PUBLIC ws2_32 winmm)
endif()

add_library(handbridge-core STATIC
    src/core/ConfigManager.cpp
    src/core/DeviceAliasManager.cpp
    src/pipeline/02_LeapSorter.cpp
    src/pipeline/03_DataProcessor.cpp
    src/pipeline/04_OscSender.cpp)
target_include_directories(handbridge-core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/transport
    ${nlohmann_json_SOURCE_DIR}/single_include/nlohmann)
target_link_libraries(handbridge-core PUBLIC nlohmann_json::nlohmann_json oscpack_lib Threads::Threads)
if(WIN32)
    target_compile_definitions(handbridge-core PUBLIC NOMINMAX WIN32_LEAN_AND_MEAN)
    target_link_libraries(handbridge-core PUBLIC shell32 ole32 uuid)
endif()

add_library(handbridge-runtime STATIC
    src/runtime/MockTrackingSource.cpp
    src/runtime/ControlSettings.cpp
    src/runtime/Processing.cpp
    src/runtime/EngineRuntime.cpp)
target_link_libraries(handbridge-runtime PUBLIC handbridge-core)
add_library(handbridge-tracking STATIC src/tracking/TrackingSourceFactory.cpp)
target_link_libraries(handbridge-tracking PUBLIC handbridge-runtime)
if(HANDBRIDGE_WITH_LEAPC)
    include(cmake/LeapC.cmake)
endif()
if(HANDBRIDGE_BUILD_CONTROL)
    find_package(Qt6 6.8 REQUIRED COMPONENTS Core Network)
    add_executable(handbridge-engine src/engine/qt_engine_main.cpp)
    target_link_libraries(handbridge-engine PRIVATE handbridge-tracking Qt6::Core Qt6::Network)
else()
    add_executable(handbridge-engine src/runtime/engine_main.cpp)
    target_link_libraries(handbridge-engine PRIVATE handbridge-tracking)
endif()

foreach(handbridge_target handbridge-core handbridge-runtime handbridge-tracking handbridge-engine)
    if(MSVC)
        target_compile_options(${handbridge_target} PRIVATE /W4 /permissive- /utf-8)
    else()
        target_compile_options(${handbridge_target} PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endforeach()
if(HANDBRIDGE_ENABLE_SANITIZERS)
    if(MSVC)
        message(FATAL_ERROR "HANDBRIDGE_ENABLE_SANITIZERS currently requires GCC or Clang")
    endif()
    target_compile_options(handbridge-core PUBLIC -fsanitize=address,undefined -fno-omit-frame-pointer)
    target_link_options(handbridge-core PUBLIC -fsanitize=address,undefined)
endif()

if(BUILD_TESTING)
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(googletest
        URL https://codeload.github.com/google/googletest/tar.gz/b514bdc898e2951020cbdca1304b75f5950d1f59
        URL_HASH SHA256=9257316d65d6259f6596bc4fa5464092305ab3f3d9dc6d7e5780ab462d8baa64
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
    FetchContent_MakeAvailable(googletest)
    include(GoogleTest)
    # Explicit list: stale legacy tests are inventoried in BUILD_AUDIT.md.
    foreach(handbridge_test test_ConfigManager test_DeviceAliasManager test_LeapSorter test_LegacyOscContract test_EngineRuntime test_ControlSettings test_Processing)
        add_executable(${handbridge_test} tests/${handbridge_test}.cpp)
        target_link_libraries(${handbridge_test} PRIVATE handbridge-runtime GTest::gtest_main)
        gtest_discover_tests(${handbridge_test} DISCOVERY_TIMEOUT 30)
    endforeach()
    add_executable(test_WorkerConcurrency tests/test_WorkerConcurrency.cpp)
    target_include_directories(test_WorkerConcurrency PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(test_WorkerConcurrency PRIVATE GTest::gtest_main Threads::Threads)
    gtest_discover_tests(test_WorkerConcurrency DISCOVERY_TIMEOUT 30 PROPERTIES TIMEOUT 15)
    find_package(Python3 3.9 REQUIRED COMPONENTS Interpreter)
    add_test(NAME FetchLeapcSdk COMMAND ${Python3_EXECUTABLE}
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_FetchLeapcSdk.py)
    add_test(NAME HeadlessUdp COMMAND ${Python3_EXECUTABLE}
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_HeadlessUdp.py $<TARGET_FILE:handbridge-engine>)
    set_tests_properties(HeadlessUdp PROPERTIES TIMEOUT 30)
    if(HANDBRIDGE_BUILD_CONTROL)
        add_test(NAME WorkerLifecycle COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_WorkerLifecycle.py $<TARGET_FILE:handbridge-engine>)
        set_tests_properties(WorkerLifecycle PROPERTIES TIMEOUT 30)
    endif()
    if(HANDBRIDGE_WITH_LEAPC)
        add_executable(test_LeapCTrackingSource tests/test_LeapCTrackingSource.cpp)
        target_include_directories(test_LeapCTrackingSource SYSTEM PRIVATE "${LEAPC_INCLUDE_DIR}")
        target_link_libraries(test_LeapCTrackingSource PRIVATE handbridge-tracking GTest::gtest_main)
        gtest_discover_tests(test_LeapCTrackingSource DISCOVERY_TIMEOUT 30)
    endif()
endif()

if(HANDBRIDGE_BUILD_LEGACY_WINDOWS)
    include(cmake/LegacyWindows.cmake)
endif()

if(HANDBRIDGE_BUILD_CONTROL)
    include(cmake/NativeControl.cmake)
endif()

include(cmake/WindowsDeploy.cmake OPTIONAL)
