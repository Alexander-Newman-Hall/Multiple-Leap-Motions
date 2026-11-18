# Optional native controller. The engine only links Qt Core/Network; rendering
# libraries belong exclusively to the separate control executable.
find_package(Qt6 6.8 REQUIRED COMPONENTS Core Network Quick Qml QuickControls2)

add_library(handbridge-control-client STATIC
    src/control/Controller.cpp
    src/control/Controller.hpp)
set_target_properties(handbridge-control-client PROPERTIES AUTOMOC ON)
target_include_directories(handbridge-control-client PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(handbridge-control-client PUBLIC Qt6::Core Qt6::Network)

qt_add_executable(handbridge-control WIN32 src/control/control_main.cpp)
target_link_libraries(handbridge-control PRIVATE
    handbridge-control-client Qt6::Quick Qt6::Qml Qt6::QuickControls2)
if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    enable_language(OBJCXX)
    find_library(HANDBRIDGE_APPKIT AppKit REQUIRED)
    add_library(handbridge-mac-menu STATIC src/control/MacMenuBar.mm)
    set_target_properties(handbridge-mac-menu PROPERTIES OBJCXX_STANDARD 20 OBJCXX_STANDARD_REQUIRED ON)
    target_compile_options(handbridge-mac-menu PRIVATE -fobjc-arc)
    target_include_directories(handbridge-mac-menu PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(handbridge-mac-menu PUBLIC Qt6::Gui ${HANDBRIDGE_APPKIT})
    target_link_libraries(handbridge-control PRIVATE handbridge-mac-menu)
endif()
# Local components retain their relative paths and are loaded from qrc; no web UI.
file(GLOB_RECURSE HANDBRIDGE_CONTROL_QML CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/control/qml/*.qml"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/control/qml/*.js"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/control/qml/*.svg"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/control/qml/qmldir")
qt_add_resources(handbridge-control "handbridge_ui"
    PREFIX "/handbridge"
    BASE "${CMAKE_CURRENT_SOURCE_DIR}/src/control/qml"
    FILES ${HANDBRIDGE_CONTROL_QML})

if(BUILD_TESTING)
    find_package(Qt6 6.8 REQUIRED COMPONENTS Test)
    if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
        add_test(NAME MacControllerStartup COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_MacControllerStartup.py $<TARGET_FILE:handbridge-control>)
        set_tests_properties(MacControllerStartup PROPERTIES TIMEOUT 30)
        add_executable(test_MacMenuBar tests/test_MacMenuBar.mm)
        set_target_properties(test_MacMenuBar PROPERTIES AUTOMOC ON OBJCXX_STANDARD 20 OBJCXX_STANDARD_REQUIRED ON)
        target_link_libraries(test_MacMenuBar PRIVATE handbridge-mac-menu Qt6::Test)
        add_test(NAME MacMenuBar COMMAND test_MacMenuBar)
        set_tests_properties(MacMenuBar PROPERTIES TIMEOUT 30 SKIP_RETURN_CODE 77)
    endif()
    add_executable(test_Controller tests/test_Controller.cpp)
    set_target_properties(test_Controller PROPERTIES AUTOMOC ON)
    target_link_libraries(test_Controller PRIVATE handbridge-control-client Qt6::Test)
    add_dependencies(test_Controller handbridge-engine)
    add_test(NAME ControllerModel COMMAND test_Controller
        -o "${CMAKE_CURRENT_BINARY_DIR}/qt-controller.txt,txt")
    set_tests_properties(ControllerModel PROPERTIES TIMEOUT 30)

    add_executable(test_ControlUi tests/test_ControlUi.cpp)
    set_target_properties(test_ControlUi PROPERTIES AUTOMOC ON)
    target_link_libraries(test_ControlUi PRIVATE
        handbridge-control-client Qt6::Quick Qt6::Qml Qt6::QuickControls2 Qt6::Test)
    target_compile_definitions(test_ControlUi PRIVATE
        TEST_QML_PATH="${CMAKE_CURRENT_SOURCE_DIR}/src/control/qml/Main.qml"
        TEST_ENGINE_PATH="$<TARGET_FILE:handbridge-engine>")
    add_dependencies(test_ControlUi handbridge-engine)
    add_test(NAME ControlUi COMMAND test_ControlUi
        -o "${CMAKE_CURRENT_BINARY_DIR}/qt-control-ui.txt,txt")
    set_tests_properties(ControlUi PROPERTIES TIMEOUT 60
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software")

    add_executable(test_ControlIntegration tests/test_ControlIntegration.cpp)
    set_target_properties(test_ControlIntegration PROPERTIES AUTOMOC ON)
    target_include_directories(test_ControlIntegration PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(test_ControlIntegration PRIVATE Qt6::Core Qt6::Network Qt6::Test)
    target_compile_definitions(test_ControlIntegration PRIVATE
        TEST_ENGINE_PATH="$<TARGET_FILE:handbridge-engine>"
        TEST_CONTROL_PATH="$<TARGET_FILE:handbridge-control>")
    add_dependencies(test_ControlIntegration handbridge-engine handbridge-control)
    add_test(NAME ControlIntegration COMMAND test_ControlIntegration
        -o "${CMAKE_CURRENT_BINARY_DIR}/qt-control-integration.txt,txt")
    set_tests_properties(ControlIntegration PROPERTIES
        TIMEOUT 90
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software")
endif()

foreach(handbridge_native_target handbridge-control-client handbridge-control
        test_Controller test_ControlUi test_ControlIntegration)
    if(TARGET ${handbridge_native_target})
        if(MSVC)
            target_compile_options(${handbridge_native_target} PRIVATE /W4 /permissive- /utf-8)
            target_compile_definitions(${handbridge_native_target} PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
        else()
            target_compile_options(${handbridge_native_target} PRIVATE -Wall -Wextra -Wpedantic)
        endif()
        if(HANDBRIDGE_ENABLE_SANITIZERS AND NOT MSVC)
            target_compile_options(${handbridge_native_target} PRIVATE
                -fsanitize=address,undefined -fno-omit-frame-pointer)
            target_link_options(${handbridge_native_target} PRIVATE -fsanitize=address,undefined)
        endif()
    endif()
endforeach()
