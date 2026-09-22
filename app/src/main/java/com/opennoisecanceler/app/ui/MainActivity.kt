package com.opennoisecanceler.app.ui

import android.Manifest
import android.content.Intent
import android.content.pm.PackageManager
import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.activity.result.contract.ActivityResultContracts
import androidx.activity.viewModels
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.core.content.ContextCompat
import com.opennoisecanceler.app.engine.EngineState
import com.opennoisecanceler.app.service.AncForegroundService
import com.opennoisecanceler.app.ui.theme.OpenNoiseCancelerTheme

sealed interface AppScreen {
    data object Home : AppScreen
    data object Presets : AppScreen
    data object Settings : AppScreen
}

class MainActivity : ComponentActivity() {
    private val viewModel: AppViewModel by viewModels()

    private var micPermissionGranted by mutableStateOf(false)

    private val requestMicPermission = registerForActivityResult(
        ActivityResultContracts.RequestPermission(),
    ) { granted ->
        micPermissionGranted = granted
        if (granted) startEngine()
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        micPermissionGranted = hasMicPermission()

        setContent {
            var screen by remember { mutableStateOf<AppScreen>(AppScreen.Home) }

            OpenNoiseCancelerTheme {
                val settings by viewModel.settings.collectAsState()
                val engineState by viewModel.engineState.collectAsState()
                val diagnostics by viewModel.engineDiagnostics.collectAsState()

                when (screen) {
                    AppScreen.Home -> HomeScreen(
                        engineState = engineState,
                        diagnostics = diagnostics,
                        activePreset = settings.effectivePreset,
                        micPermissionGranted = micPermissionGranted,
                        onToggleAnc = { desiredOn ->
                            if (desiredOn) {
                                if (micPermissionGranted) startEngine() else requestMicPermission.launch(Manifest.permission.RECORD_AUDIO)
                            } else {
                                stopEngine()
                            }
                        },
                        onOpenPresets = { screen = AppScreen.Presets },
                        onOpenSettings = { screen = AppScreen.Settings },
                    )

                    AppScreen.Presets -> PresetsScreen(
                        settings = settings,
                        onSelectPreset = { preset ->
                            viewModel.selectPreset(preset)
                            if (engineState is EngineState.Running) startEngineWithPreset(preset.id)
                        },
                        onStrengthChange = { viewModel.setStrengthOverride(it) },
                        onBack = { screen = AppScreen.Home },
                    )

                    AppScreen.Settings -> SettingsScreen(
                        micPermissionGranted = micPermissionGranted,
                        onRequestMicPermission = { requestMicPermission.launch(Manifest.permission.RECORD_AUDIO) },
                        onBack = { screen = AppScreen.Home },
                    )
                }
            }
        }
    }

    override fun onResume() {
        super.onResume()
        micPermissionGranted = hasMicPermission()
    }

    private fun hasMicPermission(): Boolean =
        ContextCompat.checkSelfPermission(this, Manifest.permission.RECORD_AUDIO) == PackageManager.PERMISSION_GRANTED

    private fun startEngine() {
        val presetId = viewModel.settings.value.selectedPresetId
        startEngineWithPreset(presetId)
    }

    private fun startEngineWithPreset(presetId: String) {
        val intent = Intent(this, AncForegroundService::class.java)
            .setAction(AncForegroundService.ACTION_START)
            .putExtra(AncForegroundService.EXTRA_PRESET_ID, presetId)
        ContextCompat.startForegroundService(this, intent)
    }

    private fun stopEngine() {
        val intent = Intent(this, AncForegroundService::class.java).setAction(AncForegroundService.ACTION_STOP)
        startService(intent)
    }
}
