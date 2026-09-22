package com.opennoisecanceler.app.engine

import com.opennoisecanceler.app.config.AncPreset
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Job
import kotlinx.coroutines.delay
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.launch

/** Observable lifecycle state of the ANC engine, driven only from the main/coroutine thread. */
sealed interface EngineState {
    data object Stopped : EngineState
    data object Starting : EngineState
    data object Running : EngineState
    data class Error(val message: String) : EngineState
}

/**
 * Coordinates [NativeAudioEngine] with app-level concerns that must not run
 * on the real-time audio thread: state tracking, periodic diagnostics
 * polling, and a one-time latency-adaptive refinement of the predictor's
 * delay parameter once real measurements are available.
 *
 * All of this class's public methods are expected to be called from the
 * main thread; none of it touches the audio callback.
 */
class AncEngineController(private val scope: CoroutineScope) {
    private val nativeEngine = NativeAudioEngine()

    private val _state = MutableStateFlow<EngineState>(EngineState.Stopped)
    val state: StateFlow<EngineState> = _state.asStateFlow()

    private val _diagnostics = MutableStateFlow(EngineDiagnostics.EMPTY)
    val diagnostics: StateFlow<EngineDiagnostics> = _diagnostics.asStateFlow()

    private var activePreset: AncPreset = AncPreset.DEFAULT
    private var pollingJob: Job? = null
    private var hasRecalibratedDelay = false

    fun start(preset: AncPreset) {
        activePreset = preset
        hasRecalibratedDelay = false
        _state.value = EngineState.Starting

        val started = nativeEngine.start(preset)
        if (!started) {
            _state.value = EngineState.Error("Failed to start the audio engine. See diagnostics for details.")
            return
        }
        _state.value = EngineState.Running
        startPolling()
    }

    fun stop() {
        pollingJob?.cancel()
        pollingJob = null
        nativeEngine.stop()
        _state.value = EngineState.Stopped
        _diagnostics.value = EngineDiagnostics.EMPTY
    }

    /** Applies a new preset. If currently running, this briefly restarts the audio streams. */
    fun updatePreset(preset: AncPreset) {
        activePreset = preset
        if (_state.value !is EngineState.Running) return
        hasRecalibratedDelay = false
        val ok = nativeEngine.configure(preset)
        if (!ok) {
            _state.value = EngineState.Error("Failed to apply preset '${preset.displayName}'.")
        }
    }

    fun release() {
        pollingJob?.cancel()
        nativeEngine.close()
    }

    private fun startPolling() {
        pollingJob?.cancel()
        pollingJob = scope.launch {
            while (true) {
                val snapshot = nativeEngine.diagnostics()
                _diagnostics.value = snapshot

                if (!snapshot.running && _state.value is EngineState.Running) {
                    // The engine stopped itself (e.g. device disconnected or a
                    // stream error) — see AudioEngine::onErrorAfterClose.
                    _state.value = EngineState.Error(
                        errorMessageFor(snapshot.lastErrorCode)
                    )
                    return@launch
                }

                maybeRecalibrateDelay(snapshot)

                delay(POLL_INTERVAL_MS)
            }
        }
    }

    /**
     * Once real latency measurements are available, refine the predictor's
     * forecast horizon to match: the delay the predictor should forecast
     * ahead by is exactly the pipeline latency it needs to compensate for.
     * Applied at most once per start()/updatePreset() to avoid repeatedly
     * restarting the stream as the latency estimate settles.
     */
    private fun maybeRecalibrateDelay(snapshot: EngineDiagnostics) {
        if (hasRecalibratedDelay) return
        val estimatedMs = snapshot.estimatedRoundTripLatencyMillis ?: return
        val sampleRate = snapshot.outputSampleRateHz
        if (sampleRate <= 0) return

        val measuredDelaySamples = ((estimatedMs / 1000.0) * sampleRate).toInt()
            .coerceIn(MIN_DELAY_SAMPLES, MAX_DELAY_SAMPLES)

        hasRecalibratedDelay = true

        val current = activePreset.predictorDelaySamples
        val relativeDelta = kotlin.math.abs(measuredDelaySamples - current).toDouble() / current
        if (relativeDelta < RECALIBRATION_THRESHOLD) return

        val recalibrated = activePreset.copy(predictorDelaySamples = measuredDelaySamples)
        activePreset = recalibrated
        nativeEngine.configure(recalibrated)
    }

    private fun errorMessageFor(code: Int): String = when (code) {
        0 -> "The audio engine stopped unexpectedly."
        else -> "The audio engine stopped after a stream error (code $code), such as the wired " +
            "headset being disconnected. Reconnect it and restart ANC."
    }

    companion object {
        private const val POLL_INTERVAL_MS = 500L
        private const val MIN_DELAY_SAMPLES = 8
        private const val MAX_DELAY_SAMPLES = 4096
        private const val RECALIBRATION_THRESHOLD = 0.2
    }
}
