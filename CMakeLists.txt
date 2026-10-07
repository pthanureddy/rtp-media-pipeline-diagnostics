cmake_minimum_required(VERSION 3.20)
project(rtp_media_pipeline_diagnostics VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(MEDIA_BUILD_GSTREAMER_PROBE "Build the GStreamer pipeline probe when GStreamer is available" ON)
option(MEDIA_ENABLE_SANITIZERS "Enable AddressSanitizer and UndefinedBehaviorSanitizer" OFF)

add_library(media_diagnostics
    src/rtp.cpp
    src/h264.cpp
    src/rtsp.cpp
    src/stun.cpp
    src/udp_socket.cpp
)
target_include_directories(media_diagnostics PUBLIC include)

if(MSVC)
    target_compile_options(media_diagnostics PRIVATE /W4 /permissive-)
    target_link_libraries(media_diagnostics PUBLIC ws2_32)
else()
    target_compile_options(media_diagnostics PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wshadow)
endif()

if(MEDIA_ENABLE_SANITIZERS AND CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(media_diagnostics PUBLIC -fsanitize=address,undefined -fno-omit-frame-pointer)
    target_link_options(media_diagnostics PUBLIC -fsanitize=address,undefined)
endif()

find_package(Threads REQUIRED)

add_executable(rtp-diagnostics apps/rtp_diagnostics.cpp)
target_link_libraries(rtp-diagnostics PRIVATE media_diagnostics Threads::Threads)

if(MEDIA_BUILD_GSTREAMER_PROBE)
    find_package(PkgConfig QUIET)
    if(PkgConfig_FOUND)
        pkg_check_modules(GSTREAMER QUIET IMPORTED_TARGET gstreamer-1.0)
    endif()
    if(GSTREAMER_FOUND)
        add_executable(gstreamer-pipeline-probe apps/gstreamer_pipeline_probe.cpp)
        target_link_libraries(gstreamer-pipeline-probe PRIVATE PkgConfig::GSTREAMER)
    else()
        message(STATUS "GStreamer development files not found; probe target disabled")
    endif()
endif()

enable_testing()
add_executable(media-diagnostics-tests tests/test_main.cpp)
target_link_libraries(media-diagnostics-tests PRIVATE media_diagnostics Threads::Threads)
add_test(NAME media-diagnostics-tests COMMAND media-diagnostics-tests)

install(TARGETS rtp-diagnostics media_diagnostics)
