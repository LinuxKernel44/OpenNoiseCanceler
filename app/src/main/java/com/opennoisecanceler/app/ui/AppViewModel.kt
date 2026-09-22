package com.opennoisecanceler.app.ui

import android.app.Application
import androidx.lifecycle.AndroidViewModel
import androidx.lifecycle.viewModelScope
import com.opennoisecanceler.app.config.AncPreset
import com.opennoisecanceler.app.config.AppSettings
import com.opennoisecanceler.app.config.AppSettingsRepository
import com.opennoisecanceler.app.engine.EngineDiagnostics
import com.opennoisecanceler.app.engine.EngineState
import com.opennoisecanceler.app.engine.EngineStatusBus
import kotlinx.coroutines.flow.SharingStarted
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.stateIn
import kotlinx.coroutines.launch

/**
 * UI-facing view model. Holds no audio engine state of its own — engine
 * state/diagnostics are observed from [EngineStatusBus], which the running
 * [com.opennoisecanceler.app.service.AncForegroundService] publishes to.
 * This view model only starts/stops that service and persists settings.
 */
class AppViewModel(application: Application) : AndroidViewModel(application) {
    private val settingsRepository = AppSettingsRepository(application)

    val settings: StateFlow<AppSettings> = settingsRepository.settingsFlow
        .stateIn(viewModelScope, SharingStarted.WhileSubscribed(5000), AppSettings())

    val engineState: StateFlow<EngineState> = EngineStatusBus.state
    val engineDiagnostics: StateFlow<EngineDiagnostics> = EngineStatusBus.diagnostics

    fun selectPreset(preset: AncPreset) {
        viewModelScope.launch {
            settingsRepository.setSelectedPreset(preset.id)
            settingsRepository.setStrengthOverride(null)
        }
    }

    fun setStrengthOverride(strength: Float) {
        viewModelScope.launch { settingsRepository.setStrengthOverride(strength) }
    }

    fun acknowledgeBatteryOptimizationExplainer() {
        viewModelScope.launch { settingsRepository.setBatteryOptimizationExplainerAcknowledged(true) }
    }
}
