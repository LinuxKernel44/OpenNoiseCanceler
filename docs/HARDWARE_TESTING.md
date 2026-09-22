# Hardware Testing Procedure (Galaxy S10e + Sennheiser CX 80S)

This document describes how to test OpenNoiseCanceler on the actual
target hardware. It was written before on-device testing was performed as
part of this project's initial build (no physical device was available
during that session — see `Claude.md` for status) — treat every numeric
example below as illustrative of *how to read the diagnostics*, not as a
reported measurement, until you have actually run these steps and filled
in real numbers.

## 0. Prerequisites

- Galaxy S10e, Android 12, USB debugging enabled (Settings → Developer
  options).
- Sennheiser CX 80S plugged into the 3.5 mm jack.
- `adb` available on the host machine, device authorized (`adb devices`
  shows it as `device`, not `unauthorized`).
- A signed release APK (see `Claude.md` → Release process) or a debug
  build installed via `./gradlew installDebug`.

## 1. Install and grant permissions

```
adb install -r app-release.apk
```

Launch the app. Grant the microphone permission when prompted (or via
Settings → Microphone permission → Grant permission in-app). Optionally
exempt the app from battery optimization from the in-app Settings screen
if you intend to test background operation.

## 2. Confirm the engine actually starts

Toggle ANC on from the home screen. Within roughly a second it should
read **ANC ON** rather than **STARTING…** or **ERROR**. If it reports an
error, check `adb logcat` filtered to the app's process for the Oboe/AAudio
error path, and confirm nothing else is holding an exclusive audio stream
(e.g. another app actively recording).

## 3. Read the diagnostics panel

With ANC on, the home screen's diagnostics card shows, per stream:

- **Sample rate** and **burst size**: the values the device actually
  negotiated. Compare against `adb shell dumpsys media.audio_flinger` or
  `getprop | grep audio` if you want to cross-check against the platform's
  reported native rate.
- **Performance mode / sharing mode**: this is the most important field to
  check first. `LOW_LATENCY/EXCLUSIVE` on both streams means the device
  granted AAudio MMAP exclusive mode — the best-case low-latency path this
  app requests. `LOW_LATENCY/SHARED` or `NONE/SHARED` means it fell back;
  latency will be measurably higher. This is a genuine report of what the
  driver granted, not a request echoed back.
- **Estimated pipeline latency**: sum of `calculateLatencyMillis()` from
  both streams. This is a *measurement* from the audio stream's own
  timestamp APIs, not a theoretical constant — but it explicitly excludes
  acoustic propagation time from the phone to your ear, which this
  hardware cannot measure (no ear-reference mic; see
  `docs/DSP_DESIGN.md`). Treat it as a lower bound on true round-trip
  latency, not the whole story.
- **Callback time / max callback time**: wall-clock microseconds spent
  inside the native audio callback. This should stay comfortably under
  the callback period implied by the burst size and sample rate (e.g. at
  48 kHz with a 192-frame burst, the period is 4 ms — callback time should
  be a small fraction of that). If callback time approaches or exceeds the
  period, expect audible glitches.
- **Driver xRuns / pipeline underrun frames**: both should stay at 0 (or
  very close to it, and not climbing) during steady playback. Rising
  numbers mean the device cannot keep up with the requested configuration
  — try a lower-CPU preset (Ultra Low Latency or Low Latency).
- **Predictor error RMS**: a convergence indicator for the adaptive
  predictor (see `docs/DSP_DESIGN.md` §3.1), not a measurement of acoustic
  cancellation. It should trend downward over the first second or two
  after starting in a stable noise environment (e.g. sitting near a fan)
  and rise again if the environment changes abruptly.

Record these numbers per preset — they are the actual, hardware-verified
latency/stability characteristics referenced (but not yet measured) by
`config/AncPreset.kt`'s preset descriptions.

## 4. Determine whether ANC is actually reducing noise, not just changing it

This is the important, easy-to-get-wrong part. Adaptive filtering can
audibly change what you hear (adding a low hum, coloring the sound)
without actually reducing the energy of the real ambient noise reaching
your ear. Do **not** rely on subjective "it sounds different, so it must
be working" — a genuinely honest test needs a way to separate
"perceived change" from "actual reduction."

### 4.1 Minimum bar: A/B by ear, controlled

1. Sit in a location with a steady, low-frequency noise source (a fan,
   an HVAC vent, a running appliance, idling car engine, etc. — exactly
   the kind of noise this design targets per `docs/DSP_DESIGN.md`).
2. With the headphones on and ANC **off**, note the perceived loudness of
   the noise.
3. Toggle ANC **on**, wait several seconds for the predictor to converge
   (watch the predictor error RMS diagnostic settle), and compare.
4. Toggle off and on again a few times to rule out habituation bias. Try
   this blind if possible (someone else toggles it without telling you
   when).

This tells you about *perceived* change only, and is influenced by the
sound simply being altered (e.g. slightly muffled) rather than reduced.

### 4.2 A real reduction measurement: external reference microphone

To actually distinguish "quieter" from "different," you need a microphone
*at the ear position*, independent of the phone:

1. Use a second phone, a dedicated SPL meter app, or a measurement
   microphone, positioned as close as practical to the ear cup/tip of the
   CX 80S while it is worn (or, more reproducibly, a fixed mannequin/ear
   simulator position if available — a rolled-up cloth or foam block
   holding the earbud in a fixed position works as an improvised fixture).
2. Record the ambient noise level for a fixed interval with ANC off, then
   the same interval with ANC on, without moving the reference microphone
   or the noise source between the two recordings.
3. Compare either broadband SPL (dB) or, better, a spectrum (many SPL
   meter apps can export a FFT/octave-band view) — since this design only
   targets low frequencies (see `docs/DSP_DESIGN.md` §3.2), look
   specifically at whether energy *below* the active preset's
   `lowPassHz` dropped, versus energy above it (which should be
   unaffected either way — if it changed too, something other than this
   app's DSP is responsible, e.g. the headphones' own passive isolation
   changing because of how they were seated).
4. Repeat for each preset you plan to ship/recommend, and log the
   external-mic numbers next to the in-app diagnostics from Step 3 so
   future readers can correlate measured pipeline latency with measured
   acoustic reduction.

A meaningful reduction shows up as a measurable dB drop in the targeted
low-frequency band on the *external, ear-position* microphone — not on
the phone's own microphone (which is upstream of any cancellation effect
and mostly measures the room, not what reaches the ear), and not merely a
change in what a listener subjectively reports.

### 4.3 Sanity checks that should always hold

- With **no headphones connected** (or the wrong output route active),
  toggling ANC on should either fail to start or should not claim to be
  doing anything useful — check that the diagnostics panel's stream info
  reflects the actual active output route.
- Unplugging the headphones while ANC is running should stop the engine
  (see `AudioEngine::onErrorAfterClose`) rather than silently continuing
  to try to output to a route that no longer exists, and should not
  auto-resume without the user explicitly restarting it, even if
  something else is plugged in shortly after — confirm this by watching
  the app move to the ERROR state and the persistent notification update
  accordingly.
- At no point should the output be uncomfortably loud or contain audible
  clicks/pops suggesting clipping or NaN/Inf leakage; the native unit
  tests (`native-test/`) already cover this at the DSP level with
  synthetic pathological input, but a real-ear listen with a conservative
  volume the first time you test any new preset or configuration change
  is still good practice.

## 5. Background operation

1. Start ANC, then switch to another app (e.g. open a web browser) and
   use the phone normally for a few minutes.
2. Confirm the persistent notification remains, correctly shows the
   active preset, and its **Stop** action actually stops the engine.
3. If you exempted the app from battery optimization (Settings screen in
   the app), confirm ANC keeps running over a longer background period
   (tens of minutes); if you did not, this is a reasonable point to
   observe whether/when Android throttles it, and note the behavior here.

## 6. Recording results

There is no automated harness for the acoustic measurements above (a
genuine acoustic reduction measurement requires physical setup a CI
runner cannot replicate). When you complete a hardware test pass, record:
device build (Android/security patch date), preset tested, the full
diagnostics panel readout, and — if you did the external-mic measurement
in §4.2 — the measured dB reduction and frequency band. Update
`config/AncPreset.kt`'s preset descriptions and `Claude.md`'s "Known
Limitations" section if real numbers meaningfully change what should be
claimed about any preset.
