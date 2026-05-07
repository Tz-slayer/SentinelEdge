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

if(NOT CMAKE_CONFIGURATION_TYPES AND NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif()

option(ENABLE_TESTS "Build unit and integration test targets" ON)
option(ENABLE_MOCK_SOURCES "Build mock video source strategies" ON)
option(ENABLE_DEV_WARNINGS "Enable extra compiler warnings" ON)
option(ENABLE_DEV_LOGGING "Enable development-oriented runtime logging" ON)
option(ENABLE_SANITIZERS "Enable address and undefined behavior sanitizers" OFF)

add_library(sentinel_project_options INTERFACE)

target_compile_features(sentinel_project_options INTERFACE cxx_std_17)
target_compile_definitions(sentinel_project_options
    INTERFACE
        SENTINEL_ENABLE_DEV_LOGGING=$<IF:$<BOOL:${ENABLE_DEV_LOGGING}>,1,0>
        SENTINEL_ENABLE_MOCK_SOURCES=$<IF:$<BOOL:${ENABLE_MOCK_SOURCES}>,1,0>
)

if(ENABLE_DEV_WARNINGS AND CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(sentinel_project_options
        INTERFACE
            -Wall
            -Wextra
            -Wpedantic
    )
endif()

if(ENABLE_SANITIZERS)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(sentinel_project_options
            INTERFACE
                -fsanitize=address,undefined
                -fno-omit-frame-pointer
        )
        target_link_options(sentinel_project_options
            INTERFACE
                -fsanitize=address,undefined
        )
    else()
        message(FATAL_ERROR "ENABLE_SANITIZERS requires a GNU or Clang compatible compiler")
    endif()
endif()

set(SENTINEL_CORE_SOURCES
    src/app/linux_signal_fd.cpp
    src/app/pipeline.cpp
    src/analytics/event_builder.cpp
    src/config/config_loader.cpp
    src/inference/mock_detector.cpp
    src/video/camera_video_source.cpp
    src/video/rtsp_video_source.cpp
    src/video/video_source_factory.cpp
)

if(ENABLE_MOCK_SOURCES)
    list(APPEND SENTINEL_CORE_SOURCES
        src/video/mock_video_source.cpp
    )
endif()

add_library(sentinel_core
    ${SENTINEL_CORE_SOURCES}
)

target_include_directories(sentinel_core
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(sentinel_core
    PUBLIC
        sentinel_project_options
)

add_executable(video_sentinel
    src/app/main.cpp
)

target_link_libraries(video_sentinel
    PRIVATE
        sentinel_core
)

if(ENABLE_TESTS)
    enable_testing()

    if(ENABLE_MOCK_SOURCES)
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
    endif()

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
endif()

message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "ENABLE_TESTS=${ENABLE_TESTS}")
message(STATUS "ENABLE_MOCK_SOURCES=${ENABLE_MOCK_SOURCES}")
message(STATUS "ENABLE_DEV_WARNINGS=${ENABLE_DEV_WARNINGS}")
message(STATUS "ENABLE_DEV_LOGGING=${ENABLE_DEV_LOGGING}")
message(STATUS "ENABLE_SANITIZERS=${ENABLE_SANITIZERS}")
