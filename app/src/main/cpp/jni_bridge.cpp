// JNI boundary between Kotlin (engine/NativeAudioEngine.kt) and the native
// AudioEngine. Kept deliberately thin: all real logic lives in AudioEngine
// and the dsp/ classes so it can be unit tested without JNI/Android at all
// (see native-test/).
//
// Diagnostics are marshalled as a fixed-layout DoubleArray rather than a
// JNI-constructed Kotlin object to avoid repeated FindClass/GetMethodID
// lookups on a path that, while not real-time itself, is polled
// periodically. The index layout below MUST stay in sync with
// EngineDiagnostics.fromNativeArray() in NativeAudioEngine.kt.

#include <jni.h>

#include <memory>

#include "audio_engine.h"
#include "dsp/anc_parameters.h"

namespace {

onc::AudioEngine* toEngine(jlong handle) {
    return reinterpret_cast<onc::AudioEngine*>(static_cast<intptr_t>(handle));
}

constexpr int kDiagnosticsArraySize = 22;

} // namespace

extern "C" {

JNIEXPORT jlong JNICALL
Java_com_opennoisecanceler_app_engine_NativeAudioEngine_nativeCreate(JNIEnv*, jobject) {
    auto* engine = new onc::AudioEngine();
    return static_cast<jlong>(reinterpret_cast<intptr_t>(engine));
}

JNIEXPORT void JNICALL
Java_com_opennoisecanceler_app_engine_NativeAudioEngine_nativeDestroy(JNIEnv*, jobject, jlong handle) {
    delete toEngine(handle);
}

JNIEXPORT jboolean JNICALL
Java_com_opennoisecanceler_app_engine_NativeAudioEngine_nativeStart(
    JNIEnv*, jobject, jlong handle,
    jint predictorTaps, jint predictorDelaySamples, jfloat stepSize, jfloat leakage,
    jfloat highPassHz, jfloat lowPassHz, jfloat filterQ, jfloat strength, jfloat outputCeiling) {
    auto* engine = toEngine(handle);
    if (engine == nullptr) return JNI_FALSE;

    onc::dsp::AncParameters params;
    // sampleRateHz is filled in from the actually negotiated stream rate
    // inside AudioEngine::openStreams(); the value here is only a
    // placeholder used for the initial isValid() check.
    params.sampleRateHz = 48000.0f;
    params.predictorTaps = static_cast<size_t>(predictorTaps);
    params.predictorDelaySamples = static_cast<size_t>(predictorDelaySamples);
    params.predictorStepSize = stepSize;
    params.predictorLeakage = leakage;
    params.highPassCutoffHz = highPassHz;
    params.lowPassCutoffHz = lowPassHz;
    params.filterQ = filterQ;
    params.strength = strength;
    params.outputCeiling = outputCeiling;

    return engine->start(params) ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT void JNICALL
Java_com_opennoisecanceler_app_engine_NativeAudioEngine_nativeStop(JNIEnv*, jobject, jlong handle) {
    auto* engine = toEngine(handle);
    if (engine != nullptr) engine->stop();
}

JNIEXPORT jboolean JNICALL
Java_com_opennoisecanceler_app_engine_NativeAudioEngine_nativeConfigure(
    JNIEnv*, jobject, jlong handle,
    jint predictorTaps, jint predictorDelaySamples, jfloat stepSize, jfloat leakage,
    jfloat highPassHz, jfloat lowPassHz, jfloat filterQ, jfloat strength, jfloat outputCeiling) {
    auto* engine = toEngine(handle);
    if (engine == nullptr) return JNI_FALSE;

    onc::dsp::AncParameters params;
    params.sampleRateHz = 48000.0f;
    params.predictorTaps = static_cast<size_t>(predictorTaps);
    params.predictorDelaySamples = static_cast<size_t>(predictorDelaySamples);
    params.predictorStepSize = stepSize;
    params.predictorLeakage = leakage;
    params.highPassCutoffHz = highPassHz;
    params.lowPassCutoffHz = lowPassHz;
    params.filterQ = filterQ;
    params.strength = strength;
    params.outputCeiling = outputCeiling;

    return engine->configure(params) ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jboolean JNICALL
Java_com_opennoisecanceler_app_engine_NativeAudioEngine_nativeIsRunning(JNIEnv*, jobject, jlong handle) {
    auto* engine = toEngine(handle);
    return (engine != nullptr && engine->isRunning()) ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jdoubleArray JNICALL
Java_com_opennoisecanceler_app_engine_NativeAudioEngine_nativeGetDiagnostics(JNIEnv* env, jobject, jlong handle) {
    jdoubleArray result = env->NewDoubleArray(kDiagnosticsArraySize);
    if (result == nullptr) return nullptr;

    auto* engine = toEngine(handle);
    double values[kDiagnosticsArraySize] = {0};

    if (engine != nullptr) {
        const onc::EngineDiagnosticsSnapshot snap = engine->diagnosticsSnapshot();
        values[0] = snap.running ? 1.0 : 0.0;
        values[1] = snap.inputSampleRateHz;
        values[2] = snap.outputSampleRateHz;
        values[3] = snap.inputFramesPerBurst;
        values[4] = snap.outputFramesPerBurst;
        values[5] = snap.inputBufferCapacityFrames;
        values[6] = snap.outputBufferCapacityFrames;
        values[7] = snap.inputBufferSizeFrames;
        values[8] = snap.outputBufferSizeFrames;
        values[9] = snap.inputPerformanceMode;
        values[10] = snap.outputPerformanceMode;
        values[11] = snap.inputSharingMode;
        values[12] = snap.outputSharingMode;
        values[13] = snap.outputLatencyMillis;
        values[14] = snap.inputLatencyMillis;
        values[15] = snap.inputXRunCount;
        values[16] = snap.outputXRunCount;
        values[17] = snap.lastCallbackDurationUs;
        values[18] = snap.maxCallbackDurationUs;
        values[19] = static_cast<double>(snap.ringBufferUnderrunFrames);
        values[20] = snap.predictionErrorRms;
        values[21] = snap.lastErrorCode;
    }

    env->SetDoubleArrayRegion(result, 0, kDiagnosticsArraySize, values);
    return result;
}

} // extern "C"
