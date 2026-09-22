package com.opennoisecanceler.app.ui

import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.Row
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.automirrored.filled.ArrowBack
import androidx.compose.material.icons.filled.Check
import androidx.compose.material3.Card
import androidx.compose.material3.CardDefaults
import androidx.compose.material3.ExperimentalMaterial3Api
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Scaffold
import androidx.compose.material3.Slider
import androidx.compose.material3.Text
import androidx.compose.material3.TopAppBar
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.dp
import com.opennoisecanceler.app.config.AncPreset
import com.opennoisecanceler.app.config.AppSettings

@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun PresetsScreen(
    settings: AppSettings,
    onSelectPreset: (AncPreset) -> Unit,
    onStrengthChange: (Float) -> Unit,
    onBack: () -> Unit,
) {
    Scaffold(
        topBar = {
            TopAppBar(
                title = { Text("Presets") },
                navigationIcon = {
                    IconButton(onClick = onBack) {
                        Icon(Icons.AutoMirrored.Filled.ArrowBack, contentDescription = "Back")
                    }
                },
            )
        },
    ) { padding ->
        Column(
            modifier = Modifier
                .fillMaxSize()
                .padding(padding)
                .padding(20.dp),
            verticalArrangement = Arrangement.spacedBy(12.dp),
        ) {
            AncPreset.ALL.forEach { preset ->
                PresetRow(
                    preset = preset,
                    selected = preset.id == settings.selectedPresetId,
                    onClick = { onSelectPreset(preset) },
                )
            }

            Text(
                text = "Strength",
                style = MaterialTheme.typography.titleMedium,
                modifier = Modifier.padding(top = 12.dp),
            )
            Text(
                text = "Trims how strongly the predicted anti-noise signal is applied for the " +
                    "current preset. There is no ear-reference microphone to auto-calibrate this " +
                    "against (see the project documentation), so it is a manual control: start low " +
                    "and increase only if it clearly helps.",
                style = MaterialTheme.typography.bodyMedium,
                color = MaterialTheme.colorScheme.onSurfaceVariant,
            )
            val strength = settings.effectivePreset.strength
            Slider(value = strength, onValueChange = onStrengthChange, valueRange = 0f..1f)
            Text(text = "%.0f%%".format(strength * 100), style = MaterialTheme.typography.bodyMedium)
        }
    }
}

@Composable
private fun PresetRow(preset: AncPreset, selected: Boolean, onClick: () -> Unit) {
    Card(
        modifier = Modifier
            .fillMaxWidth()
            .clickable(onClick = onClick),
        colors = CardDefaults.cardColors(
            containerColor = if (selected) MaterialTheme.colorScheme.primary.copy(alpha = 0.14f) else MaterialTheme.colorScheme.surface,
        ),
    ) {
        Column(modifier = Modifier.padding(16.dp)) {
            Row(
                modifier = Modifier.fillMaxWidth(),
                horizontalArrangement = Arrangement.SpaceBetween,
            ) {
                Text(text = preset.displayName, style = MaterialTheme.typography.titleLarge)
                if (selected) {
                    Icon(Icons.Filled.Check, contentDescription = "Selected", tint = MaterialTheme.colorScheme.primary)
                }
            }
            Text(
                text = preset.description,
                style = MaterialTheme.typography.bodyMedium,
                color = MaterialTheme.colorScheme.onSurfaceVariant,
            )
        }
    }
}
