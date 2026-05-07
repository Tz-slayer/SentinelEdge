cmake_minimum_required(VERSION 3.16)

project(edge_ai_video_sentinel
    VERSION 0.1.0
    DESCRIPTION "Edge AI video sentinel MVP"
    LANGUAGES CXX
)

if(NOT CMAKE_SYSTEM_NAME STREQUAL "Linux")
    message(FATAL_ERROR "edge_ai_video_sentinel currently targets Linux only")
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(sentinel_core
    src/app/linux_signal_fd.cpp
    src/app/pipeline.cpp
    src/analytics/event_builder.cpp
    src/config/config_loader.cpp
    src/inference/mock_detector.cpp
    src/video/camera_video_source.cpp
    src/video/mock_video_source.cpp
    src/video/rtsp_video_source.cpp
    src/video/video_source_factory.cpp
)

target_include_directories(sentinel_core
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(sentinel_core PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_executable(video_sentinel
    src/app/main.cpp
)

target_link_libraries(video_sentinel
    PRIVATE
        sentinel_core
)

enable_testing()

add_executable(sentinel_tests
    tests/unit/pipeline_smoke_test.cpp
)

target_link_libraries(sentinel_tests
    PRIVATE
        sentinel_core
)

add_test(
    NAME sentinel.pipeline_smoke
    COMMAND sentinel_tests ${CMAKE_CURRENT_SOURCE_DIR}/config
)

add_executable(sentinel_signal_tests
    tests/unit/linux_signal_fd_test.cpp
)

target_link_libraries(sentinel_signal_tests
    PRIVATE
        sentinel_core
)

add_test(
    NAME sentinel.signal_fd
    COMMAND sentinel_signal_tests
)

add_executable(sentinel_camera_tests
    tests/unit/camera_video_source_test.cpp
)

target_link_libraries(sentinel_camera_tests
    PRIVATE
        sentinel_core
)

add_test(
    NAME sentinel.camera_source
    COMMAND sentinel_camera_tests
)
