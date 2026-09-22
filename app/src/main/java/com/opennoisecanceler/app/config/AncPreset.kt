package com.opennoisecanceler.app.config

/**
 * A named, complete configuration of the ANC DSP pipeline.
 *
 * These fields mirror `onc::dsp::AncParameters` (see
 * app/src/main/cpp/dsp/anc_parameters.h) field-for-field; they are passed
 * across the JNI boundary as individual primitives by
 * [com.opennoisecanceler.app.engine.NativeAudioEngine].
 *
 * The four presets below trade predictor filter length / delay window
 * (memory of past samples, CPU cost) and adaptation rate against how
 * quickly and smoothly the predictor tracks the noise. They are a starting
 * point, not a result of on-device tuning: the project's `sampleRateHz` is
 * always overridden at start time with whatever the audio stream actually
 * negotiates (see AncEngineController), and `predictorDelaySamples` is
 * further refined once after start from measured pipeline latency (see
 * AncEngineController.recalibrateDelay). Real cancellation quality numbers
 * for each preset are not yet available — see docs/HARDWARE_TESTING.md.
 */
data class AncPreset(
    val id: String,
    val displayName: String,
    val description: String,
    val predictorTaps: Int,
    val predictorDelaySamples: Int,
    val stepSize: Float,
    val leakage: Float,
    val highPassHz: Float,
    val lowPassHz: Float,
    val filterQ: Float,
    val strength: Float,
    val outputCeiling: Float,
) {
    init {
        require(predictorTaps in 4..256) { "predictorTaps out of supported range: $predictorTaps" }
        require(predictorDelaySamples in 1..8192) { "predictorDelaySamples out of supported range: $predictorDelaySamples" }
        require(stepSize > 0f && stepSize <= 2f) { "stepSize out of supported range: $stepSize" }
        require(leakage in 0f..0.01f) { "leakage out of supported range: $leakage" }
        require(lowPassHz > highPassHz) { "lowPassHz ($lowPassHz) must exceed highPassHz ($highPassHz)" }
        require(highPassHz >= 0f) { "highPassHz must not be negative: $highPassHz" }
        require(filterQ > 0f) { "filterQ must be positive: $filterQ" }
        require(strength in 0f..1f) { "strength out of range: $strength" }
        require(outputCeiling in 0f..1f) { "outputCeiling out of range: $outputCeiling" }
    }

    companion object {
        val ULTRA_LOW_LATENCY = AncPreset(
            id = "ultra_low_latency",
            displayName = "Ultra Low Latency",
            description = "Smallest predictor and shortest lookback window. Lowest CPU cost and the " +
                "fastest possible reaction to the measured pipeline delay, at the cost of a narrower " +
                "effective cancellation bandwidth and less smooth adaptation.",
            predictorTaps = 16,
            predictorDelaySamples = 64,
            stepSize = 0.6f,
            leakage = 2.0e-5f,
            highPassHz = 20f,
            lowPassHz = 500f,
            filterQ = 0.707f,
            strength = 0.6f,
            outputCeiling = 0.85f,
        )

        val LOW_LATENCY = AncPreset(
            id = "low_latency",
            displayName = "Low Latency",
            description = "A smaller, faster predictor than Balanced, trading some cancellation " +
                "bandwidth and adaptation smoothness for lower CPU cost and faster tracking.",
            predictorTaps = 24,
            predictorDelaySamples = 96,
            stepSize = 0.5f,
            leakage = 1.5e-5f,
            highPassHz = 20f,
            lowPassHz = 650f,
            filterQ = 0.707f,
            strength = 0.65f,
            outputCeiling = 0.85f,
        )

        val BALANCED = AncPreset(
            id = "balanced",
            displayName = "Balanced",
            description = "The recommended default: a moderate predictor length and adaptation rate " +
                "balancing cancellation bandwidth, adaptation smoothness, and CPU cost.",
            predictorTaps = 32,
            predictorDelaySamples = 128,
            stepSize = 0.5f,
            leakage = 1.0e-5f,
            highPassHz = 20f,
            lowPassHz = 800f,
            filterQ = 0.707f,
            strength = 0.7f,
            outputCeiling = 0.9f,
        )

        val HIGH_QUALITY = AncPreset(
            id = "high_quality",
            displayName = "High Quality",
            description = "The longest predictor and slowest adaptation rate, aiming for the smoothest " +
                "tracking and widest effective cancellation bandwidth this design supports. Highest CPU " +
                "cost and the slowest to settle after the noise environment changes.",
            predictorTaps = 48,
            predictorDelaySamples = 192,
            stepSize = 0.35f,
            leakage = 5.0e-6f,
            highPassHz = 20f,
            lowPassHz = 1000f,
            filterQ = 0.707f,
            strength = 0.75f,
            outputCeiling = 0.9f,
        )

        val ALL: List<AncPreset> = listOf(ULTRA_LOW_LATENCY, LOW_LATENCY, BALANCED, HIGH_QUALITY)

        val DEFAULT: AncPreset = BALANCED

        fun byId(id: String): AncPreset? = ALL.firstOrNull { it.id == id }
    }
}
