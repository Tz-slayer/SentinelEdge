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
option(ENABLE_ASCENDCL "Build AscendCL inference backend" OFF)
option(ENABLE_OPENCV_PREPROCESSOR "Build OpenCV frame preprocessing backend" ON)
option(ENABLE_OPENCV_POSTPROCESSOR "Build OpenCV YOLO postprocessing backend" ON)
option(INSTALL_DEBUG_SOURCE "Install a compact source snapshot for board-side GDB debugging" OFF)

set(SENTINEL_DEFAULT_CONFIG_DIR "config/prod" CACHE STRING "Default runtime config directory")
set(ASCENDCL_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/third_party/ascend-cann/8.0.0" CACHE PATH "AscendCL CANN root directory")
set(ASCENDCL_LIB_DIR "" CACHE PATH "Optional AscendCL library directory")

if(ENABLE_ASCENDCL)
    if(NOT EXISTS "${ASCENDCL_ROOT}/include/acl/acl.h")
        message(FATAL_ERROR
            "ENABLE_ASCENDCL=ON requires ${ASCENDCL_ROOT}/include/acl/acl.h. "
            "Set ASCENDCL_ROOT to the toolkit architecture root, for example "
            "/usr/local/Ascend/ascend-toolkit/8.0.0/aarch64-linux"
        )
    endif()

    set(ASCENDCL_LIBRARY_SEARCH_PATHS)
    if(ASCENDCL_LIB_DIR)
        list(APPEND ASCENDCL_LIBRARY_SEARCH_PATHS "${ASCENDCL_LIB_DIR}")
    endif()
    list(APPEND ASCENDCL_LIBRARY_SEARCH_PATHS
        "${ASCENDCL_ROOT}/devlib/linux/aarch64"
        "${ASCENDCL_ROOT}/devlib/aarch64"
        "${ASCENDCL_ROOT}/devlib"
        "${ASCENDCL_ROOT}/lib64"
        "${ASCENDCL_ROOT}/lib"
    )
    list(REMOVE_DUPLICATES ASCENDCL_LIBRARY_SEARCH_PATHS)

    find_library(ASCENDCL_LIBRARY ascendcl PATHS ${ASCENDCL_LIBRARY_SEARCH_PATHS} NO_DEFAULT_PATH)
    if(NOT ASCENDCL_LIBRARY)
        message(FATAL_ERROR
            "ENABLE_ASCENDCL=ON requires libascendcl.so. "
            "Searched: ${ASCENDCL_LIBRARY_SEARCH_PATHS}. "
            "Set ASCENDCL_LIB_DIR explicitly if your toolkit uses a different layout."
        )
    endif()

    get_filename_component(ASCENDCL_RESOLVED_LIB_DIR "${ASCENDCL_LIBRARY}" DIRECTORY)
    set(ASCENDCL_LIB_DIR "${ASCENDCL_RESOLVED_LIB_DIR}" CACHE PATH "Resolved AscendCL library directory" FORCE)
endif()

set(SENTINEL_OPENCV_COMPONENTS)
if(ENABLE_OPENCV_PREPROCESSOR)
    list(APPEND SENTINEL_OPENCV_COMPONENTS core imgproc imgcodecs)
endif()
if(ENABLE_OPENCV_POSTPROCESSOR)
    list(APPEND SENTINEL_OPENCV_COMPONENTS core dnn)
endif()
if(SENTINEL_OPENCV_COMPONENTS)
    list(REMOVE_DUPLICATES SENTINEL_OPENCV_COMPONENTS)
    find_package(OpenCV REQUIRED COMPONENTS ${SENTINEL_OPENCV_COMPONENTS})
endif()

file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/generated/include/sentinel/build)

configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/include/sentinel/build/build_config.hpp.in
    ${CMAKE_CURRENT_BINARY_DIR}/generated/include/sentinel/build/build_config.hpp
    @ONLY
)

add_library(sentinel_project_options INTERFACE)

target_compile_features(sentinel_project_options INTERFACE cxx_std_17)
target_compile_definitions(sentinel_project_options
    INTERFACE
        SENTINEL_ENABLE_DEV_LOGGING=$<IF:$<BOOL:${ENABLE_DEV_LOGGING}>,1,0>
        SENTINEL_ENABLE_MOCK_SOURCES=$<IF:$<BOOL:${ENABLE_MOCK_SOURCES}>,1,0>
        SENTINEL_ENABLE_ASCENDCL=$<IF:$<BOOL:${ENABLE_ASCENDCL}>,1,0>
        SENTINEL_ENABLE_OPENCV_PREPROCESSOR=$<IF:$<BOOL:${ENABLE_OPENCV_PREPROCESSOR}>,1,0>
        SENTINEL_ENABLE_OPENCV_POSTPROCESSOR=$<IF:$<BOOL:${ENABLE_OPENCV_POSTPROCESSOR}>,1,0>
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
    src/image/dvpp_image_backend.cpp
    src/image/image_backend_factory.cpp
    src/inference/detector_factory.cpp
    src/inference/mock_detector.cpp
    src/logging/logger.cpp
    src/logging/logger_factory.cpp
    src/logging/stderr_logger.cpp
    src/logging/syslog_logger.cpp
    src/output/null_video_sink.cpp
    src/output/video_sink_factory.cpp
    src/postprocess/dvpp_yolo_postprocessor.cpp
    src/postprocess/postprocessor_factory.cpp
    src/preprocess/dvpp_frame_preprocessor.cpp
    src/preprocess/preprocessor_factory.cpp
    src/video/camera_video_source.cpp
    src/video/rtsp_video_source.cpp
    src/video/video_source_factory.cpp
)

if(ENABLE_ASCENDCL)
    list(APPEND SENTINEL_CORE_SOURCES
        src/inference/ascendcl_detector.cpp
    )
else()
    list(APPEND SENTINEL_CORE_SOURCES
        src/inference/ascendcl_detector_stub.cpp
    )
endif()

if(ENABLE_OPENCV_PREPROCESSOR)
    list(APPEND SENTINEL_CORE_SOURCES
        src/image/opencv_image_backend.cpp
        src/output/debug_image_sink.cpp
        src/output/mjpeg_http_sink.cpp
        src/preprocess/opencv_frame_preprocessor.cpp
    )
else()
    list(APPEND SENTINEL_CORE_SOURCES
        src/image/opencv_image_backend_stub.cpp
        src/output/debug_image_sink_stub.cpp
        src/output/mjpeg_http_sink_stub.cpp
        src/preprocess/opencv_frame_preprocessor_stub.cpp
    )
endif()

if(ENABLE_OPENCV_POSTPROCESSOR)
    list(APPEND SENTINEL_CORE_SOURCES
        src/postprocess/opencv_yolo_postprocessor.cpp
    )
else()
    list(APPEND SENTINEL_CORE_SOURCES
        src/postprocess/opencv_yolo_postprocessor_stub.cpp
    )
endif()

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
        ${CMAKE_CURRENT_BINARY_DIR}/generated/include
)

target_link_libraries(sentinel_core
    PUBLIC
        sentinel_project_options
)

if(ENABLE_ASCENDCL)
    target_include_directories(sentinel_core
        PUBLIC
            ${ASCENDCL_ROOT}/include
    )

    target_link_directories(sentinel_core
        PUBLIC
            ${ASCENDCL_LIB_DIR}
    )

    target_link_libraries(sentinel_core
        PUBLIC
            ${ASCENDCL_LIBRARY}
    )

    set_target_properties(sentinel_core
        PROPERTIES
            BUILD_RPATH "${ASCENDCL_LIB_DIR}"
            INSTALL_RPATH "${ASCENDCL_LIB_DIR}"
    )
endif()

if(SENTINEL_OPENCV_COMPONENTS)
    target_include_directories(sentinel_core
        PUBLIC
            ${OpenCV_INCLUDE_DIRS}
    )

    target_link_libraries(sentinel_core
        PUBLIC
            ${OpenCV_LIBS}
    )
endif()

add_executable(video_sentinel
    src/app/main.cpp
)

target_link_libraries(video_sentinel
    PRIVATE
        sentinel_core
)

add_executable(sentinel_camera_probe
    tools/camera_probe.cpp
)

target_link_libraries(sentinel_camera_probe
    PRIVATE
        sentinel_core
)

install(
    TARGETS
        video_sentinel
        sentinel_camera_probe
    RUNTIME DESTINATION bin
)

install(
    DIRECTORY
        ${CMAKE_CURRENT_SOURCE_DIR}/config/
    DESTINATION config
)

install(
    DIRECTORY
        ${CMAKE_CURRENT_SOURCE_DIR}/models/
    DESTINATION models
    FILES_MATCHING
        PATTERN "*.om"
)

if(INSTALL_DEBUG_SOURCE)
    install(
        FILES
            ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
            ${CMAKE_CURRENT_SOURCE_DIR}/CMakePresets.json
        DESTINATION source
    )

    install(
        DIRECTORY
            ${CMAKE_CURRENT_SOURCE_DIR}/include
            ${CMAKE_CURRENT_SOURCE_DIR}/src
            ${CMAKE_CURRENT_SOURCE_DIR}/tools
            ${CMAKE_CURRENT_SOURCE_DIR}/tests
        DESTINATION source
        PATTERN "CMakeFiles" EXCLUDE
        PATTERN "Testing" EXCLUDE
        PATTERN ".venv" EXCLUDE
        PATTERN ".venv.broken" EXCLUDE
    )

    install(
        FILES
            ${CMAKE_CURRENT_SOURCE_DIR}/cmake/gdbinit.in
        DESTINATION debug
        RENAME gdbinit
    )
endif()

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
            COMMAND sentinel_tests ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/mock_config
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

    add_executable(sentinel_logging_tests
        tests/unit/logger_factory_test.cpp
    )

    target_link_libraries(sentinel_logging_tests
        PRIVATE
            sentinel_core
    )

    add_test(
        NAME sentinel.logging
        COMMAND sentinel_logging_tests
    )

    if(ENABLE_MOCK_SOURCES)
        add_executable(sentinel_detector_tests
            tests/unit/detector_factory_test.cpp
        )

        target_link_libraries(sentinel_detector_tests
            PRIVATE
                sentinel_core
        )

        add_test(
            NAME sentinel.detector_factory
            COMMAND sentinel_detector_tests
        )
    endif()

    if(ENABLE_OPENCV_POSTPROCESSOR)
        add_executable(sentinel_postprocessor_tests
            tests/unit/postprocessor_factory_test.cpp
        )

        target_link_libraries(sentinel_postprocessor_tests
            PRIVATE
                sentinel_core
        )

        add_test(
            NAME sentinel.postprocessor_factory
            COMMAND sentinel_postprocessor_tests
        )
    endif()

    if(ENABLE_OPENCV_PREPROCESSOR)
        add_executable(sentinel_image_backend_tests
            tests/unit/image_backend_test.cpp
        )

        target_link_libraries(sentinel_image_backend_tests
            PRIVATE
                sentinel_core
        )

        add_test(
            NAME sentinel.image_backend
            COMMAND sentinel_image_backend_tests
        )

        add_executable(sentinel_output_tests
            tests/unit/output_sink_test.cpp
        )

        target_link_libraries(sentinel_output_tests
            PRIVATE
                sentinel_core
        )

        add_test(
            NAME sentinel.output_sink
            COMMAND sentinel_output_tests
        )

        add_executable(sentinel_preprocessor_tests
            tests/unit/preprocessor_factory_test.cpp
        )

        target_link_libraries(sentinel_preprocessor_tests
            PRIVATE
                sentinel_core
        )

        add_test(
            NAME sentinel.preprocessor_factory
            COMMAND sentinel_preprocessor_tests
        )
    endif()
endif()

message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "ENABLE_TESTS=${ENABLE_TESTS}")
message(STATUS "ENABLE_MOCK_SOURCES=${ENABLE_MOCK_SOURCES}")
message(STATUS "ENABLE_DEV_WARNINGS=${ENABLE_DEV_WARNINGS}")
message(STATUS "ENABLE_DEV_LOGGING=${ENABLE_DEV_LOGGING}")
message(STATUS "ENABLE_SANITIZERS=${ENABLE_SANITIZERS}")
message(STATUS "ENABLE_ASCENDCL=${ENABLE_ASCENDCL}")
message(STATUS "ENABLE_OPENCV_PREPROCESSOR=${ENABLE_OPENCV_PREPROCESSOR}")
message(STATUS "ENABLE_OPENCV_POSTPROCESSOR=${ENABLE_OPENCV_POSTPROCESSOR}")
message(STATUS "INSTALL_DEBUG_SOURCE=${INSTALL_DEBUG_SOURCE}")
message(STATUS "ASCENDCL_ROOT=${ASCENDCL_ROOT}")
message(STATUS "ASCENDCL_LIB_DIR=${ASCENDCL_LIB_DIR}")
message(STATUS "ASCENDCL_LIBRARY=${ASCENDCL_LIBRARY}")
message(STATUS "SENTINEL_DEFAULT_CONFIG_DIR=${SENTINEL_DEFAULT_CONFIG_DIR}")
