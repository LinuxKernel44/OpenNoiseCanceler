package com.opennoisecanceler.app.ui

import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.verticalScroll
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.Settings
import androidx.compose.material.icons.filled.Warning
import androidx.compose.material3.Card
import androidx.compose.material3.CardDefaults
import androidx.compose.material3.ExperimentalMaterial3Api
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Scaffold
import androidx.compose.material3.Switch
import androidx.compose.material3.Text
import androidx.compose.material3.TopAppBar
import androidx.compose.runtime.Composable
import androidx.compose.foundation.background
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import com.opennoisecanceler.app.config.AncPreset
import com.opennoisecanceler.app.engine.EngineDiagnostics
import com.opennoisecanceler.app.engine.EngineState
import com.opennoisecanceler.app.ui.theme.AccentActive
import com.opennoisecanceler.app.ui.theme.AccentError
import com.opennoisecanceler.app.ui.theme.AccentInactive
import com.opennoisecanceler.app.ui.theme.AccentWarning

@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun HomeScreen(
    engineState: EngineState,
    diagnostics: EngineDiagnostics,
    activePreset: AncPreset,
    micPermissionGranted: Boolean,
    onToggleAnc: (Boolean) -> Unit,
    onOpenPresets: () -> Unit,
    onOpenSettings: () -> Unit,
) {
    Scaffold(
        topBar = {
            TopAppBar(
                title = { Text("OpenNoiseCanceler") },
                actions = {
                    IconButton(onClick = onOpenSettings) {
                        Icon(Icons.Filled.Settings, contentDescription = "Settings")
                    }
                },
            )
        },
    ) { padding ->
        Column(
            modifier = Modifier
                .fillMaxSize()
                .padding(padding)
                .padding(20.dp)
                .verticalScroll(rememberScrollState()),
            verticalArrangement = Arrangement.spacedBy(16.dp),
        ) {
            StatusCard(engineState = engineState, onToggleAnc = onToggleAnc)

            if (!micPermissionGranted) {
                WarningCard(text = "Microphone permission is required for ANC. Open Settings to grant it.")
            }

            PresetCard(activePreset = activePreset, onClick = onOpenPresets)

            if (engineState is EngineState.Running) {
                DiagnosticsCard(diagnostics = diagnostics)
            }
        }
    }
}

@Composable
private fun StatusCard(engineState: EngineState, onToggleAnc: (Boolean) -> Unit) {
    val (label, color) = when (engineState) {
        is EngineState.Running -> "ANC ON" to AccentActive
        is EngineState.Starting -> "STARTING…" to AccentWarning
        is EngineState.Error -> "ERROR" to AccentError
        is EngineState.Stopped -> "ANC OFF" to AccentInactive
    }

    Card(
        modifier = Modifier.fillMaxWidth(),
        colors = CardDefaults.cardColors(containerColor = MaterialTheme.colorScheme.surface),
    ) {
        Column(modifier = Modifier.padding(24.dp), horizontalAlignment = Alignment.CenterHorizontally) {
            Spacer(modifier = Modifier.height(4.dp))
            Box(
                modifier = Modifier
                    .size(96.dp)
                    .clip(CircleShape)
                    .background(color.copy(alpha = 0.16f)),
                contentAlignment = Alignment.Center,
            ) {
                Box(
                    modifier = Modifier
                        .size(56.dp)
                        .clip(CircleShape)
                        .background(color),
                )
            }
            Spacer(modifier = Modifier.height(16.dp))
            Text(text = label, style = MaterialTheme.typography.headlineMedium, fontWeight = FontWeight.SemiBold)
            if (engineState is EngineState.Error) {
                Spacer(modifier = Modifier.height(4.dp))
                Text(
                    text = engineState.message,
                    style = MaterialTheme.typography.bodyMedium,
                    color = MaterialTheme.colorScheme.onSurfaceVariant,
                )
            }
            Spacer(modifier = Modifier.height(20.dp))
            Switch(
                checked = engineState is EngineState.Running || engineState is EngineState.Starting,
                onCheckedChange = onToggleAnc,
            )
        }
    }
}

@Composable
private fun WarningCard(text: String) {
    Card(
        modifier = Modifier.fillMaxWidth(),
        colors = CardDefaults.cardColors(containerColor = AccentWarning.copy(alpha = 0.14f)),
    ) {
        Row(modifier = Modifier.padding(16.dp), verticalAlignment = Alignment.CenterVertically) {
            Icon(Icons.Filled.Warning, contentDescription = null, tint = AccentWarning)
            Spacer(modifier = Modifier.width(12.dp))
            Text(text = text, style = MaterialTheme.typography.bodyMedium)
        }
    }
}

@Composable
private fun PresetCard(activePreset: AncPreset, onClick: () -> Unit) {
    Card(
        modifier = Modifier
            .fillMaxWidth()
            .clickableCard(onClick),
        colors = CardDefaults.cardColors(containerColor = MaterialTheme.colorScheme.surface),
    ) {
        Column(modifier = Modifier.padding(16.dp)) {
            Text(text = "Preset", style = MaterialTheme.typography.labelSmall, color = MaterialTheme.colorScheme.onSurfaceVariant)
            Spacer(modifier = Modifier.height(4.dp))
            Text(text = activePreset.displayName, style = MaterialTheme.typography.titleLarge)
            Spacer(modifier = Modifier.height(4.dp))
            Text(text = activePreset.description, style = MaterialTheme.typography.bodyMedium, color = MaterialTheme.colorScheme.onSurfaceVariant)
        }
    }
}

@Composable
private fun DiagnosticsCard(diagnostics: EngineDiagnostics) {
    Card(
        modifier = Modifier.fillMaxWidth(),
        colors = CardDefaults.cardColors(containerColor = MaterialTheme.colorScheme.surface),
    ) {
        Column(modifier = Modifier.padding(16.dp), verticalArrangement = Arrangement.spacedBy(6.dp)) {
            Text(text = "Diagnostics", style = MaterialTheme.typography.titleMedium)
            DiagnosticsRow("Input", "${diagnostics.inputSampleRateHz} Hz, burst ${diagnostics.inputFramesPerBurst}f, ${diagnostics.inputPerformanceMode}/${diagnostics.inputSharingMode}")
            DiagnosticsRow("Output", "${diagnostics.outputSampleRateHz} Hz, burst ${diagnostics.outputFramesPerBurst}f, ${diagnostics.outputPerformanceMode}/${diagnostics.outputSharingMode}")
            val latency = diagnostics.estimatedRoundTripLatencyMillis
            DiagnosticsRow(
                "Estimated pipeline latency",
                if (latency != null) "%.1f ms (measured stream latency; excludes acoustic path)".format(latency) else "not available on this stream",
            )
            DiagnosticsRow("Callback time", "${diagnostics.lastCallbackDurationUs} µs (max ${diagnostics.maxCallbackDurationUs} µs)")
            DiagnosticsRow("Driver xRuns", "in ${diagnostics.inputXRunCount} / out ${diagnostics.outputXRunCount}")
            DiagnosticsRow("Pipeline underrun frames", "${diagnostics.ringBufferUnderrunFrames}")
            DiagnosticsRow("Predictor error RMS", "%.4f (convergence indicator, not acoustic error)".format(diagnostics.predictionErrorRms))
        }
    }
}

@Composable
private fun DiagnosticsRow(label: String, value: String) {
    Row(modifier = Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.SpaceBetween) {
        Text(text = label, style = MaterialTheme.typography.bodyMedium, color = MaterialTheme.colorScheme.onSurfaceVariant)
    }
    Text(text = value, style = MaterialTheme.typography.bodyMedium)
}

private fun Modifier.clickableCard(onClick: () -> Unit): Modifier = this.clickable(onClick = onClick)
