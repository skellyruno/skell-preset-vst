# 322 DAW concepts in Skell Preset — implementation record

**Status:** PR #6, stacked on the original advanced-suite PR #5. The original
Skell Preset processor, Simple macro mixer, existing 16 rack slots, and all
previous factory preset IDs are unchanged.

## What was inspected

User-provided `322_DAW_source_with_real_3D_logo.zip`,
`322-daw-main.zip` and `322-daw-1.0.0-assets (1).zip`.

322 is a React/Web Audio DAW. The relevant files:
- `src/lib/builtinPlugins.ts`: registry with six AudioNode factories and
  parameter definitions.
- `src/hooks/useAudioEngine.ts`: microphone permission, track arm, analyser,
  MediaRecorder recording/clip blobs, transport controls.
- `src/pages/workspace.tsx`: transport, clip lanes, track meters and routing.
- `src/pages/recordings.tsx`: recorded-clip browser.
- `src/pages/plugins.tsx`: built-in effect list / external VST3 bridge.

This source **does not contain a separate named 322 factory preset library**.
It has six effect parameter defaults and project/clip entities. The recording
thumbnail widgets use `Math.random()` placeholder waveforms; copying those
would be misleading. Its immediate `mr.stop()` then Blob construction can
also occur before the asynchronous final MediaRecorder chunks arrive, so we
did not copy that recording implementation.

## Six effects compared and retained

| 322 built-in | Skell native alternative | Decision |
|---|---|---|
| 322.EQ: lowshelf + peak + highshelf | Legacy one-bell/highpass EQ or new **8-Band EQ** with Q per band | Keep both native variants; more control, never replace Simple |
| 322.COMP: Web Audio DynamicsCompressorNode | Legacy Compressor, VoxRack Smooth Comp, FET + Opto | Retain native options and attack/release controls |
| 322.VERB: randomized 2.5s Convolver | SkellCore FDN Reverb + VoxRack Room | Keep deterministic existing DSP; no heavy generated IR in audio path |
| 322.DELAY: DelayNode feedback | Skell stereo Delay, Adlib Throw, Ducking Ping-Pong | Keep preallocated native echo and modulation |
| 322.DIST: oversampled WaveShaperNode | Skell native Saturator and Retro Crush | Retain controllable saturation rather than blindly port Web Audio |
| 322.CHORUS: Oscillator-modulated DelayNode | Skell native Vocal Chorus, Vocal Flanger, Vocal Vibrato | Keep native stereo modulations; avoid Web Audio node graph |

**We do not claim a listening-benchmark quality ranking.** Decisions are
based on architecture, native VST3 suitability, control range, automation
and real-time predictability. No 322 DSP engines or React UI were embedded.
They have Web Audio APIs and no demonstrated audio-quality advantage over
the Skell equivalents.

## Implemented

### Effect registry
`Source/chain/EffectRegistry.h` is the canonical C++17 array of all 31
existing effects. IDs 0–30 stay fixed. Contains name, category, searchable
keywords and the 322-equivalent tag where relevant. The processor's choice
names, slot editor's selector and searchable effect picker derive their text
from it, preventing three separate hard-coded lists drifting apart. Registry
metadata is not a DSP engine and cannot change host automation or latency.

### Real live audio visualization
`Source/chain/LiveAudioScope.h` samples the *actual processed output* at 1
sample per 64 audio frames into a bounded, atomic 256-slot ring. The
message-thread editor reads snapshots and draws a neon waveform in the main
toolbar and an enlarged trace in Simple mode. At silence it is flat; the
display never generates fake sine or random samples. There are no UI calls,
allocations, locks or file reads in the realtime producer.

### WAV recording workflow
A new **REC WAV** control asks where to save a file, records the processed
audio sent by FL Studio to this plugin, and stops/finalizes as a 24-bit stereo
WAV. The host controls microphone capture, transport and track arming.
Recording within Skell Preset is a *post-effect print*, not a separate
microphone interface or FL playlist recorder.

The producer writes to a preallocated, bounded 4-second `AbstractFifo`
stereo ring. A separate JUCE `Thread` drains that ring and writes
AudioFormatWriter WAV data. The producer never waits for disk I/O; on overflow
it counts dropped samples instead of blocking the DAW. On Stop, pending samples
flush and the file header finalizes. An alert reports dropped samples.

**Limitations:** no true multi-track timeline, punch-in, loop-takes, cloud
recording gallery, microphonic device selection or independent recording of
a track that never feeds the plugin. Use FL Studio's built-in capture workflow
for those. The button does not alter track state or the original processor.

### New opt-in preset
**Studio Six** is a new Skell-authored preset appended at factory index 42,
containing the six native counterparts: EQ, Compressor, Saturator, Delay,
Reverb and Vocal Chorus. It is *not* marketed as a 322 preset because the
source has no named factory presets. The original 0–41 presets remain at
their exact numerical indices and are not overwritten.

## Quality gates

- Portable C++ regression verifies all old effect IDs and names, six
  comparison mappings, no fake waveform on silence, signal response,
  mono/stereo capture and display bounds.
- JUCE WAV smoke test records a known stereo signal to a unique temp WAV,
  stops/finalizes, reads it back and asserts 24-bit/48k stereo/audio data.
- Headless actual-editor screenshots at multiple sizes and themes, Windows
  MSVC /W4 /WX VST3 build and JUCE host checks.
- Manual FL Studio test still required: processing at 44.1/48/96k,
  different buffer lengths, simultaneous external-VST3 stages, long captures,
  capture overflows, save cancel, file/folder permissions, audio playback,
  and host transport stopping during capture.

**Do not merge PR #6 until #5, #4, #3 and #2 are reconciled in order.**
