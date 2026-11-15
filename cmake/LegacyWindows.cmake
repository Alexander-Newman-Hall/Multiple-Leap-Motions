# The original CMake definition is preserved in legacy-original.cmake as audit
# evidence. This target uses the live Visual Studio application path only.
if(NOT WIN32 OR NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(FATAL_ERROR "The original LeapC application requires Windows x64; use an SDK-free core preset here.")
endif()
if(NOT EXISTS "${LEAP_SDK_ROOT}/include/LeapC.h" OR
   NOT EXISTS "${LEAP_SDK_ROOT}/lib/x64/LeapC.lib")
    message(FATAL_ERROR "Set LEAP_SDK_ROOT to your installed Ultraleap SDK containing include/LeapC.h and lib/x64/LeapC.lib. No SDK is downloaded or redistributed.")
endif()
find_package(OpenGL REQUIRED)
FetchContent_Declare(glm
    GIT_REPOSITORY https://github.com/g-truc/glm.git
    GIT_TAG bf71a834948186f4097caa076cd2663c69a10e1e)
set(SDL_SHARED OFF CACHE BOOL "" FORCE)
set(SDL_STATIC ON CACHE BOOL "" FORCE)
set(SDL_TEST OFF CACHE BOOL "" FORCE)
FetchContent_Declare(sdl2
    GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
    GIT_TAG ac13ca9ab691e13e8eebe9684740ddcb0d716203)
FetchContent_Declare(imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG c6e0284ac58b3f205c95365478888f7b53b077e2)
FetchContent_MakeAvailable(glm sdl2 imgui)
add_executable(leapMotionApp WIN32
    src/main.cpp
    src/resource.rc
    src/app/AppCore.cpp
    src/core/LeapInput.cpp
    src/pipeline/00_LeapConnection.cpp
    src/pipeline/01_LeapPoller.cpp
    src/transport/osc/OscController.cpp
    src/ui/MainAppWindow.cpp
    src/ui/UIController.cpp
    src/ui/OpenGLRenderer.cpp
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_sdl2.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp)
target_include_directories(leapMotionApp PRIVATE
    ${LEAP_SDK_ROOT}/include ${imgui_SOURCE_DIR} ${imgui_SOURCE_DIR}/backends)
target_compile_definitions(leapMotionApp PRIVATE _CRT_SECURE_NO_WARNINGS _WINSOCK_DEPRECATED_NO_WARNINGS SDL_MAIN_HANDLED)
target_link_libraries(leapMotionApp PRIVATE handbridge-core SDL2::SDL2-static glm OpenGL::GL
    "${LEAP_SDK_ROOT}/lib/x64/LeapC.lib" shell32 user32)
message(WARNING "leapMotionApp is the original coupled GUI path. This target is UNTESTED ON WINDOWS HARDWARE; it is not the new headless SDK adapter.")
