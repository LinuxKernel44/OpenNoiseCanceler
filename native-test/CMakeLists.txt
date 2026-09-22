# Host-only build of the DSP core's unit tests. Deliberately independent of
# the Android/NDK toolchain and of app/src/main/cpp/CMakeLists.txt so these
# tests can run on any Linux/macOS/Windows dev machine without an Android
# SDK, NDK, or connected device:
#
#   cmake -S native-test -B build-native-test
#   cmake --build build-native-test
#   ctest --test-dir build-native-test --output-on-failure
#
# Only the platform-independent DSP sources (app/src/main/cpp/dsp/*) are
# compiled here. audio_engine.cpp / jni_bridge.cpp are intentionally excluded
# since they depend on Oboe and JNI and are exercised on-device instead (see
# docs/HARDWARE_TESTING.md).

cmake_minimum_required(VERSION 3.16)
project(OpenNoiseCancelerDspTests CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE RelWithDebInfo)
endif()

set(DSP_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../app/src/main/cpp/dsp)

add_executable(dsp_tests
    main.cpp
    test_ring_buffer.cpp
    test_biquad_filter.cpp
    test_adaptive_predictor.cpp
    test_output_limiter.cpp
    test_anc_processor.cpp
    ${DSP_DIR}/biquad_filter.cpp
    ${DSP_DIR}/adaptive_predictor.cpp
    ${DSP_DIR}/output_limiter.cpp
    ${DSP_DIR}/anc_processor.cpp
)

target_include_directories(dsp_tests PRIVATE ${DSP_DIR} ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_options(dsp_tests PRIVATE -Wall -Wextra -Werror=reorder)

enable_testing()
add_test(NAME dsp_tests COMMAND dsp_tests)
