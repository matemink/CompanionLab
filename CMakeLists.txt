cmake_minimum_required(VERSION 3.22)

project(
    CompanionLab
    VERSION 0.1.0
    DESCRIPTION "UAV companion-computer testbench for ArduPilot"
    LANGUAGES C CXX
)

find_package(Threads REQUIRED)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(FetchContent)

# Generated MAVLink headers are used instead of implementing the protocol.
FetchContent_Declare(
    mavlink_headers
    GIT_REPOSITORY https://github.com/mavlink/c_library_v2.git
    GIT_TAG ee5827fcb834bf9e149dc2317fd444e4d1ef9d2e
    GIT_SHALLOW FALSE
)
FetchContent_MakeAvailable(mavlink_headers)

FetchContent_Declare(
    apriltag_source
    GIT_REPOSITORY https://github.com/AprilRobotics/apriltag.git
    GIT_TAG 12cf56b93d8f1f95e528246c8000c36881fb43fb
    GIT_SHALLOW FALSE
)

FetchContent_Declare(
    cpp_httplib_source
    GIT_REPOSITORY https://github.com/yhirose/cpp-httplib.git
    GIT_TAG 34b7c846d43e30598f3b748ef5ce23591077cb7e
    GIT_SHALLOW FALSE
)
FetchContent_GetProperties(cpp_httplib_source)
if(NOT cpp_httplib_source_POPULATED)
    FetchContent_Populate(cpp_httplib_source)
endif()
FetchContent_GetProperties(apriltag_source)
if(NOT apriltag_source_POPULATED)
    FetchContent_Populate(apriltag_source)
    set(BUILD_SHARED_LIBS OFF)
    set(BUILD_EXAMPLES OFF)
    set(BUILD_PYTHON_WRAPPER OFF)
    set(companionlab_build_testing_before_apriltag ${BUILD_TESTING})
    set(BUILD_TESTING OFF)
    add_subdirectory(
        ${apriltag_source_SOURCE_DIR}
        ${apriltag_source_BINARY_DIR}
        EXCLUDE_FROM_ALL
    )
    set(BUILD_TESTING ${companionlab_build_testing_before_apriltag})
endif()

add_library(companionlab_public_headers INTERFACE)
target_include_directories(
    companionlab_public_headers
    INTERFACE
        ${PROJECT_SOURCE_DIR}/include
)

add_library(companionlab_mavlink_headers INTERFACE)
target_include_directories(
    companionlab_mavlink_headers
    SYSTEM INTERFACE
        ${mavlink_headers_SOURCE_DIR}
)

function(companionlab_enable_warnings target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /permissive-)
    else()
        target_compile_options(
            ${target}
            PRIVATE
                -Wall
                -Wextra
                -Wpedantic
                -Wconversion
                -Wsign-conversion
        )
    endif()
endfunction()

add_library(
    companionlab_domain
    src/domain/VehicleState.cpp
)
target_link_libraries(
    companionlab_domain
    PUBLIC
        companionlab_public_headers
)
companionlab_enable_warnings(companionlab_domain)

add_library(companionlab_transport_port INTERFACE)
target_link_libraries(
    companionlab_transport_port
    INTERFACE
        companionlab_public_headers
)

add_library(companionlab_camera_port INTERFACE)
target_link_libraries(
    companionlab_camera_port
    INTERFACE
        companionlab_public_headers
)

add_library(companionlab_target_detector_port INTERFACE)
target_link_libraries(
    companionlab_target_detector_port
    INTERFACE
        companionlab_public_headers
)

add_library(companionlab_camera_preview_port INTERFACE)
target_link_libraries(
    companionlab_camera_preview_port
    INTERFACE
        companionlab_camera_port
        companionlab_public_headers
)

add_library(
    companionlab_camera_adapter
    src/adapters/camera/RpicamCameraSource.cpp
)
target_link_libraries(
    companionlab_camera_adapter
    PUBLIC
        companionlab_camera_port
    PRIVATE
        Threads::Threads
)
companionlab_enable_warnings(companionlab_camera_adapter)

add_library(
    companionlab_apriltag_adapter
    src/adapters/vision/AprilTagTargetDetector.cpp
)
target_link_libraries(
    companionlab_apriltag_adapter
    PUBLIC
        companionlab_target_detector_port
    PRIVATE
        apriltag
)
target_include_directories(
    companionlab_apriltag_adapter
    SYSTEM PRIVATE
        ${apriltag_source_SOURCE_DIR}
)
companionlab_enable_warnings(companionlab_apriltag_adapter)

add_library(
    companionlab_camera_preview_adapter
    src/adapters/preview/HttpCameraPreviewServer.cpp
)
target_link_libraries(
    companionlab_camera_preview_adapter
    PUBLIC
        companionlab_camera_preview_port
    PRIVATE
        Threads::Threads
)
target_include_directories(
    companionlab_camera_preview_adapter
    SYSTEM PRIVATE
        ${cpp_httplib_source_SOURCE_DIR}
)
companionlab_enable_warnings(companionlab_camera_preview_adapter)

add_library(
    companionlab_transport_adapter
    src/adapters/transport/PosixSerialTransport.cpp
    src/adapters/transport/UdpTransport.cpp
)
target_link_libraries(
    companionlab_transport_adapter
    PUBLIC
        companionlab_transport_port
)
if(WIN32)
    target_link_libraries(companionlab_transport_adapter PRIVATE ws2_32)
endif()
companionlab_enable_warnings(companionlab_transport_adapter)

add_library(
    companionlab_mavlink_adapter
    src/adapters/mavlink/MavlinkDecoder.cpp
    src/adapters/mavlink/MavlinkEncoder.cpp
    src/adapters/mavlink/TelemetryStreamConfigurator.cpp
)
target_link_libraries(
    companionlab_mavlink_adapter
    PUBLIC
        companionlab_domain
        companionlab_mavlink_headers
        companionlab_public_headers
)
companionlab_enable_warnings(companionlab_mavlink_adapter)

add_library(
    companionlab_ardupilot_catalog_adapter
    src/adapters/ardupilot/BoardTypeCatalog.cpp
)
target_link_libraries(
    companionlab_ardupilot_catalog_adapter
    PUBLIC
        companionlab_public_headers
)
companionlab_enable_warnings(companionlab_ardupilot_catalog_adapter)

add_library(
    companionlab_application
    src/application/AppSnapshot.cpp
    src/application/CameraMonitor.cpp
    src/application/VisionMonitor.cpp
    src/application/Scenario.cpp
    src/application/ScenarioRunner.cpp
    src/application/CompanionApplication.cpp
)
target_link_libraries(
    companionlab_application
    PUBLIC
        companionlab_domain
        companionlab_camera_port
        companionlab_camera_preview_port
        companionlab_target_detector_port
        companionlab_public_headers
        companionlab_transport_port
    PRIVATE
        companionlab_mavlink_adapter
)
companionlab_enable_warnings(companionlab_application)

add_library(
    companionlab_console_presentation
    src/presentation/console/ConsoleInput.cpp
    src/presentation/console/ConsoleView.cpp
)
target_link_libraries(
    companionlab_console_presentation
    PUBLIC
        companionlab_application
        companionlab_public_headers
)
companionlab_enable_warnings(companionlab_console_presentation)

add_executable(companionlab src/main.cpp)
target_link_libraries(
    companionlab
    PRIVATE
        companionlab_application
        companionlab_ardupilot_catalog_adapter
        companionlab_camera_adapter
        companionlab_camera_preview_adapter
        companionlab_apriltag_adapter
        companionlab_console_presentation
        companionlab_transport_adapter
)
companionlab_enable_warnings(companionlab)

include(GNUInstallDirs)
install(
    TARGETS companionlab
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
install(
    FILES third_party/ardupilot/board_types.txt
    DESTINATION ${CMAKE_INSTALL_DATADIR}/companionlab
    RENAME ardupilot-board-types.txt
)
install(
    FILES third_party/ardupilot/COPYING.txt
    DESTINATION ${CMAKE_INSTALL_DATADIR}/licenses/CompanionLab
    RENAME ArduPilot-COPYING.txt
)
install(
    FILES ${apriltag_source_SOURCE_DIR}/LICENSE.md
    DESTINATION ${CMAKE_INSTALL_DATADIR}/licenses/CompanionLab
    RENAME AprilTag-LICENSE.md
)
install(
    FILES ${cpp_httplib_source_SOURCE_DIR}/LICENSE
    DESTINATION ${CMAKE_INSTALL_DATADIR}/licenses/CompanionLab
    RENAME cpp-httplib-LICENSE
)
install(
    FILES assets/camera-preview/index.html
    DESTINATION ${CMAKE_INSTALL_DATADIR}/companionlab
    RENAME camera-preview.html
)

include(CTest)

if(BUILD_TESTING)
    add_executable(
        companionlab_tests
        tests/CompanionApplicationTests.cpp
        tests/BoardTypeCatalogTests.cpp
        tests/CameraMonitorTests.cpp
        tests/VisionMonitorTests.cpp
        tests/ScenarioRunnerTests.cpp
        tests/ConsoleViewTests.cpp
        tests/MavlinkDecoderTests.cpp
        tests/MavlinkEncoderTests.cpp
        tests/TelemetryStreamConfiguratorTests.cpp
        tests/VehicleStateTests.cpp
        tests/TestMain.cpp
    )
    target_link_libraries(
        companionlab_tests
        PRIVATE
            companionlab_application
            companionlab_ardupilot_catalog_adapter
            companionlab_camera_adapter
            companionlab_camera_port
            companionlab_camera_preview_port
            companionlab_apriltag_adapter
            companionlab_target_detector_port
            companionlab_domain
            companionlab_mavlink_adapter
            companionlab_console_presentation
            companionlab_transport_port
            apriltag
    )
    target_compile_definitions(
        companionlab_tests
        PRIVATE
            COMPANIONLAB_SOURCE_DIR="${PROJECT_SOURCE_DIR}"
    )
    target_include_directories(
        companionlab_tests
        SYSTEM PRIVATE
            ${apriltag_source_SOURCE_DIR}
    )
    companionlab_enable_warnings(companionlab_tests)
    add_test(NAME companionlab_tests COMMAND companionlab_tests)
endif()
