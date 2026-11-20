# Deuce v2.5 (Native Trial) — Skell Preset

**Status:** implementation candidate only; not a verified transfer of the
owner-approved 15 s lossless listening sound, not a production preset.

## Selected offline research candidate

The owner preferred the **Deuce v2.5 Presence Repair** lossless A/B version
over the earlier candidate on 2026-11-15 (America/New_York). The offline v2.5
signal path was **v2.3 Sustained Air** plus a causal RBJ peaking EQ centered at
**7000 Hz, Q 0.75, +0.75 dB**, then **-0.146715 dB** gain calibrated on
the first 0–4.8 seconds of that offline base audio.

The research measurements on a 44.1 kHz same-take 15 s sample improved the
third-phrase 4.5–7.9 kHz spectral error from 1.140 to 0.892 dB and the
9–14 kHz error from 0.839 to 0.809 dB. These are same-take spectral-band
errors, not a blinded listening test or held-out vocal performance.

## Native integration (factory index 46)

- Append a separate 13-slot **Deuce v2.5 (Native Trial)** program while
  preserving all earlier factory indices, effect IDs, APVTS parameters,
  rack ordering, automation compatibility, and native preset schema.
- Start with the 12-stage native D/Max-inspired Deuce rack, copying all
  stages bit-for-bit; append a native EightBandTone EQ (existing effect ID 26).
- Configure one bell to 7 kHz, +0.75 dB, Q 0.75; remaining seven bands flat,
  no high-pass/low-pass. Fully wet, causal, no added PDC, editable in the rack.
- Do **not** apply the offline -0.146715 dB calibration to the different native
  base; that would be an unsupported normalization transfer.

The native rack is a **D/Max-inspired approximation**, not the offline v2.3
REC/MAIN engine. The factory program is therefore not sound-identical to the
selected offline v2.5 winner. Preserve the original Deuce for rollback. The
offline v2.5 and user reference recordings remain untouched and are not
committed or embedded in the plugin.

## Verification still required

Before considering this a verified winner: compile the exact Windows x64
VST3; capture and inspect four genuine Quick4 screenshots; verify FL Studio
factory recall/save/reopen/export/import; run loudness-matched 15 s reference
A/B against the owner-selected offline v2.5 and an independent Deuce take;
measure native 13-stage CPU/latency, clicking, clipping, denormals and
third-party state compatibility.

Historical trial tests covered append-only numbering, preservation of the
12 existing stages, EQ units, finite stereo output and frequency response.
The consolidated branch's current Windows validation is the release gate.
