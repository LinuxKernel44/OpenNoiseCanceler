package com.opennoisecanceler.app.engine

import com.opennoisecanceler.app.config.AncPreset

/**
 * Thin JNI wrapper around the native `onc::AudioEngine` (see
 * app/src/main/cpp/audio_engine.h). Owns exactly one native instance for
 * its lifetime; callers must call [close] exactly once (AncEngineController
 * does this) to release it.
 *
 * This class does no real-time work itself: `start`/`stop`/`configure` are
 * ordinary (blocking, briefly) calls that open/close Oboe streams on the
 * calling thread, and [diagnostics] just copies out atomics on the native
 * side. None of this runs on the audio callback thread.
 */
class NativeAudioEngine : AutoCloseable {
    private var handle: Long = nativeCreate()
    private var closed = false

    fun start(preset: AncPreset): Boolean {
        check(!closed) { "NativeAudioEngine already closed" }
        return nativeStart(
            handle,
            preset.predictorTaps,
            preset.predictorDelaySamples,
            preset.stepSize,
            preset.leakage,
            preset.highPassHz,
            preset.lowPassHz,
            preset.filterQ,
            preset.strength,
            preset.outputCeiling,
        )
    }

    fun stop() {
        if (closed) return
        nativeStop(handle)
    }

    fun configure(preset: AncPreset): Boolean {
        check(!closed) { "NativeAudioEngine already closed" }
        return nativeConfigure(
            handle,
            preset.predictorTaps,
            preset.predictorDelaySamples,
            preset.stepSize,
            preset.leakage,
            preset.highPassHz,
            preset.lowPassHz,
            preset.filterQ,
            preset.strength,
            preset.outputCeiling,
        )
    }

    fun isRunning(): Boolean {
        if (closed) return false
        return nativeIsRunning(handle)
    }

    fun diagnostics(): EngineDiagnostics {
        if (closed) return EngineDiagnostics.EMPTY
        return EngineDiagnostics.fromNativeArray(nativeGetDiagnostics(handle))
    }

    override fun close() {
        if (closed) return
        nativeStop(handle)
        nativeDestroy(handle)
        closed = true
    }

    private external fun nativeCreate(): Long
    private external fun nativeDestroy(handle: Long)
    private external fun nativeStart(
        handle: Long,
        predictorTaps: Int,
        predictorDelaySamples: Int,
        stepSize: Float,
        leakage: Float,
        highPassHz: Float,
        lowPassHz: Float,
        filterQ: Float,
        strength: Float,
        outputCeiling: Float,
    ): Boolean

    private external fun nativeStop(handle: Long)
    private external fun nativeConfigure(
        handle: Long,
        predictorTaps: Int,
        predictorDelaySamples: Int,
        stepSize: Float,
        leakage: Float,
        highPassHz: Float,
        lowPassHz: Float,
        filterQ: Float,
        strength: Float,
        outputCeiling: Float,
    ): Boolean

    private external fun nativeIsRunning(handle: Long): Boolean
    private external fun nativeGetDiagnostics(handle: Long): DoubleArray

    companion object {
        init {
            System.loadLibrary("opennoisecanceler_native")
        }
    }
}
