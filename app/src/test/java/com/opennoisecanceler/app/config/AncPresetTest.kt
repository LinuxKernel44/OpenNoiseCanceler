package com.opennoisecanceler.app.config

import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Assert.assertThrows
import org.junit.Assert.assertTrue
import org.junit.Test

class AncPresetTest {

    @Test
    fun `all built-in presets are internally consistent`() {
        for (preset in AncPreset.ALL) {
            assertTrue("${preset.id}: lowPassHz must exceed highPassHz", preset.lowPassHz > preset.highPassHz)
            assertTrue("${preset.id}: strength in [0,1]", preset.strength in 0f..1f)
            assertTrue("${preset.id}: outputCeiling in (0,1]", preset.outputCeiling > 0f && preset.outputCeiling <= 1f)
        }
    }

    @Test
    fun `preset ids are unique`() {
        val ids = AncPreset.ALL.map { it.id }
        assertEquals(ids.size, ids.toSet().size)
    }

    @Test
    fun `byId resolves known presets`() {
        assertEquals(AncPreset.BALANCED, AncPreset.byId("balanced"))
        assertEquals(AncPreset.ULTRA_LOW_LATENCY, AncPreset.byId("ultra_low_latency"))
    }

    @Test
    fun `byId returns null for unknown id`() {
        assertNull(AncPreset.byId("does_not_exist"))
    }

    @Test
    fun `default preset is balanced`() {
        assertEquals(AncPreset.BALANCED, AncPreset.DEFAULT)
    }

    @Test
    fun `construction rejects an invalid band (lowPass at or below highPass)`() {
        assertThrows(IllegalArgumentException::class.java) {
            AncPreset.BALANCED.copy(lowPassHz = AncPreset.BALANCED.highPassHz)
        }
    }

    @Test
    fun `construction rejects out-of-range strength`() {
        assertThrows(IllegalArgumentException::class.java) {
            AncPreset.BALANCED.copy(strength = 1.5f)
        }
        assertThrows(IllegalArgumentException::class.java) {
            AncPreset.BALANCED.copy(strength = -0.1f)
        }
    }

    @Test
    fun `construction rejects zero or negative predictor taps`() {
        assertThrows(IllegalArgumentException::class.java) {
            AncPreset.BALANCED.copy(predictorTaps = 0)
        }
    }

    @Test
    fun `construction rejects non-positive filter Q`() {
        assertThrows(IllegalArgumentException::class.java) {
            AncPreset.BALANCED.copy(filterQ = 0f)
        }
    }

    @Test
    fun `ultra low latency preset uses fewer taps than high quality`() {
        assertTrue(AncPreset.ULTRA_LOW_LATENCY.predictorTaps < AncPreset.HIGH_QUALITY.predictorTaps)
        assertTrue(AncPreset.ULTRA_LOW_LATENCY.predictorDelaySamples < AncPreset.HIGH_QUALITY.predictorDelaySamples)
    }

    @Test
    fun `AppSettings effectivePreset uses preset default when no override is set`() {
        val settings = AppSettings(selectedPresetId = AncPreset.HIGH_QUALITY.id, strengthOverride = null)
        assertEquals(AncPreset.HIGH_QUALITY.strength, settings.effectivePreset.strength)
    }

    @Test
    fun `AppSettings effectivePreset applies a strength override`() {
        val settings = AppSettings(selectedPresetId = AncPreset.HIGH_QUALITY.id, strengthOverride = 0.2f)
        assertEquals(0.2f, settings.effectivePreset.strength)
    }

    @Test
    fun `AppSettings effectivePreset clamps an out-of-range override`() {
        val settings = AppSettings(selectedPresetId = AncPreset.BALANCED.id, strengthOverride = 5.0f)
        assertEquals(1.0f, settings.effectivePreset.strength)
    }

    @Test
    fun `AppSettings falls back to default preset for an unknown id`() {
        val settings = AppSettings(selectedPresetId = "not_a_real_preset")
        assertNotNull(settings.selectedPreset)
        assertEquals(AncPreset.DEFAULT, settings.selectedPreset)
    }
}
