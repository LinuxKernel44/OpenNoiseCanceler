package com.opennoisecanceler.app.service

import android.Manifest
import android.app.Service
import android.content.Intent
import android.content.pm.PackageManager
import android.media.AudioAttributes
import android.media.AudioFocusRequest
import android.media.AudioManager
import android.os.IBinder
import androidx.core.app.NotificationManagerCompat
import androidx.core.content.ContextCompat
import com.opennoisecanceler.app.config.AncPreset
import com.opennoisecanceler.app.engine.AncEngineController
import com.opennoisecanceler.app.engine.EngineState
import com.opennoisecanceler.app.engine.EngineStatusBus
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.Job
import kotlinx.coroutines.flow.drop
import kotlinx.coroutines.launch

/**
 * Foreground service that owns the ANC engine's lifecycle while it runs in
 * the background. Required so ANC keeps working while the user is in
 * another app (see AndroidManifest.xml's `foregroundServiceType="microphone"`).
 *
 * This service does not itself touch the real-time audio path — it only
 * starts/stops [AncEngineController], which in turn starts/stops the native
 * engine. All of that is ordinary (non-real-time) main-thread work.
 */
class AncForegroundService : Service() {
    private val serviceJob = Job()
    private val serviceScope = CoroutineScope(Dispatchers.Main.immediate + serviceJob)

    private lateinit var notificationHelper: NotificationHelper
    private lateinit var engineController: AncEngineController
    private var audioManager: AudioManager? = null
    private var audioFocusRequest: AudioFocusRequest? = null
    private var pausedByTransientFocusLoss = false
    private var activePreset: AncPreset = AncPreset.DEFAULT

    private val audioFocusListener = AudioManager.OnAudioFocusChangeListener { focusChange ->
        when (focusChange) {
            AudioManager.AUDIOFOCUS_LOSS -> {
                // A higher-priority audio use case has taken over permanently
                // (from this app's perspective) — stop rather than guess when
                // it is safe to resume.
                stopSelf()
            }
            AudioManager.AUDIOFOCUS_LOSS_TRANSIENT,
            AudioManager.AUDIOFOCUS_LOSS_TRANSIENT_CAN_DUCK -> {
                // A well-understood, temporary interruption (e.g. an incoming
                // call). Pause and plan to resume automatically on regain —
                // unlike a stream error/disconnect, this is not a safety
                // concern, it is exactly what this Android API is for.
                if (engineController.state.value is EngineState.Running) {
                    pausedByTransientFocusLoss = true
                    engineController.stop()
                    updateNotification()
                }
            }
            AudioManager.AUDIOFOCUS_GAIN -> {
                if (pausedByTransientFocusLoss) {
                    pausedByTransientFocusLoss = false
                    engineController.start(activePreset)
                }
            }
        }
    }

    override fun onCreate() {
        super.onCreate()
        notificationHelper = NotificationHelper(this)
        notificationHelper.ensureChannel()
        engineController = AncEngineController(serviceScope)
        audioManager = getSystemService(AudioManager::class.java)

        serviceScope.launch {
            engineController.diagnostics.collect { EngineStatusBus.publishDiagnostics(it) }
        }

        serviceScope.launch {
            // drop(1): a MutableStateFlow immediately replays its current
            // value (EngineState.Stopped, at this point — nothing has been
            // started yet) to a brand new collector. Without dropping that
            // initial replay, this collector would see "Stopped" and call
            // stopSelf() on every single onCreate(), before onStartCommand()
            // ever gets to call startForeground() — which is exactly what
            // produces ForegroundServiceDidNotStartInTimeException.
            engineController.state.drop(1).collect { state ->
                EngineStatusBus.publishState(state)
                updateNotification()
                if (state is EngineState.Error) {
                    // Surface the error via the notification briefly, then
                    // release the microphone/foreground status: silently
                    // retrying in the background is not appropriate for an
                    // audio pipeline that just failed.
                    releaseAudioFocus()
                    stopForeground(STOP_FOREGROUND_REMOVE)
                    stopSelf()
                } else if (state is EngineState.Stopped) {
                    releaseAudioFocus()
                    stopForeground(STOP_FOREGROUND_REMOVE)
                    stopSelf()
                }
            }
        }
    }

    override fun onStartCommand(intent: Intent?, flags: Int, startId: Int): Int {
        when (intent?.action) {
            ACTION_STOP -> {
                engineController.stop()
                return START_NOT_STICKY
            }
            else -> {
                val presetId = intent?.getStringExtra(EXTRA_PRESET_ID)
                activePreset = AncPreset.byId(presetId ?: "") ?: AncPreset.DEFAULT

                // Must call startForeground() promptly after
                // startForegroundService(); do this before any permission
                // check so the OS contract is honored even on the failure
                // path below.
                startForeground(NotificationHelper.NOTIFICATION_ID, notificationHelper.buildNotification(activePreset.displayName, isRunning = false))

                if (ContextCompat.checkSelfPermission(this, Manifest.permission.RECORD_AUDIO) != PackageManager.PERMISSION_GRANTED) {
                    stopForeground(STOP_FOREGROUND_REMOVE)
                    stopSelf()
                    return START_NOT_STICKY
                }

                requestAudioFocus()
                engineController.start(activePreset)
                return START_NOT_STICKY
            }
        }
    }

    override fun onDestroy() {
        engineController.stop()
        engineController.release()
        releaseAudioFocus()
        serviceJob.cancel()
        EngineStatusBus.publishState(EngineState.Stopped)
        super.onDestroy()
    }

    override fun onBind(intent: Intent?): IBinder? = null

    private fun requestAudioFocus() {
        val manager = audioManager ?: return
        val attributes = AudioAttributes.Builder()
            .setUsage(AudioAttributes.USAGE_ASSISTANCE_ACCESSIBILITY)
            .setContentType(AudioAttributes.CONTENT_TYPE_UNKNOWN)
            .build()
        val request = AudioFocusRequest.Builder(AudioManager.AUDIOFOCUS_GAIN)
            .setAudioAttributes(attributes)
            .setOnAudioFocusChangeListener(audioFocusListener)
            .build()
        audioFocusRequest = request
        manager.requestAudioFocus(request)
    }

    private fun releaseAudioFocus() {
        val manager = audioManager ?: return
        audioFocusRequest?.let { manager.abandonAudioFocusRequest(it) }
        audioFocusRequest = null
    }

    private fun updateNotification() {
        val isRunning = engineController.state.value is EngineState.Running
        val notification = notificationHelper.buildNotification(activePreset.displayName, isRunning)
        NotificationManagerCompat.from(this).notify(NotificationHelper.NOTIFICATION_ID, notification)
    }

    companion object {
        const val ACTION_START = "com.opennoisecanceler.app.action.START"
        const val ACTION_STOP = "com.opennoisecanceler.app.action.STOP"
        const val EXTRA_PRESET_ID = "preset_id"
    }
}
