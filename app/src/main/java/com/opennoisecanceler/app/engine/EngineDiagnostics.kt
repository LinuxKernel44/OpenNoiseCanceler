package com.opennoisecanceler.app.engine

/**
 * Kotlin-side mirror of `onc::EngineDiagnosticsSnapshot`
 * (app/src/main/cpp/engine_diagnostics.h). Every field is either a genuine
 * runtime measurement (from the audio driver's own counters/timestamps, or
 * from code in this app that measured it) or explicitly `null`/sentinel
 * where no measurement was available — this class must never invent a
 * value to fill a gap.
 */
data class EngineDiagnostics(
    val running: Boolean,
    val inputSampleRateHz: Int,
    val outputSampleRateHz: Int,
    val inputFramesPerBurst: Int,
    val outputFramesPerBurst: Int,
    val inputBufferCapacityFrames: Int,
    val outputBufferCapacityFrames: Int,
    val inputBufferSizeFrames: Int,
    val outputBufferSizeFrames: Int,
    val inputPerformanceMode: AaudioPerformanceMode,
    val outputPerformanceMode: AaudioPerformanceMode,
    val inputSharingMode: AaudioSharingMode,
    val outputSharingMode: AaudioSharingMode,
    /** Milliseconds, from Oboe's `calculateLatencyMillis()`; null if unavailable on this stream. */
    val outputLatencyMillis: Double?,
    val inputLatencyMillis: Double?,
    val inputXRunCount: Int,
    val outputXRunCount: Int,
    val lastCallbackDurationUs: Int,
    val maxCallbackDurationUs: Int,
    val ringBufferUnderrunFrames: Long,
    val predictionErrorRms: Float,
    val lastErrorCode: Int,
) {
    /**
     * Best-effort *estimate* (not a direct measurement) of total one-way
     * pipeline latency: sum of the two streams' reported latencies, which
     * are themselves each already a measurement from that stream alone.
     * Combining them assumes the two are additive and does not include any
     * acoustic propagation time in the room, which cannot be measured
     * without an ear-reference microphone (see docs/DSP_DESIGN.md). Null if
     * either component is unavailable.
     */
    val estimatedRoundTripLatencyMillis: Double?
        get() {
            val out = outputLatencyMillis ?: return null
            val inp = inputLatencyMillis ?: return null
            return out + inp
        }

    companion object {
        private const val ARRAY_SIZE = 22

        fun fromNativeArray(values: DoubleArray): EngineDiagnostics {
            require(values.size == ARRAY_SIZE) {
                "Native diagnostics array size mismatch: expected $ARRAY_SIZE, got ${values.size}"
            }
            fun latency(v: Double): Double? = if (v < 0.0) null else v
            return EngineDiagnostics(
                running = values[0] != 0.0,
                inputSampleRateHz = values[1].toInt(),
                outputSampleRateHz = values[2].toInt(),
                inputFramesPerBurst = values[3].toInt(),
                outputFramesPerBurst = values[4].toInt(),
                inputBufferCapacityFrames = values[5].toInt(),
                outputBufferCapacityFrames = values[6].toInt(),
                inputBufferSizeFrames = values[7].toInt(),
                outputBufferSizeFrames = values[8].toInt(),
                inputPerformanceMode = AaudioPerformanceMode.fromRaw(values[9].toInt()),
                outputPerformanceMode = AaudioPerformanceMode.fromRaw(values[10].toInt()),
                inputSharingMode = AaudioSharingMode.fromRaw(values[11].toInt()),
                outputSharingMode = AaudioSharingMode.fromRaw(values[12].toInt()),
                outputLatencyMillis = latency(values[13]),
                inputLatencyMillis = latency(values[14]),
                inputXRunCount = values[15].toInt(),
                outputXRunCount = values[16].toInt(),
                lastCallbackDurationUs = values[17].toInt(),
                maxCallbackDurationUs = values[18].toInt(),
                ringBufferUnderrunFrames = values[19].toLong(),
                predictionErrorRms = values[20].toFloat(),
                lastErrorCode = values[21].toInt(),
            )
        }

        val EMPTY = EngineDiagnostics(
            running = false,
            inputSampleRateHz = 0,
            outputSampleRateHz = 0,
            inputFramesPerBurst = 0,
            outputFramesPerBurst = 0,
            inputBufferCapacityFrames = 0,
            outputBufferCapacityFrames = 0,
            inputBufferSizeFrames = 0,
            outputBufferSizeFrames = 0,
            inputPerformanceMode = AaudioPerformanceMode.UNKNOWN,
            outputPerformanceMode = AaudioPerformanceMode.UNKNOWN,
            inputSharingMode = AaudioSharingMode.UNKNOWN,
            outputSharingMode = AaudioSharingMode.UNKNOWN,
            outputLatencyMillis = null,
            inputLatencyMillis = null,
            inputXRunCount = 0,
            outputXRunCount = 0,
            lastCallbackDurationUs = 0,
            maxCallbackDurationUs = 0,
            ringBufferUnderrunFrames = 0,
            predictionErrorRms = 0f,
            lastErrorCode = 0,
        )
    }
}

/**
 * Mirrors the stable, published AAudio performance-mode constants that
 * Oboe's `oboe::PerformanceMode` enum is defined in terms of
 * (AAUDIO_PERFORMANCE_MODE_*). LOW_LATENCY with [AaudioSharingMode.EXCLUSIVE]
 * is the best-case low-latency path this app requests; devices/OSes may
 * still fall back to NONE / SHARED, which diagnostics must show honestly.
 */
enum class AaudioPerformanceMode(val raw: Int) {
    UNKNOWN(-1),
    NONE(10),
    POWER_SAVING(11),
    LOW_LATENCY(12);

    companion object {
        fun fromRaw(raw: Int): AaudioPerformanceMode = entries.firstOrNull { it.raw == raw } ?: UNKNOWN
    }
}

/** Mirrors AAUDIO_SHARING_MODE_*. */
enum class AaudioSharingMode(val raw: Int) {
    UNKNOWN(-1),
    EXCLUSIVE(0),
    SHARED(1);

    companion object {
        fun fromRaw(raw: Int): AaudioSharingMode = entries.firstOrNull { it.raw == raw } ?: UNKNOWN
    }
}
