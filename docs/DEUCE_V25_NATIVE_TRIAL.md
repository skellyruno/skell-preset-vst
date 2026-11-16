# Deuce v2.5 (Native Trial) — Skell Preset

**Status:** implementation candidate only; not a verified transfer of the
owner-approved 15 s lossless listening sound, not a production preset.

## What was selected

The owner preferred the **Deuce v2.5 Presence Repair** lossless A/B version
over the earlier candidate on 2026-11-15 (America/New_York). The offline v2.5
signal path was **v2.3 Sustained Air** plus a causal RBJ peaking EQ centered at
**7000 Hz, Q 0.75, +0.75 dB**, then **-0.146715 dB** gain calibrated on
the first 0–4.8 seconds of *that offline base audio*.

The research measurements on a 44.1 kHz same-take 15 s sample improved the
third-phrase 4.5–7.9 kHz spectral error from 1.140 to 0.892 dB and the
9–14 kHz error from 0.839 to 0.809 dB. These are **same-take spectral band
errors**, not a blinded listening or held-out vocal performance.

## Native integration (factory index 46)

- Append a separate 13-slot **Deuce v2.5 (Native Trial)** program in Main Vocals,
  visible alongside original Alchemist and Deuce D/Max native trial.
- Preserve factory indices 0–45, existing effect IDs, APVTS parameters, program
  names, existing rack ordering, automation compatibility, and .skellpreset schema.
- Start with the current 12-stage native D/Max-inspired Deuce rack, copying all
  stages bit-for-bit; append a native EightBandTone EQ (existing effect ID 26).
- Configure one bell to 7 kHz, +0.75 dB, Q 0.75; remaining seven bands flat,
  no high-pass/low-pass. Fully wet, causal, no added PDC, editable in the rack.
- Do **not** apply the offline -0.146715 dB calibration to the different native
  base; it would be an unsupported normalization transfer.

**Important difference:** the native rack is a *D/Max-inspired approximation*,
not the offline v2.3 REC/MAIN engine. Therefore this new factory program is
**not yet sound-identical to the selected v2.5 winner**. Preserve D/Max for
instant rollback. The offline v2.5 and user reference recordings remain
untouched and are not committed or embedded in the plugin.

## Tests and release gates

Portable tests assert append-only program numbering, exact preservation of
all original 12 Deuce stages, physical EQ units, finite stereo audio and
7 kHz / 1 kHz frequency response at 44.1/48/96 kHz. The native JUCE renderer
also exercises factory loader, 13-slot access, parameter recall and PDC.

**Still required before considering this a verified winner:** compile
Windows x64 VST3 from exact commit; locally capture/inspect four genuine
Quick4 UI screenshots; real FL Studio factory recall/save/reopen/export/import;
loudness-matched 15 s reference A/B against the owner-selected offline v2.5
and a second, independent Deuce take; measure native 13-stage CPU/latency,
clicking, clipping, denormals and third-party state compatibility.
