package com.opennoisecanceler.app.engine

import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Assert.assertThrows
import org.junit.Test

class EngineDiagnosticsTest {

    private fun sampleArray(): DoubleArray {
        val values = DoubleArray(22)
        values[0] = 1.0 // running
        values[1] = 48000.0 // inputSampleRateHz
        values[2] = 48000.0 // outputSampleRateHz
        values[3] = 96.0 // inputFramesPerBurst
        values[4] = 96.0 // outputFramesPerBurst
        values[5] = 4096.0 // inputBufferCapacityFrames
        values[6] = 4096.0 // outputBufferCapacityFrames
        values[7] = 192.0 // inputBufferSizeFrames
        values[8] = 192.0 // outputBufferSizeFrames
        values[9] = 12.0 // inputPerformanceMode = LOW_LATENCY
        values[10] = 12.0 // outputPerformanceMode = LOW_LATENCY
        values[11] = 0.0 // inputSharingMode = EXCLUSIVE
        values[12] = 1.0 // outputSharingMode = SHARED
        values[13] = 8.5 // outputLatencyMillis
        values[14] = 4.2 // inputLatencyMillis
        values[15] = 2.0 // inputXRunCount
        values[16] = 1.0 // outputXRunCount
        values[17] = 45.0 // lastCallbackDurationUs
        values[18] = 120.0 // maxCallbackDurationUs
        values[19] = 3.0 // ringBufferUnderrunFrames
        values[20] = 0.0123 // predictionErrorRms
        values[21] = 0.0 // lastErrorCode
        return values
    }

    @Test
    fun `parses a full valid array`() {
        val diag = EngineDiagnostics.fromNativeArray(sampleArray())
        assertEquals(true, diag.running)
        assertEquals(48000, diag.inputSampleRateHz)
        assertEquals(AaudioPerformanceMode.LOW_LATENCY, diag.inputPerformanceMode)
        assertEquals(AaudioSharingMode.EXCLUSIVE, diag.inputSharingMode)
        assertEquals(AaudioSharingMode.SHARED, diag.outputSharingMode)
        assertEquals(8.5, diag.outputLatencyMillis)
        assertEquals(4.2, diag.inputLatencyMillis)
        assertEquals(8.5 + 4.2, diag.estimatedRoundTripLatencyMillis!!, 1e-9)
    }

    @Test
    fun `negative latency sentinel becomes null, not a fabricated value`() {
        val values = sampleArray()
        values[13] = -1.0
        val diag = EngineDiagnostics.fromNativeArray(values)
        assertNull(diag.outputLatencyMillis)
        assertNull(diag.estimatedRoundTripLatencyMillis)
    }

    @Test
    fun `unrecognized performance and sharing mode raw values map to UNKNOWN`() {
        val values = sampleArray()
        values[9] = 999.0
        values[11] = 999.0
        val diag = EngineDiagnostics.fromNativeArray(values)
        assertEquals(AaudioPerformanceMode.UNKNOWN, diag.inputPerformanceMode)
        assertEquals(AaudioSharingMode.UNKNOWN, diag.inputSharingMode)
    }

    @Test
    fun `wrong-size array is rejected rather than silently misread`() {
        assertThrows(IllegalArgumentException::class.java) {
            EngineDiagnostics.fromNativeArray(DoubleArray(5))
        }
    }

    @Test
    fun `EMPTY represents a not-running, unmeasured state`() {
        assertEquals(false, EngineDiagnostics.EMPTY.running)
        assertNull(EngineDiagnostics.EMPTY.outputLatencyMillis)
        assertNull(EngineDiagnostics.EMPTY.estimatedRoundTripLatencyMillis)
    }
}
