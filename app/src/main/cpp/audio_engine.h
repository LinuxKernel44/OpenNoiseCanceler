#pragma once

#include <atomic>
#include <memory>
#include <mutex>
#include <vector>

#include <oboe/Oboe.h>

#include "dsp/anc_parameters.h"
#include "dsp/anc_processor.h"
#include "dsp/ring_buffer.h"
#include "engine_diagnostics.h"

namespace onc {

// Owns the two real-time Oboe streams (microphone input, headphone output)
// and the DSP processing that sits between them.
//
// Threading model: Oboe delivers `onAudioReady` for the input stream and the
// output stream on Oboe's own high-priority callback thread(s) (on this
// device, both streams typically share Android's shared low-latency audio
// thread, but the code below does not assume that). The two callbacks
// communicate only through the lock-free `RingBuffer` and the atomics in
// `EngineDiagnosticsState`; there is no mutex, allocation, blocking I/O, or
// logging on either callback path. `start()`/`stop()`/`configure()` run on
// the calling (non-real-time) thread and must not be called concurrently
// with each other.
class AudioEngine : public oboe::AudioStreamCallback {
public:
    AudioEngine();
    ~AudioEngine() override;

    AudioEngine(const AudioEngine&) = delete;
    AudioEngine& operator=(const AudioEngine&) = delete;

    // Opens both streams and starts real-time processing with the given
    // parameters. Returns true on success. Safe to call from any thread but
    // not concurrently with start()/stop()/configure().
    bool start(const dsp::AncParameters& params);

    // Stops and closes both streams. Idempotent.
    void stop();

    bool isRunning() const { return diagnostics_.running.load(std::memory_order_acquire); }

    // Reconfigures DSP parameters. If the engine is currently running, this
    // stops and restarts the streams (parameter changes such as filter
    // length are not real-time safe to apply in place).
    bool configure(const dsp::AncParameters& params);

    EngineDiagnosticsSnapshot diagnosticsSnapshot() const;

    // oboe::AudioStreamCallback
    oboe::DataCallbackResult onAudioReady(oboe::AudioStream* stream, void* audioData, int32_t numFrames) override;
    void onErrorBeforeClose(oboe::AudioStream* stream, oboe::Result error) override;
    void onErrorAfterClose(oboe::AudioStream* stream, oboe::Result error) override;

private:
    bool openStreams(const dsp::AncParameters& params);
    void closeStreams();
    void captureStreamInfo();

    oboe::DataCallbackResult handleInputCallback(oboe::AudioStream* stream, const float* data, int32_t numFrames);
    oboe::DataCallbackResult handleOutputCallback(oboe::AudioStream* stream, float* data, int32_t numFrames);

    std::shared_ptr<oboe::AudioStream> inputStream_;
    std::shared_ptr<oboe::AudioStream> outputStream_;

    dsp::AncProcessor processor_;
    dsp::AncParameters currentParams_;

    // Sized generously (see kRingBufferFrames) relative to expected burst
    // sizes so short scheduling jitter between the two callback threads
    // does not by itself cause an underrun.
    static constexpr size_t kRingBufferFrames = 8192;
    dsp::RingBuffer ringBuffer_{kRingBufferFrames};

    // Pre-allocated scratch buffer for the output callback's DSP pass. Sized
    // to the largest plausible callback frame count and never resized after
    // start() to keep the real-time path allocation-free.
    static constexpr size_t kMaxScratchFrames = 4096;
    std::vector<float> scratchMono_{std::vector<float>(kMaxScratchFrames, 0.0f)};

    EngineDiagnosticsState diagnostics_;

    // Guards start()/stop()/configure() against concurrent calls from
    // multiple non-real-time threads (e.g. the service and a diagnostics
    // poll racing). Never touched from onAudioReady. Mutable so the
    // const diagnosticsSnapshot() can also take it briefly to safely read
    // the stream shared_ptrs.
    mutable std::mutex controlMutex_;
};

} // namespace onc
