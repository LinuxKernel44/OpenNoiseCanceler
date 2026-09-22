# DSP and ANC Algorithm Design

This document explains the reasoning behind the signal processing design in
`app/src/main/cpp/dsp/`. It describes what is actually implemented, why,
and — just as importantly — what it deliberately does *not* attempt to do
and why that would not work on this hardware.

## 1. What hardware we actually have

- **Reference microphone**: the Galaxy S10e's built-in microphone(s), on
  the phone body — not at the ear.
- **Output transducer**: a passive, wired Sennheiser CX 80S connected via
  the 3.5 mm jack. It has no microphone, no electronics, no active
  circuitry of any kind.
- **No error microphone.** There is no microphone anywhere near the ear
  canal that could measure the actual residual sound after cancellation.

This last point drives almost every design decision below.

## 2. Why classic FxLMS/closed-loop ANC does not apply here

Textbook adaptive ANC (LMS, NLMS, FxLMS) adapts a filter to minimize an
**error signal**: the output of an error microphone placed at or very near
the point you want quiet (the ear canal, in a headphone ANC product).
FxLMS specifically exists to correct for the "secondary path" `S(z)`
(speaker → acoustic path → error mic) by filtering the reference signal
with an estimate of `S(z)` before using it in the LMS weight update, so the
gradient direction is correct despite that path.

Both of these techniques fundamentally require measuring what actually
happened at the ear. **This hardware cannot measure that.** The phone's
microphone is on the phone body; it does not hear what the ear hears, and
it picks up essentially none of the sealed/near-ear output of in-ear
headphones. There is no signal anywhere in this system that represents
"how well did cancellation work."

Building a filter that is labeled "FxLMS" but is secretly adapting against
a signal that isn't the real error would be exactly the kind of dishonest
result this project explicitly must not produce. So instead of pretending
to have closed-loop error correction, the design below uses only what is
physically measurable.

## 3. What we can measure, and what we build from it

Even without an error microphone, two things are genuinely available:

1. **The reference signal itself, over time** (the ambient noise captured
   by the phone mic).
2. **The actual end-to-end pipeline latency**, measured from the audio
   stream APIs (see `AudioEngine::diagnosticsSnapshot()` /
   `oboe::AudioStream::calculateLatencyMillis()`), i.e. how many
   milliseconds elapse from a frame being captured to a frame reaching the
   output stream's playback point. This is a genuine measurement of the
   *electronic* pipeline (buffering, DSP, OS/driver, DAC/ADC); it does not
   include acoustic propagation time in the room, which cannot be measured
   without an ear-reference mic.

### 3.1 The adaptive predictor

`onc::dsp::AdaptivePredictor` (`adaptive_predictor.h/.cpp`) is an
**NLMS-based adaptive linear predictor**, not an error-driven adaptive
filter. At each sample `n` it:

1. Uses the filter's current weights to forecast sample `x[n]` from the
   window of samples ending `delaySamples` steps *before* `n` (i.e. a
   forecast made `delaySamples` samples ago, using only data available at
   that time).
2. Compares that forecast against the now-known real value of `x[n]` — a
   fully **self-supervised** error signal that requires no ear microphone
   at all, since it's checking the predictor's own past forecast against
   reality.
3. Updates the NLMS weights from that error.
4. Produces a **new** forecast, of `x[n + delaySamples]`, using the just
   updated weights and the most recent samples.

`delaySamples` is set to (and periodically re-derived from) the measured
pipeline latency (see `AncEngineController.maybeRecalibrateDelay` in the
Kotlin layer). The intent: if the predictor can forecast the reference
signal `delaySamples` samples into the future, then inverting that
forecast and sending it to the output now means the anti-noise signal
reaches the acoustic output at roughly the same time the corresponding
real noise (assuming it changes slowly enough to be predictable at all)
would otherwise arrive — compensating for the pipeline's own latency, at
least for the frequency content that is actually predictable that far
ahead.

This is a well-established technique for latency compensation in
predictive/feed-forward noise control (adaptive linear prediction / an
"adaptive line enhancer" applied to a noise-cancellation feed-forward
path), not something invented for this project — what's specific to this
project is using it *instead of* error-driven adaptation, because that's
what the hardware actually allows.

### 3.2 Why only low frequencies

A linear predictor can only forecast content that has enough
autocorrelation structure at the prediction horizon to be predictable at
all. For a periodic/narrowband signal, the higher its frequency relative
to the prediction horizon, the less predictable it becomes (a full or
multiple cycles of uncertainty accumulate). Practically:

- Real pipeline latency on this hardware, per Section 4, lands somewhere
  in the tens of milliseconds.
- At tens-of-milliseconds horizons, only slowly varying, low-frequency,
  fairly stationary content (engine/fan/HVAC drone, low rumble) is
  realistically predictable at all.

`AncProcessor` therefore band-limits the reference signal with a
high-pass (removes DC/rumble below ~20 Hz, which the predictor and
limiter should never have to deal with) and a low-pass (see each preset's
`lowPassHz`, 500–1000 Hz depending on preset) **before** the predictor ever
sees it. This is a deliberate bandwidth restriction, not an accident: it
keeps the predictor working only in the range where it has a realistic
chance of producing a useful forecast, and prevents it from trying (and
failing, adding noise of its own) to predict broadband/transient content
it fundamentally cannot forecast.

### 3.3 Strength is a manual control, not an auto-calibrated gain

Real ANC products calibrate anti-noise gain/phase against an error mic.
Without one, this app cannot determine "how loud should the anti-noise be
to actually cancel, rather than over- or under-shoot, at your ear, with
your specific ear/headphone seal." `strength` (0–1, per preset, with a
user override in Settings) is therefore an honest, manual trim — the user
listens and adjusts, rather than the app claiming a calibrated value it
has no way to derive.

## 4. Realistic latency and what it means for cancellation

End-to-end pipeline latency is the sum of: microphone ADC + input
buffering, the ring buffer between the two audio callbacks, DSP processing
time, output buffering, and DAC + analog output through the 3.5 mm jack —
plus, on top of all of that and *not* included in the measured figure,
acoustic propagation time from the phone body to the ear, which cannot be
measured on this hardware. Expect the measured (input + output) stream
latency reported by diagnostics to be on the order of several milliseconds
to a few tens of milliseconds depending on preset and whether the device
actually grants AAudio MMAP exclusive mode (see
`docs/HARDWARE_TESTING.md` for how to read this from the app and what it
means).

This rules out effective cancellation of anything except slowly varying,
low-frequency, spatially diffuse noise. It will not usefully cancel
speech, transient noise (footsteps, keyboard clicks, most traffic noise),
or generally anything with fast onsets or broadband content. This is a
physical consequence of the hardware and the API surface available on
Android without root/vendor access, not a bug to be fixed later — see
`Claude.md` for the full feasibility discussion.

## 5. Safety

Every stage that touches the signal sanitizes non-finite (NaN/Inf) values
and, since a single extreme-but-finite input sample was found (via the
native unit tests) to be able to leave the IIR band-limiting filters at an
enormous internal state for a very long time afterward, the top of
`AncProcessor::processBlock` also clamps input magnitude to a generous but
bounded range before it ever reaches those filters (see
`AncProcessor::kMaxInputMagnitude` and
`AdaptivePredictor::kMaxSampleMagnitude`, both documented in place). The
final `OutputLimiter` stage guarantees the returned sample is always
finite and within a configured ceiling (`outputCeiling`, ≤ 0.9 by
default), regardless of what happened upstream. This is exercised directly
by `native-test/test_anc_processor.cpp`'s pathological-input test, which
feeds huge values, NaN, and Inf through the full pipeline and asserts the
output never leaves that bound and that the engine recovers afterward.

## 6. Where this can evolve

`AncProcessor` is structured so the predictor/filter stage can be swapped
without touching the audio engine or JNI boundary: `AncParameters` and the
`processBlock` contract are the only coupling. If, in the future, a
different approach becomes viable — e.g. this app gains a companion
wearable with a true ear-reference microphone, enabling genuine
error-driven FxLMS — that would replace `AdaptivePredictor` behind the
same interface.
