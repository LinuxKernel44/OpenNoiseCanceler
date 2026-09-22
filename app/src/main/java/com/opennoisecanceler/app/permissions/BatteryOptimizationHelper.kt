package com.opennoisecanceler.app.permissions

import android.content.Context
import android.content.Intent
import android.net.Uri
import android.os.PowerManager
import android.provider.Settings

/**
 * Wraps the system "ignore battery optimizations" flow.
 *
 * ANC needs to keep its foreground service (and therefore the real-time
 * audio streams) alive continuously while the user is in another app.
 * Android's battery optimization ("Doze"/App Standby) can throttle or kill
 * background work for apps that are not exempted, which for this app means
 * ANC audibly cutting out. This is requested only as an explicit,
 * explained user choice from the in-app screen — never silently, and using
 * only the documented public API ([Settings.ACTION_REQUEST_IGNORE_BATTERY_OPTIMIZATIONS]).
 */
object BatteryOptimizationHelper {
    fun isIgnoringBatteryOptimizations(context: Context): Boolean {
        val powerManager = context.getSystemService(PowerManager::class.java) ?: return false
        return powerManager.isIgnoringBatteryOptimizations(context.packageName)
    }

    fun buildRequestIntent(context: Context): Intent {
        return Intent(
            Settings.ACTION_REQUEST_IGNORE_BATTERY_OPTIMIZATIONS,
            Uri.parse("package:${context.packageName}"),
        )
    }
}
