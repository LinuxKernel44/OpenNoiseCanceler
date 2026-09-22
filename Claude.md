# Claude.md — OpenNoiseCanceler

This file is the primary technical reference for this repository, written
for both human maintainers and future Claude Code sessions working on it.
Everything here describes what is **actually implemented**, not what was
merely planned. Where something is a known gap, it says so explicitly.

## 1. Project purpose

OpenNoiseCanceler is a real-time, fully local active noise cancellation
(ANC) Android app, engineered for one specific, fixed hardware target
rather than as a general-purpose product:

- **Device**: Samsung Galaxy S10e, Snapdragon 855, Android 12, no root.
- **Headphones**: Sennheiser CX 80S, wired, passive (no microphone, no
  electronics), connected through the phone's 3.5 mm jack.
- **Constraint**: everything runs on-device. No cloud processing, no
  remote servers, no Bluetooth processing, no paid/proprietary APIs.

The goal is not to claim studio-grade ANC — it is to be honest about what
this specific hardware and the public Android audio APIs can and cannot
do, and to build the best technically valid noise-reduction system within
those limits. See §9 "Known limitations" before making any claims about
this app's effectiveness.

## 2. Supported device assumptions

`minSdk = targetSdk = 31` (Android 12) exactly, `compileSdk = 34` (see the
comment in `app/build.gradle.kts` for why compileSdk is higher than
target/min — it's a toolchain requirement of modern AndroidX/Compose
artifacts, not a behavior change; targetSdk is what governs runtime
behavior and stays pinned to match the device). Native builds only target
`arm64-v8a` (the Snapdragon 855 is 64-bit ARM only). The app assumes:

- A working microphone and a wired 3.5 mm output.
- Stereo wired headphone output, mono microphone input (both are
  requested explicitly; see `AudioEngine::openStreams`).
- Input and output streams negotiate the **same** native sample rate. If
  they don't, `AudioEngine::openStreams` fails on purpose rather than
  attempting on-the-fly resampling (out of scope; see §9).

This is a single-device engineering exercise. Code should not grow
speculative multi-device compatibility branches unless the project's scope
explicitly changes.

## 3. Architecture overview

```
app/src/main/java/com/opennoisecanceler/app/
  OpenNoiseCancelerApplication.kt   Application class
  ui/                               Compose UI (dark theme only), MainActivity, ViewModel
  config/                           AncPreset (DSP parameter sets), AppSettingsRepository (DataStore)
  engine/                           NativeAudioEngine (JNI wrapper), AncEngineController,
                                     EngineDiagnostics, EngineStatusBus (service ↔ UI bridge)
  service/                          AncForegroundService, NotificationHelper
  permissions/                      BatteryOptimizationHelper

app/src/main/cpp/
  dsp/                              Platform-independent DSP core (no Android/JNI/Oboe deps)
  audio_engine.h/.cpp               Oboe-based real-time I/O, owns the DSP pipeline
  jni_bridge.cpp                    Thin JNI boundary
  engine_diagnostics.h              Lock-free diagnostics counters shared with the RT thread
  CMakeLists.txt                    NDK build (links Oboe via prefab)

native-test/                        Host-only (no NDK/Android) unit tests for dsp/
app/src/test/                       JVM unit tests for pure-Kotlin logic (presets, diagnostics parsing)
docs/                                DSP_DESIGN.md (algorithm reasoning), HARDWARE_TESTING.md
```

**Key separation**: the DSP core (`app/src/main/cpp/dsp/`) has zero
dependencies on Android, JNI, or Oboe. It is compiled and unit tested
completely independently by `native-test/CMakeLists.txt` using the host's
own C++ toolchain — this is what makes "automated DSP tests that run
without Android hardware" (a project requirement) literally true rather
than aspirational. `audio_engine.cpp` is the only place that depends on
Oboe, and `jni_bridge.cpp` is the only place that depends on JNI.

The real-time audio callback path (`AudioEngine::onAudioReady` →
`AncProcessor::processBlock` → the DSP classes it owns) performs **no
allocation, no locking, no blocking I/O, no logging** — see the class
comments on `AudioEngine`, `AncProcessor`, `AdaptivePredictor`, and
`RingBuffer` for exactly how that's enforced (lock-free SPSC ring buffer
between the input and output callbacks, pre-allocated scratch buffers,
atomics for diagnostics/parameter reads). Reconfiguration (`configure()`)
is explicitly *not* real-time safe and stops/restarts the streams — this
is a deliberate tradeoff, documented at the call site.

## 4. Audio architecture (Android audio API research and choice)

Researched (see git history / this file's authorship) before
implementation: `AudioRecord`/`AudioTrack`, `AAudio`, Oboe, performance
modes, sharing modes, and MMAP.

**Decision: Oboe, requesting `PerformanceMode::LowLatency` +
`SharingMode::Exclusive` on both an input and an output stream**, native
C++ throughout the real-time path. Reasoning:

- AAudio's MMAP exclusive mode (available since Android 8.1, and what
  `SharingMode::Exclusive` + `PerformanceMode::LowLatency` requests) lets
  the app write/read directly into a buffer shared with the audio driver,
  bypassing the mixer — the lowest-latency path Android exposes without
  root or vendor-specific APIs. Oboe is Google's own wrapper that uses
  AAudio when available (API 27+) and falls back to OpenSL ES on older
  APIs; since `minSdk = 31`, AAudio is always available here, and Oboe is
  used for its stream-timestamp/latency helpers and because it's the
  documented, actively maintained way to reach this path rather than
  hand-rolling raw AAudio calls.
- The Snapdragon 855 is a 2019 flagship SoC; Samsung markets Snapdragon
  flagships of this era with Pro Audio / low-latency audio support, so
  MMAP exclusive mode is *expected* to be available — but this is
  reported, not assumed, at runtime via `EngineDiagnostics` (performance
  mode / sharing mode actually granted; see §7). If the device or OS
  falls back to `Shared` mode, the diagnostics show it honestly instead of
  claiming exclusive mode was achieved.
- Native C++ for the entire real-time path avoids JVM GC pauses,
  JIT/deopt jitter, and JNI call overhead inside the callback — the JNI
  boundary (`jni_bridge.cpp`) exists only for control calls
  (start/stop/configure/diagnostics), never inside the callback.
- Input requests `InputPreset::Unprocessed` specifically so the
  platform's own AGC/noise-suppressor/echo-canceller does not alter the
  reference signal before this app's own DSP ever sees it — and output
  uses `Usage::Media` (not `VoiceCommunication`) specifically to avoid the
  platform pairing the two streams into an implicit voice-call AEC path,
  which would actively fight this app's intentionally injected anti-noise
  signal.
- No fixed sample rate or buffer size is requested; both streams open at
  whatever the device's native rate/burst size is (queried, not assumed),
  and buffer size is then set to `2 × framesPerBurst` per Oboe's own
  documented low-latency guidance. See `AudioEngine::openStreams`.

Limitations this research surfaced and that the code/docs are explicit
about (see §9): no root/vendor API can bypass Android's audio stack
further than this; simultaneous mic input + wired output on this specific
device's actually-granted mode has **not yet been measured on physical
hardware** (see §11) — diagnostics exist specifically to report the real
numbers once it is.

## 5. DSP architecture and the ANC algorithm

Full reasoning lives in [`docs/DSP_DESIGN.md`](./docs/DSP_DESIGN.md) —
read it before touching `app/src/main/cpp/dsp/`. Summary:

- **No error microphone exists on this hardware** (phone-body mic only,
  passive headphones). This rules out genuine closed-loop FxLMS/adaptive
  ANC, which requires measuring the actual residual sound at the ear.
- Instead, `AdaptivePredictor` is a **self-supervised NLMS adaptive linear
  predictor**: it forecasts the reference signal a configurable number of
  samples ahead (matched to measured pipeline latency), trained by
  comparing its own past forecasts against the now-known real samples —
  no ear signal required, because it's validating its own prior
  prediction, not measuring cancellation.
- The signal is band-limited (high-pass ~20 Hz, low-pass 500–1000 Hz
  depending on preset) before the predictor, because only slowly varying,
  low-frequency content is realistically forecastable at the latencies
  this hardware achieves.
- `strength` (anti-noise gain) is an intentionally manual, user-facing
  control, not an auto-calibrated value — there's no way to calibrate
  gain/phase against the ear without an ear microphone.
- `OutputLimiter` plus per-stage NaN/Inf sanitization guarantee bounded,
  finite output regardless of upstream DSP state; a genuine bug found via
  the native unit tests (a single extreme-but-finite input sample could
  leave the band-limiting filters' IIR state enormous for a very long
  time) is fixed with an explicit input-magnitude clamp — see
  `docs/DSP_DESIGN.md` §5 and `native-test/test_anc_processor.cpp`.

`AncProcessor` is the composition point (`dsp/anc_processor.h/.cpp`); it's
structured so the predictor/filter stage can be swapped later behind the
same `AncParameters` / `processBlock` contract if the hardware situation
ever changes (e.g. a companion device with a real ear-reference mic).

## 6. Presets

`config/AncPreset.kt` defines four presets — Ultra Low Latency, Low
Latency, Balanced (default), High Quality — trading predictor filter
length (`predictorTaps`), lookback/delay window (`predictorDelaySamples`),
adaptation rate (`stepSize`/`leakage`), and band-limit cutoff
(`lowPassHz`) against CPU cost and adaptation smoothness. Field-for-field,
these map directly onto `onc::dsp::AncParameters`
(`app/src/main/cpp/dsp/anc_parameters.h`) and are passed across JNI as
primitives by `NativeAudioEngine`.

**These are starting points, not results of on-device tuning** — the
project's initial build session had no physical device available (see
§11). `AncPreset.kt`'s doc comment says this explicitly; update the preset
values and descriptions once real hardware numbers exist (see
`docs/HARDWARE_TESTING.md`).

`AncEngineController.maybeRecalibrateDelay` refines `predictorDelaySamples`
once, shortly after start, from the *actually measured* pipeline latency
(`EngineDiagnostics.estimatedRoundTripLatencyMillis`) rather than trusting
the preset's static guess — this is the one place a preset value is
automatically corrected against a real measurement.

## 7. Latency: measured vs. estimated vs. theoretical

`EngineDiagnostics` (Kotlin) / `EngineDiagnosticsSnapshot` (native) draw a
hard line between:

- **Measured**: sample rates, burst sizes, buffer sizes, actually-granted
  performance/sharing mode, driver xRun counts, our own callback wall-clock
  timing, our own ring-buffer underrun counter — all read directly from
  the running system, never hardcoded.
- **Measured-but-partial**: `outputLatencyMillis`/`inputLatencyMillis`,
  from Oboe's `calculateLatencyMillis()` (itself derived from the stream's
  real timestamp APIs) — genuine measurements of the *electronic*
  pipeline, but excluding acoustic propagation time, which cannot be
  measured on this hardware (no ear mic). `null` (not `0`, not a fake
  fallback) when unavailable — see `EngineDiagnostics.fromNativeArray`'s
  `latency()` helper and its unit test asserting the negative-sentinel →
  `null` conversion.
- **Not present in this codebase at all**: any claim of "theoretical"
  latency numbers presented as if measured. If you add one, label it
  explicitly as theoretical in both the UI string and any doc.

The UI (`HomeScreen.kt`'s `DiagnosticsCard`) labels the combined latency
figure "estimated" and states in-line what it excludes, rather than
presenting it as a precise round-trip measurement.

## 8. Foreground service, battery optimization, permissions

- **`AncForegroundService`** (`service/AncForegroundService.kt`) owns the
  `AncEngineController`/`NativeAudioEngine` lifecycle so ANC keeps running
  while the user is in another app. Declared with
  `android:foregroundServiceType="microphone"` (required alongside
  `RECORD_AUDIO` since Android 9+ for a foreground service that uses the
  mic). Publishes state/diagnostics to `EngineStatusBus`, a same-process
  singleton the UI observes (no AIDL/Messenger — service and UI share a
  process, so this is intentionally simple).
- **Notification**: persistent, `IMPORTANCE_LOW` (status indicator, not an
  alert — no sound), shows the active preset, includes a **Stop** action,
  built by `NotificationHelper`.
- **Audio focus**: requested with `USAGE_ASSISTANCE_ACCESSIBILITY` (this
  is a continuous accessibility-style signal, not music/media content).
  `AUDIOFOCUS_LOSS` (permanent, e.g. another app takes over) stops the
  service. `AUDIOFOCUS_LOSS_TRANSIENT[_CAN_DUCK]` (e.g. an incoming call)
  pauses and **auto-resumes on `AUDIOFOCUS_GAIN`** — this is the one place
  the app auto-restarts audio without a fresh user action, because it's
  exactly the well-defined, expected use of that specific Android API,
  unlike a hardware disconnect (see next point).
- **Device disconnect / stream errors**: `AudioEngine::onErrorAfterClose`
  deliberately does **not** auto-restart. Silently resuming anti-noise
  output through a route that just changed (e.g. headphones unplugged,
  something else now default) is treated as a safety concern, not an
  inconvenience — the engine stops, surfaces the error through
  `EngineState.Error`, the notification, and the home screen, and restart
  is an explicit user action.
- **Battery optimization**: `permissions/BatteryOptimizationHelper` wraps
  the standard, documented
  `Settings.ACTION_REQUEST_IGNORE_BATTERY_OPTIMIZATIONS` flow. Only
  reachable from an explicit in-app Settings screen with an explanation of
  why it matters for this specific app (foreground audio service getting
  throttled); never requested automatically, never bypassed by
  undocumented means.
- **Permissions requested, and only these**: `RECORD_AUDIO`,
  `FOREGROUND_SERVICE`, `FOREGROUND_SERVICE_MICROPHONE`,
  `MODIFY_AUDIO_SETTINGS`, `REQUEST_IGNORE_BATTERY_OPTIMIZATIONS`. No
  `INTERNET`, no storage, no location, no contacts, nothing beyond what
  the audio pipeline and the two flows above genuinely need. Verified
  absent from the built APK — see §12.

## 9. Known limitations (do not sell past these)

- **No true acoustic ANC.** No error microphone exists on this hardware;
  see §5 and `docs/DSP_DESIGN.md` for exactly why and what's built
  instead.
- **Effective range is low-frequency, slowly varying noise only** (fan/
  HVAC/engine drone). Speech, transients, and broadband noise are not
  meaningfully addressed by design, not by oversight.
- **No acoustic cancellation-quality numbers exist yet** — nothing in this
  repo should be read as claiming a specific dB reduction until
  `docs/HARDWARE_TESTING.md` §4.2 has actually been performed and results
  recorded.
- **Input/output sample-rate mismatch is not handled** — the engine
  refuses to start rather than resample; see §2.
- **No automatic restart after a stream error/device disconnect** — by
  design (§8), not a missing feature.
- **Presets are starting points, not tuned results** — see §6.
- **`isMinifyEnabled = false` on release builds for now** (see
  `app/build.gradle.kts`) — R8 shrinking/obfuscation is deferred until
  there's been an on-device test pass to validate it doesn't strip
  anything the JNI/reflection-adjacent paths need; `proguard-rules.pro`
  already has the JNI `keep` rules ready for when it's enabled.

## 10. Testing

- **Native DSP unit tests** (`native-test/`): built and run completely
  independently of Android/NDK/a connected device, using the host's own
  C++ toolchain and a small dependency-free test framework
  (`native-test/test_framework.h`). Covers: ring buffer correctness,
  biquad filter pass/stop-band behavior, adaptive predictor convergence
  and NaN/Inf/extreme-input robustness, output limiter bounds, and full
  `AncProcessor` pipeline safety (including the pathological-input
  recovery test that caught the IIR-state bug in §5/§9).
  ```
  cmake -S native-test -B build-native-test
  cmake --build build-native-test
  ./build-native-test/dsp_tests
  ```
- **Kotlin JVM unit tests** (`app/src/test/`): pure-logic coverage for
  `AncPreset`/`AppSettings` validation and `EngineDiagnostics` parsing
  (including the negative-latency-sentinel → `null` contract). Run via
  `./gradlew testDebugUnitTest`.
- **Not yet covered by automated tests** (documented gap, not an
  oversight): `AncEngineController`/`NativeAudioEngine`/`AudioEngine`
  itself, since they load a native library and open real audio streams —
  exercising them requires either a connected device/emulator with audio
  support or refactoring `NativeAudioEngine` behind an interface to allow
  a fake implementation in JVM tests. If you pick this up, prefer the
  interface-extraction approach so real JNI behavior stays covered by
  on-device testing rather than a mock hiding real integration bugs.
- **Hardware test procedure**: [`docs/HARDWARE_TESTING.md`](./docs/HARDWARE_TESTING.md),
  including specifically how to tell whether ANC is reducing real ambient
  noise versus merely changing the perceived sound (§4 there).

## 11. Build, verification, and session status

Everything below was actually run, not just written:

- `./gradlew assembleDebug` — **succeeds** (native code compiles via
  CMake/NDK 27.0.12077973 for `arm64-v8a`, links Oboe via prefab).
- `./gradlew testDebugUnitTest` — **succeeds**, 19 tests.
- `native-test` DSP suite — **succeeds**, 27 tests (after fixing one real
  member-initialization-order bug and one real IIR-state-overflow
  robustness bug found by these tests during development — see git
  history and `docs/DSP_DESIGN.md` §5).
- `./gradlew assembleRelease` — **succeeds**, produces a signed APK
  (signature verified with `apksigner verify`; see §12/§13).
- **Not yet done**: any on-device installation, run, or measurement on the
  actual Galaxy S10e + CX 80S (no physical device was connected during
  this build session). Every latency/performance-mode/cancellation-quality
  number in this repo is either a design target or explicitly marked as
  not yet measured. **Do not** change any of this file's or
  `docs/DSP_DESIGN.md`'s wording to imply on-device verification happened
  until `docs/HARDWARE_TESTING.md` has actually been carried out — update
  this section with the date and results when it has.

## 12. Security rules for this repository

- **Never commit**: `keystore.properties`, `keystore/` (or any `.jks`/
  `.keystore` file), `local.properties`, build output directories. All are
  in `.gitignore`; verify `git status --ignored` before every push if you
  touch signing config.
- The release keystore is generated locally (PKCS12, RSA 4096, random
  32-character password shared for both store and key password — PKCS12
  keystores don't support separate ones, see `keystore.properties`'s
  layout) and referenced only via `keystore.properties`, read by
  `app/build.gradle.kts` and never logged.
- Before any push: `git status`, review the diff, re-run the secret scan
  pattern used during this project's initial setup (grep the built APK's
  dex/resources for `password`/`secret`/`api_key`/private-key markers,
  and grep for the literal store password and for local absolute paths
  like `/home/<user>/...`), and confirm `.gitignore` still covers
  everything above.
- Do not commit `~/archlinuxContext.md` or any other file from outside
  this repository.

## 13. Release process

1. `./gradlew testDebugUnitTest` and the `native-test` suite both green.
2. `./gradlew assembleRelease`.
3. `apksigner verify -v --print-certs app/build/outputs/apk/release/app-release.apk`
   — confirm it verifies and the certificate fingerprint matches the
   expected release keystore.
4. `aapt dump badging` the APK — confirm package name, versionCode/Name,
   permissions list, and min/target SDK match what's expected (see §8 for
   the exact permission list).
5. Secret-scan the APK (see §12) and the repo working tree.
6. `git status`/diff review, commit.
7. `gh repo create LinuxKernel44/OpenNoiseCanceler --public` (only once,
   first release) and `git push`.
8. `gh release create <tag> app/build/outputs/apk/release/app-release.apk`
   with release notes that state the version, what changed, and repeat the
   §9 known-limitations summary — never omit it from release notes to make
   the release look more capable than it is.

## 14. Development workflow / GitHub workflow

- Small, meaningful commits; commit messages explain *why*, not just
  *what* (see repo git log for the established style).
- This is a single-maintainer, single-device-target project — do not
  introduce multi-device abstraction, CI pipelines, or speculative
  features unless the project's actual scope changes; keep changes
  focused on the stated core purpose (§1).
- Before pushing: rebuild (`assembleDebug` + both test suites), re-run the
  §12 secret scan, review `git diff`.

## 15. Instructions for future Claude Code sessions

- **Read `docs/DSP_DESIGN.md` before changing anything under
  `app/src/main/cpp/dsp/`** — the "why not classic FxLMS" reasoning is not
  obvious from the code alone and getting it wrong risks reintroducing a
  design that silently can't work on this hardware.
- **Never present an estimate as a measurement.** If you add a new
  diagnostic, decide explicitly which bucket (§7) it belongs to and label
  it accordingly in both the native struct comment and the Kotlin/UI
  layer.
- **The real-time audio callback path must stay allocation/lock/log-free.**
  If you need to touch `AudioEngine::handleInputCallback`/
  `handleOutputCallback` or anything in `dsp/`, check first whether your
  change introduces a `new`/`std::vector` resize/mutex/`printf` inside the
  hot path — if so, pre-allocate or move it to the control path instead
  (see how `AdaptivePredictor`'s `scratchPast_`/`scratchNow_` avoid this).
- **Re-run the native-test suite after any `dsp/` change** — it caught two
  real bugs during initial development (see §11) and is fast (a few
  seconds) since it needs no emulator/device.
- **When physical hardware becomes available**, work through
  `docs/HARDWARE_TESTING.md`, then update: preset values/descriptions in
  `config/AncPreset.kt` if they need retuning, §6/§9/§11 of this file with
  real numbers and the date tested, and the release notes of the next
  version.
- **Do not weaken the honesty commitments** in §1/§5/§9 to make the
  project sound more capable — this is a stated, explicit project
  requirement, not house style that can be relaxed for a cleaner-sounding
  README.
