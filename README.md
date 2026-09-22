# OpenNoiseCanceler

A real-time, fully local active noise cancellation (ANC) prototype for
Android, built specifically for a **Samsung Galaxy S10e (Snapdragon 855,
Android 12)** paired with **wired Sennheiser CX 80S** headphones through the
3.5 mm jack.

All audio capture, processing, and playback happen entirely on-device.
There is no cloud processing, no network access, no remote servers, and no
telemetry.

## What this actually is (read before expecting "real ANC")

Genuine, effective active noise cancellation requires latency low enough
that an inverted copy of the noise arrives back at the ear still in phase
with the original — and an error microphone at the ear to continuously
verify and correct that. This device has neither a sub-millisecond audio
path nor an ear-reference microphone (the CX 80S are passive, and the only
microphone is on the phone body).

This project does not pretend otherwise. It implements the best
technically honest approach available on this hardware: a self-supervised
**adaptive linear predictor** that forecasts the low-frequency, slowly
varying part of the ambient noise far enough ahead to compensate for the
measured pipeline latency, inverts it, and plays it back — useful mainly
against steady low-frequency drone (fans, HVAC, engine noise), not speech,
transients, or general broadband noise.

The full reasoning, what was considered and rejected, and the honest
limitations are documented in [`Claude.md`](./Claude.md) and
[`docs/DSP_DESIGN.md`](./docs/DSP_DESIGN.md). Please read those before
filing an issue about cancellation quality.

## Status

Native DSP core and the full app (UI, foreground service, diagnostics,
presets, background/battery handling) are implemented and build a signed
release APK. See `Claude.md` for exactly what has and hasn't been verified
on physical hardware yet.

## Building

```
./gradlew assembleDebug      # debug APK
./gradlew testDebugUnitTest  # Kotlin unit tests
```

DSP unit tests run independently of Android/Gradle:

```
cmake -S native-test -B build-native-test
cmake --build build-native-test
./build-native-test/dsp_tests
```

See `Claude.md` for the full build/release/signing workflow.

## Hardware testing

See [`docs/HARDWARE_TESTING.md`](./docs/HARDWARE_TESTING.md), including how
to tell whether ANC is actually reducing noise rather than just changing
the perceived sound.

## License

MIT — see [`LICENSE`](./LICENSE).
