package com.opennoisecanceler.app.config

import android.content.Context
import androidx.datastore.preferences.core.booleanPreferencesKey
import androidx.datastore.preferences.core.edit
import androidx.datastore.preferences.core.floatPreferencesKey
import androidx.datastore.preferences.core.stringPreferencesKey
import androidx.datastore.preferences.preferencesDataStore
import kotlinx.coroutines.flow.Flow
import kotlinx.coroutines.flow.map

private val Context.dataStore by preferencesDataStore(name = "onc_settings")

/** Persisted, non-real-time app configuration. Never touched from the audio callback path. */
data class AppSettings(
    val selectedPresetId: String = AncPreset.DEFAULT.id,
    /** Null means "use the selected preset's own strength value". */
    val strengthOverride: Float? = null,
    val hasAcknowledgedBatteryOptimizationExplainer: Boolean = false,
) {
    val selectedPreset: AncPreset
        get() = AncPreset.byId(selectedPresetId) ?: AncPreset.DEFAULT

    val effectivePreset: AncPreset
        get() {
            val base = selectedPreset
            val strength = strengthOverride
            return if (strength == null) base else base.copy(strength = strength.coerceIn(0f, 1f))
        }
}

/** Thin DataStore-backed persistence for [AppSettings]. */
class AppSettingsRepository(private val context: Context) {
    private object Keys {
        val PRESET_ID = stringPreferencesKey("selected_preset_id")
        val STRENGTH_OVERRIDE = floatPreferencesKey("strength_override")
        val BATTERY_EXPLAINER_ACK = booleanPreferencesKey("battery_explainer_acknowledged")
    }

    val settingsFlow: Flow<AppSettings> = context.dataStore.data.map { prefs ->
        AppSettings(
            selectedPresetId = prefs[Keys.PRESET_ID] ?: AncPreset.DEFAULT.id,
            strengthOverride = prefs[Keys.STRENGTH_OVERRIDE],
            hasAcknowledgedBatteryOptimizationExplainer = prefs[Keys.BATTERY_EXPLAINER_ACK] ?: false,
        )
    }

    suspend fun setSelectedPreset(presetId: String) {
        context.dataStore.edit { it[Keys.PRESET_ID] = presetId }
    }

    suspend fun setStrengthOverride(strength: Float?) {
        context.dataStore.edit { prefs ->
            if (strength == null) {
                prefs.remove(Keys.STRENGTH_OVERRIDE)
            } else {
                prefs[Keys.STRENGTH_OVERRIDE] = strength.coerceIn(0f, 1f)
            }
        }
    }

    suspend fun setBatteryOptimizationExplainerAcknowledged(acknowledged: Boolean) {
        context.dataStore.edit { it[Keys.BATTERY_EXPLAINER_ACK] = acknowledged }
    }
}
