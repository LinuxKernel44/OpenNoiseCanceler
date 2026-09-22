# Android/NDK build of the real-time audio engine. This is the ONLY CMake
# project that depends on Oboe/JNI/Android; the platform-independent DSP
# core it links against is also built and unit tested completely separately
# on the host by native-test/CMakeLists.txt.

cmake_minimum_required(VERSION 3.22.1)
project(opennoisecanceler_native CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(opennoisecanceler_native SHARED
    jni_bridge.cpp
    audio_engine.cpp
    dsp/biquad_filter.cpp
    dsp/adaptive_predictor.cpp
    dsp/output_limiter.cpp
    dsp/anc_processor.cpp
)

target_include_directories(opennoisecanceler_native PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

find_package(oboe REQUIRED CONFIG)

target_link_libraries(opennoisecanceler_native
    PRIVATE
        oboe::oboe
        android
        log
)

target_compile_options(opennoisecanceler_native PRIVATE -Wall -Wextra -Werror=reorder)
