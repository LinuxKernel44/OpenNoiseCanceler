package com.opennoisecanceler.app.engine

import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow

/**
 * Process-wide, in-memory bridge between [com.opennoisecanceler.app.service.AncForegroundService]
 * (the single owner of the real [AncEngineController]) and the UI layer,
 * which only ever observes these flows — it never creates its own engine
 * instance. Since the service and the UI run in the same app process,
 * a simple shared singleton is sufficient; there is no cross-process IPC
 * here.
 */
object EngineStatusBus {
    private val _state = MutableStateFlow<EngineState>(EngineState.Stopped)
    val state: StateFlow<EngineState> = _state.asStateFlow()

    private val _diagnostics = MutableStateFlow(EngineDiagnostics.EMPTY)
    val diagnostics: StateFlow<EngineDiagnostics> = _diagnostics.asStateFlow()

    fun publishState(state: EngineState) {
        _state.value = state
    }

    fun publishDiagnostics(diagnostics: EngineDiagnostics) {
        _diagnostics.value = diagnostics
    }
}
