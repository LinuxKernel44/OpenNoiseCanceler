#pragma once

#include <atomic>
#include <cstdint>

namespace onc {

// Snapshot of engine diagnostics, safe to read from any thread (all fields
// are plain old data copied out of atomics — see EngineDiagnosticsState).
// Every field here documents whether it is a genuine runtime measurement or
// a value reported by the audio framework/driver that we did not measure
// ourselves, so the UI layer can label things honestly instead of
// presenting estimates as measurements.
struct EngineDiagnosticsSnapshot {
    bool running = false;

    // Reported by AAudio/Oboe for the stream actually opened (not
    // necessarily what was requested if the device had to fall back).
    int32_t inputSampleRateHz = 0;
    int32_t outputSampleRateHz = 0;
    int32_t inputFramesPerBurst = 0;
    int32_t outputFramesPerBurst = 0;
    int32_t inputBufferCapacityFrames = 0;
    int32_t outputBufferCapacityFrames = 0;
    int32_t inputBufferSizeFrames = 0;
    int32_t outputBufferSizeFrames = 0;

    // 0 = Unknown, 1 = None, 2 = MMAP, 3 = MMAP+Exclusive (mirrors
    // oboe::AudioApi / SharingMode / PerformanceMode; translated to plain
    // ints at the JNI boundary rather than exposing Oboe enums to Kotlin).
    int32_t inputPerformanceMode = 0;
    int32_t outputPerformanceMode = 0;
    int32_t inputSharingMode = 0;
    int32_t outputSharingMode = 0;

    // Measured by Oboe from the stream's actual timing/timestamp APIs at
    // query time (not a fixed theoretical constant) but only where the
    // underlying HAL reports timestamps; -1.0 means "not available on this
    // stream/device", which must be shown as such, not as 0 ms latency.
    double outputLatencyMillis = -1.0;
    double inputLatencyMillis = -1.0;

    // Counted by the driver (xRun = buffer underrun on output / overrun on
    // input) since the stream was opened. Genuine measurements.
    int32_t inputXRunCount = 0;
    int32_t outputXRunCount = 0;

    // Measured wall-clock time spent inside our own callback, in
    // microseconds, tracked by us (not estimated).
    int32_t lastCallbackDurationUs = 0;
    int32_t maxCallbackDurationUs = 0;

    // How many frames the DSP ring buffer did not have available when the
    // output callback needed them (distinct from the driver's own xRun
    // counters above; this is our own pipeline underrun counter).
    int64_t ringBufferUnderrunFrames = 0;

    float predictionErrorRms = 0.0f;

    // Set when the engine has stopped itself after a stream error (device
    // disconnect, etc.). 0 = no error.
    int32_t lastErrorCode = 0;
};

// Lock-free counters updated from the real-time audio callbacks and read
// from any other thread for diagnostics polling. Every member is atomic so
// no locking is ever needed on the audio thread.
struct EngineDiagnosticsState {
    std::atomic<int32_t> inputSampleRateHz{0};
    std::atomic<int32_t> outputSampleRateHz{0};
    std::atomic<int32_t> inputFramesPerBurst{0};
    std::atomic<int32_t> outputFramesPerBurst{0};
    std::atomic<int32_t> inputBufferCapacityFrames{0};
    std::atomic<int32_t> outputBufferCapacityFrames{0};
    std::atomic<int32_t> inputBufferSizeFrames{0};
    std::atomic<int32_t> outputBufferSizeFrames{0};
    std::atomic<int32_t> inputPerformanceMode{0};
    std::atomic<int32_t> outputPerformanceMode{0};
    std::atomic<int32_t> inputSharingMode{0};
    std::atomic<int32_t> outputSharingMode{0};
    std::atomic<int32_t> inputXRunCount{0};
    std::atomic<int32_t> outputXRunCount{0};
    std::atomic<int32_t> lastCallbackDurationUs{0};
    std::atomic<int32_t> maxCallbackDurationUs{0};
    std::atomic<int64_t> ringBufferUnderrunFrames{0};
    std::atomic<int32_t> lastErrorCode{0};
    std::atomic<bool> running{false};
    std::atomic<float> predictionErrorRms{0.0f};

    void reset() {
        inputSampleRateHz = 0;
        outputSampleRateHz = 0;
        inputFramesPerBurst = 0;
        outputFramesPerBurst = 0;
        inputBufferCapacityFrames = 0;
        outputBufferCapacityFrames = 0;
        inputBufferSizeFrames = 0;
        outputBufferSizeFrames = 0;
        inputPerformanceMode = 0;
        outputPerformanceMode = 0;
        inputSharingMode = 0;
        outputSharingMode = 0;
        inputXRunCount = 0;
        outputXRunCount = 0;
        lastCallbackDurationUs = 0;
        maxCallbackDurationUs = 0;
        ringBufferUnderrunFrames = 0;
        lastErrorCode = 0;
        running = false;
        predictionErrorRms = 0.0f;
    }
};

} // namespace onc
