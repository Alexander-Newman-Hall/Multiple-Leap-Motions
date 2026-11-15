# Release deployment is opt-in and never copies the proprietary LeapC SDK.
option(HANDBRIDGE_PACKAGE_WINDOWS "Enable the Windows Release portable package target (requires PowerShell 7)" OFF)
if(NOT WIN32 OR NOT HANDBRIDGE_BUILD_CONTROL OR NOT HANDBRIDGE_PACKAGE_WINDOWS)
    return()
endif()
find_program(HANDBRIDGE_PWSH NAMES pwsh REQUIRED)
get_target_property(handbridge_qt_qmake Qt6::qmake IMPORTED_LOCATION)
get_filename_component(handbridge_qt_bin "${handbridge_qt_qmake}" DIRECTORY)
get_filename_component(handbridge_qt_root "${handbridge_qt_bin}" DIRECTORY)
find_package(Git QUIET)
set(handbridge_package_revision "unknown")
if(Git_FOUND)
    execute_process(COMMAND "${GIT_EXECUTABLE}" rev-parse HEAD
        WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
        OUTPUT_VARIABLE handbridge_package_revision OUTPUT_STRIP_TRAILING_WHITESPACE
        COMMAND_ERROR_IS_FATAL ANY)
endif()
add_custom_target(handbridge-portable
    COMMAND "${HANDBRIDGE_PWSH}" -NoProfile -File
        "${PROJECT_SOURCE_DIR}/scripts/Package-Windows.ps1"
        -Repository "${PROJECT_SOURCE_DIR}"
        -BinaryDirectory "$<TARGET_FILE_DIR:handbridge-control>"
        -OutputDirectory "${PROJECT_BINARY_DIR}/portable"
        -QtRoot "${handbridge_qt_root}" -Configuration "$<CONFIG>"
        -Revision "${handbridge_package_revision}" -QtVersion "${Qt6_VERSION}"
        -LeapCCompiled "$<BOOL:${HANDBRIDGE_WITH_LEAPC}>"
        -LeapCHeaderSha256 "${HANDBRIDGE_LEAPC_HEADER_SHA256}"
    DEPENDS handbridge-engine handbridge-control
    COMMENT "Deploy native Release executables, Qt, release CRT, notices and corresponding source"
    VERBATIM USES_TERMINAL)
