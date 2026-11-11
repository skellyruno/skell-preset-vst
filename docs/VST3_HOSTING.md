# Skell-Preset / Experimental third-party VST3 slots

**Windows development builds only.** This is JUCE's in-process VST3 host, not a
sandbox or a DAW replacement. A faulty external plugin can hang, crash or corrupt
the parent FL Studio process. Save your FL project before testing a new insert.

## How to try it

1. Open Skell-Preset / Advanced.
2. Choose an empty rack slot (or click **+ ADD SLOT**) and select **External VST3**.
3. Click **LOAD VST3** and choose a local, installed `.vst3` bundle.
4. Once loaded, click **OPEN PLUGIN UI** to use the third-party editor.
5. Use Skell-Preset's per-slot wet/bypass and reorder controls. The hosted
   effect receives genuine multi-sample blocks, not one process call per sample.

The host accepts **stereo-input/stereo-output audio effects only**. Instruments,
MIDI routing, arbitrary sidechains and external VST2/AU/CLAP formats are not
supported. Plugin scanning and creation are performed on the JUCE message
thread; loading a slow or defective plugin can stall the UI. The current host
retains replaced instances until the processor is destroyed to prevent
audio-thread use-after-free; replacements are capped per physical slot.

A selected VST3's path and opaque state are recorded with the **DAW project
state**. Reload is requested on the JUCE message thread after the DAW state
is restored. If a plugin is missing, the slot passes audio dry but shows an
unloaded insert. External plugins are currently **not** exported inside the
portable `.skellpreset` ZIP (the exporter explicitly rejects external slots
to prevent silent data loss). This is not an installer or redistributor of
third-party plugins and does not package their copyrighted binaries.

**Latency:** Each hosted plugin reports its own lookahead; Skell-Preset adds
that to host PDC and delays the slot dry signal to preserve its wet/bypass
alignment. The total hosted latency is capped at 65,536 samples. Dynamic
latency changes can still click or briefly misalign while a project is
playing; stop playback before changing external plugin quality/oversampling.

### Remaining validation before production

- CI verifies native VST3/Standalone compilation and the actual JUCE DSP
  regression harness with no plugin selected.
- A real installed *third-party* effect still needs integration testing inside
  FL Studio for UI, preset state, automation, CPU, crashes and latency changes.
- Audio units with arbitrary stereo/sidechains are intentionally rejected.
- The external editor runs in another window, not embedded inside the rack.
- No child process isolation is provided.

The original SkellCore DSP effects and your Skell-Preset native presets continue
to work without loading any third-party code.
