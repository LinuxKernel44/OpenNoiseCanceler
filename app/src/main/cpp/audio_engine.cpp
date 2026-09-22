#include "audio_engine.h"

#include <algorithm>
#include <chrono>

namespace onc {

namespace {

// Update `slot` to hold the larger of its current value and `value`,
// lock-free. Safe to call from the real-time thread.
void atomicMax(std::atomic<int32_t>& slot, int32_t value) {
    int32_t prev = slot.load(std::memory_order_relaxed);
    while (value > prev && !slot.compare_exchange_weak(prev, value, std::memory_order_relaxed)) {
    }
}

} // namespace

AudioEngine::AudioEngine() = default;

AudioEngine::~AudioEngine() {
    stop();
}

bool AudioEngine::start(const dsp::AncParameters& params) {
    std::lock_guard<std::mutex> lock(controlMutex_);
    if (diagnostics_.running.load(std::memory_order_acquire)) {
        return true; // already running
    }
    diagnostics_.reset();
    ringBuffer_.reset();

    if (!openStreams(params)) {
        closeStreams();
        return false;
    }

    currentParams_ = params;
    processor_.configure(params);

    // Start input first: by the time the output callback first runs, there
    // is a better chance real captured audio is already flowing into the
    // ring buffer instead of the output immediately underrunning.
    if (inputStream_->requestStart() != oboe::Result::OK) {
        closeStreams();
        return false;
    }
    if (outputStream_->requestStart() != oboe::Result::OK) {
        closeStreams();
        return false;
    }

    diagnostics_.running.store(true, std::memory_order_release);
    return true;
}

void AudioEngine::stop() {
    std::lock_guard<std::mutex> lock(controlMutex_);
    closeStreams();
    diagnostics_.running.store(false, std::memory_order_release);
}

bool AudioEngine::configure(const dsp::AncParameters& params) {
    std::lock_guard<std::mutex> lock(controlMutex_);
    const bool wasRunning = diagnostics_.running.load(std::memory_order_acquire);
    if (!wasRunning) {
        currentParams_ = params;
        processor_.configure(params);
        return true;
    }

    closeStreams();
    diagnostics_.running.store(false, std::memory_order_release);

    if (!openStreams(params)) {
        closeStreams();
        return false;
    }
    currentParams_ = params;
    processor_.configure(params);

    if (inputStream_->requestStart() != oboe::Result::OK) {
        closeStreams();
        return false;
    }
    if (outputStream_->requestStart() != oboe::Result::OK) {
        closeStreams();
        return false;
    }
    diagnostics_.running.store(true, std::memory_order_release);
    return true;
}

bool AudioEngine::openStreams(const dsp::AncParameters& params) {
    oboe::AudioStreamBuilder inputBuilder;
    inputBuilder.setDirection(oboe::Direction::Input)
        ->setPerformanceMode(oboe::PerformanceMode::LowLatency)
        ->setSharingMode(oboe::SharingMode::Exclusive)
        ->setFormat(oboe::AudioFormat::Float)
        ->setFormatConversionAllowed(true)
        ->setChannelCount(oboe::ChannelCount::Mono)
        ->setSampleRateConversionQuality(oboe::SampleRateConversionQuality::None)
        // Request the platform's least-processed input path: our own DSP
        // does the noise handling, so the built-in AGC/noise
        // suppressor/echo canceller must stay out of the way rather than
        // altering the reference signal before we ever see it.
        ->setInputPreset(oboe::InputPreset::Unprocessed)
        ->setCallback(this);

    if (inputBuilder.openStream(inputStream_) != oboe::Result::OK) {
        return false;
    }

    oboe::AudioStreamBuilder outputBuilder;
    outputBuilder.setDirection(oboe::Direction::Output)
        ->setPerformanceMode(oboe::PerformanceMode::LowLatency)
        ->setSharingMode(oboe::SharingMode::Exclusive)
        ->setFormat(oboe::AudioFormat::Float)
        ->setFormatConversionAllowed(true)
        ->setChannelCount(oboe::ChannelCount::Stereo)
        ->setSampleRateConversionQuality(oboe::SampleRateConversionQuality::None)
        ->setUsage(oboe::Usage::Media)
        ->setCallback(this);

    if (outputBuilder.openStream(outputStream_) != oboe::Result::OK) {
        return false;
    }

    // This app does not resample between input and output: if the device
    // negotiated different native rates for capture and playback, the DSP
    // pipeline (single AncProcessor instance, single sample rate) cannot
    // process both correctly. Fail explicitly rather than silently running
    // with mismatched rates.
    if (inputStream_->getSampleRate() != outputStream_->getSampleRate()) {
        return false;
    }

    // Oboe best practice for low latency: after opening, size the buffer to
    // a small multiple of the burst size instead of leaving it at the
    // (larger) default, trading a little extra underrun risk for lower
    // latency.
    const int32_t inBurst = inputStream_->getFramesPerBurst();
    const int32_t outBurst = outputStream_->getFramesPerBurst();
    inputStream_->setBufferSizeInFrames(inBurst * 2);
    outputStream_->setBufferSizeInFrames(outBurst * 2);

    (void)params; // sample rate comes from the negotiated streams, not params
    captureStreamInfo();
    return true;
}

void AudioEngine::closeStreams() {
    if (inputStream_) {
        inputStream_->stop();
        inputStream_->close();
        inputStream_.reset();
    }
    if (outputStream_) {
        outputStream_->stop();
        outputStream_->close();
        outputStream_.reset();
    }
}

void AudioEngine::captureStreamInfo() {
    if (inputStream_) {
        diagnostics_.inputSampleRateHz.store(inputStream_->getSampleRate(), std::memory_order_relaxed);
        diagnostics_.inputFramesPerBurst.store(inputStream_->getFramesPerBurst(), std::memory_order_relaxed);
        diagnostics_.inputBufferCapacityFrames.store(inputStream_->getBufferCapacityInFrames(), std::memory_order_relaxed);
        diagnostics_.inputBufferSizeFrames.store(inputStream_->getBufferSizeInFrames(), std::memory_order_relaxed);
        diagnostics_.inputPerformanceMode.store(static_cast<int32_t>(inputStream_->getPerformanceMode()), std::memory_order_relaxed);
        diagnostics_.inputSharingMode.store(static_cast<int32_t>(inputStream_->getSharingMode()), std::memory_order_relaxed);
    }
    if (outputStream_) {
        diagnostics_.outputSampleRateHz.store(outputStream_->getSampleRate(), std::memory_order_relaxed);
        diagnostics_.outputFramesPerBurst.store(outputStream_->getFramesPerBurst(), std::memory_order_relaxed);
        diagnostics_.outputBufferCapacityFrames.store(outputStream_->getBufferCapacityInFrames(), std::memory_order_relaxed);
        diagnostics_.outputBufferSizeFrames.store(outputStream_->getBufferSizeInFrames(), std::memory_order_relaxed);
        diagnostics_.outputPerformanceMode.store(static_cast<int32_t>(outputStream_->getPerformanceMode()), std::memory_order_relaxed);
        diagnostics_.outputSharingMode.store(static_cast<int32_t>(outputStream_->getSharingMode()), std::memory_order_relaxed);
    }
}

oboe::DataCallbackResult AudioEngine::onAudioReady(oboe::AudioStream* stream, void* audioData, int32_t numFrames) {
    if (stream == inputStream_.get()) {
        return handleInputCallback(stream, static_cast<const float*>(audioData), numFrames);
    }
    return handleOutputCallback(stream, static_cast<float*>(audioData), numFrames);
}

oboe::DataCallbackResult AudioEngine::handleInputCallback(oboe::AudioStream* stream, const float* data, int32_t numFrames) {
    const auto startTime = std::chrono::steady_clock::now();

    const int32_t channelCount = stream->getChannelCount();
    if (channelCount <= 0 || numFrames <= 0) {
        return oboe::DataCallbackResult::Continue;
    }

    // We requested Mono; if the device still delivered more channels,
    // defensively take channel 0 rather than reading out of bounds or
    // silently mixing.
    size_t written;
    if (channelCount == 1) {
        written = ringBuffer_.write(data, static_cast<size_t>(numFrames));
    } else {
        // Downmix by picking the first channel, using the pre-allocated
        // scratch buffer so this stays allocation-free.
        const size_t frames = std::min<size_t>(numFrames, scratchMono_.size());
        for (size_t i = 0; i < frames; ++i) {
            scratchMono_[i] = data[i * channelCount];
        }
        written = ringBuffer_.write(scratchMono_.data(), frames);
    }

    if (written < static_cast<size_t>(numFrames)) {
        diagnostics_.ringBufferUnderrunFrames.fetch_add(static_cast<int64_t>(numFrames) - static_cast<int64_t>(written),
                                                          std::memory_order_relaxed);
    }

    const auto inputXRun = stream->getXRunCount();
    if (inputXRun) {
        diagnostics_.inputXRunCount.store(inputXRun.value(), std::memory_order_relaxed);
    }

    const auto elapsedUs = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - startTime).count();
    diagnostics_.lastCallbackDurationUs.store(static_cast<int32_t>(elapsedUs), std::memory_order_relaxed);
    atomicMax(diagnostics_.maxCallbackDurationUs, static_cast<int32_t>(elapsedUs));

    return oboe::DataCallbackResult::Continue;
}

oboe::DataCallbackResult AudioEngine::handleOutputCallback(oboe::AudioStream* stream, float* data, int32_t numFrames) {
    const auto startTime = std::chrono::steady_clock::now();

    const int32_t channelCount = stream->getChannelCount();
    const size_t frames = std::min<size_t>(std::max(numFrames, 0), scratchMono_.size());

    const size_t available = ringBuffer_.read(scratchMono_.data(), frames);
    if (available < frames) {
        std::fill(scratchMono_.begin() + static_cast<long>(available), scratchMono_.begin() + static_cast<long>(frames), 0.0f);
        diagnostics_.ringBufferUnderrunFrames.fetch_add(static_cast<int64_t>(frames) - static_cast<int64_t>(available),
                                                          std::memory_order_relaxed);
    }

    processor_.processBlock(scratchMono_.data(), scratchMono_.data(), frames);
    diagnostics_.predictionErrorRms.store(processor_.predictionErrorRms(), std::memory_order_relaxed);

    if (channelCount == 1) {
        std::copy(scratchMono_.begin(), scratchMono_.begin() + static_cast<long>(frames), data);
    } else {
        for (size_t i = 0; i < frames; ++i) {
            const float sample = scratchMono_[i];
            for (int32_t ch = 0; ch < channelCount; ++ch) {
                data[i * channelCount + ch] = sample;
            }
        }
    }
    // Zero-fill any tail beyond what we could produce (numFrames exceeded
    // our scratch capacity — should not happen in practice given
    // kMaxScratchFrames, but this keeps the contract safe regardless).
    for (size_t i = frames; i < static_cast<size_t>(numFrames); ++i) {
        for (int32_t ch = 0; ch < channelCount; ++ch) {
            data[i * channelCount + ch] = 0.0f;
        }
    }

    const auto outputXRun = stream->getXRunCount();
    if (outputXRun) {
        diagnostics_.outputXRunCount.store(outputXRun.value(), std::memory_order_relaxed);
    }

    const auto elapsedUs = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - startTime).count();
    diagnostics_.lastCallbackDurationUs.store(static_cast<int32_t>(elapsedUs), std::memory_order_relaxed);
    atomicMax(diagnostics_.maxCallbackDurationUs, static_cast<int32_t>(elapsedUs));

    return oboe::DataCallbackResult::Continue;
}

void AudioEngine::onErrorBeforeClose(oboe::AudioStream* /*stream*/, oboe::Result error) {
    diagnostics_.lastErrorCode.store(static_cast<int32_t>(error), std::memory_order_relaxed);
}

void AudioEngine::onErrorAfterClose(oboe::AudioStream* /*stream*/, oboe::Result error) {
    // Deliberately does not auto-restart: silently resuming playback of
    // anti-noise through a route that just changed underneath us (e.g. the
    // wired headset was unplugged and something else is now the output)
    // is a safety concern, not just an inconvenience. The engine stops and
    // surfaces the error; restarting is an explicit user action driven from
    // the foreground service observing isRunning()/diagnostics.
    diagnostics_.lastErrorCode.store(static_cast<int32_t>(error), std::memory_order_relaxed);
    diagnostics_.running.store(false, std::memory_order_release);
}

EngineDiagnosticsSnapshot AudioEngine::diagnosticsSnapshot() const {
    EngineDiagnosticsSnapshot snap;
    snap.running = diagnostics_.running.load(std::memory_order_acquire);
    snap.inputSampleRateHz = diagnostics_.inputSampleRateHz.load(std::memory_order_relaxed);
    snap.outputSampleRateHz = diagnostics_.outputSampleRateHz.load(std::memory_order_relaxed);
    snap.inputFramesPerBurst = diagnostics_.inputFramesPerBurst.load(std::memory_order_relaxed);
    snap.outputFramesPerBurst = diagnostics_.outputFramesPerBurst.load(std::memory_order_relaxed);
    snap.inputBufferCapacityFrames = diagnostics_.inputBufferCapacityFrames.load(std::memory_order_relaxed);
    snap.outputBufferCapacityFrames = diagnostics_.outputBufferCapacityFrames.load(std::memory_order_relaxed);
    snap.inputBufferSizeFrames = diagnostics_.inputBufferSizeFrames.load(std::memory_order_relaxed);
    snap.outputBufferSizeFrames = diagnostics_.outputBufferSizeFrames.load(std::memory_order_relaxed);
    snap.inputPerformanceMode = diagnostics_.inputPerformanceMode.load(std::memory_order_relaxed);
    snap.outputPerformanceMode = diagnostics_.outputPerformanceMode.load(std::memory_order_relaxed);
    snap.inputSharingMode = diagnostics_.inputSharingMode.load(std::memory_order_relaxed);
    snap.outputSharingMode = diagnostics_.outputSharingMode.load(std::memory_order_relaxed);
    snap.inputXRunCount = diagnostics_.inputXRunCount.load(std::memory_order_relaxed);
    snap.outputXRunCount = diagnostics_.outputXRunCount.load(std::memory_order_relaxed);
    snap.lastCallbackDurationUs = diagnostics_.lastCallbackDurationUs.load(std::memory_order_relaxed);
    snap.maxCallbackDurationUs = diagnostics_.maxCallbackDurationUs.load(std::memory_order_relaxed);
    snap.ringBufferUnderrunFrames = diagnostics_.ringBufferUnderrunFrames.load(std::memory_order_relaxed);
    snap.predictionErrorRms = diagnostics_.predictionErrorRms.load(std::memory_order_relaxed);
    snap.lastErrorCode = diagnostics_.lastErrorCode.load(std::memory_order_relaxed);

    std::lock_guard<std::mutex> lock(controlMutex_);
    if (outputStream_) {
        const auto result = outputStream_->calculateLatencyMillis();
        snap.outputLatencyMillis = result ? result.value() : -1.0;
    }
    if (inputStream_) {
        const auto result = inputStream_->calculateLatencyMillis();
        snap.inputLatencyMillis = result ? result.value() : -1.0;
    }
    return snap;
}

} // namespace onc
