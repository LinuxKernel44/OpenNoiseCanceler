package com.opennoisecanceler.app.ui

import android.content.Intent
import android.provider.Settings
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.automirrored.filled.ArrowBack
import androidx.compose.material3.Button
import androidx.compose.material3.Card
import androidx.compose.material3.CardDefaults
import androidx.compose.material3.ExperimentalMaterial3Api
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.Scaffold
import androidx.compose.material3.Text
import androidx.compose.material3.TopAppBar
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.unit.dp
import com.opennoisecanceler.app.BuildConfig
import com.opennoisecanceler.app.permissions.BatteryOptimizationHelper

@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun SettingsScreen(
    micPermissionGranted: Boolean,
    onRequestMicPermission: () -> Unit,
    onBack: () -> Unit,
) {
    val context = LocalContext.current
    var ignoringBatteryOptimizations by remember {
        mutableStateOf(BatteryOptimizationHelper.isIgnoringBatteryOptimizations(context))
    }

    Scaffold(
        topBar = {
            TopAppBar(
                title = { Text("Settings") },
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
            verticalArrangement = Arrangement.spacedBy(16.dp),
        ) {
            SettingsSection(
                title = "Microphone permission",
                body = "OpenNoiseCanceler needs microphone access to capture ambient noise. Audio " +
                    "is processed entirely on this device and is never recorded to a file or sent " +
                    "anywhere.",
            ) {
                if (micPermissionGranted) {
                    Text("Granted", color = MaterialTheme.colorScheme.primary)
                } else {
                    Button(onClick = onRequestMicPermission) { Text("Grant permission") }
                }
            }

            SettingsSection(
                title = "Background operation",
                body = "ANC runs as a foreground service so it keeps working while you use other " +
                    "apps. Android's battery optimization can throttle background work and cause " +
                    "ANC to cut out; exempting this app keeps it running reliably. This is optional " +
                    "and only takes effect if you approve it in the next screen.",
            ) {
                if (ignoringBatteryOptimizations) {
                    Text("Exempted", color = MaterialTheme.colorScheme.primary)
                } else {
                    Button(onClick = {
                        context.startActivity(BatteryOptimizationHelper.buildRequestIntent(context))
                    }) { Text("Exempt from battery optimization") }
                }
            }

            SettingsSection(
                title = "Notification access",
                body = "A persistent, low-priority notification is shown whenever ANC is active, " +
                    "with a Stop action, as required by Android for foreground audio services.",
            ) {
                OutlinedButton(onClick = {
                    context.startActivity(
                        Intent(Settings.ACTION_APP_NOTIFICATION_SETTINGS)
                            .putExtra(Settings.EXTRA_APP_PACKAGE, context.packageName),
                    )
                }) { Text("Open notification settings") }
            }

            SettingsSection(
                title = "About",
                body = "OpenNoiseCanceler ${BuildConfig.VERSION_NAME}. Runs entirely on-device: no " +
                    "cloud processing, no network access, no analytics. See the project's Claude.md " +
                    "and docs/ for the full technical design, known limitations, and hardware test " +
                    "procedure.",
            ) {}
        }
    }
}

@Composable
private fun SettingsSection(title: String, body: String, action: @Composable () -> Unit) {
    Card(
        modifier = Modifier.fillMaxWidth(),
        colors = CardDefaults.cardColors(containerColor = MaterialTheme.colorScheme.surface),
    ) {
        Column(modifier = Modifier.padding(16.dp), verticalArrangement = Arrangement.spacedBy(8.dp)) {
            Text(text = title, style = MaterialTheme.typography.titleMedium)
            Text(text = body, style = MaterialTheme.typography.bodyMedium, color = MaterialTheme.colorScheme.onSurfaceVariant)
            action()
        }
    }
}
