# Skell-Preset / Experimental third-party VST3 slots

**Windows development builds only.** This is JUCE's in-process VST3 host, not a
sandbox or a DAW replacement. A faulty external plugin can hang, crash or corrupt
the parent FL Studio process. Save your FL project before testing a new insert.

## How to try it

1. Open Skell-Preset / Advanced.
2. Choose an empty rack slot (or click **+ ADD SLOT**) and select **External VST3**.
3. Click **LOAD VST3** to open the native installed-plugin browser.
   Type part of a bundle name or location, choose a result, and click **LOAD**
   (or double-click it). **RESCAN** refreshes installed bundle locations.
4. If your plugin is installed elsewhere, choose **BROWSE FILE...** from the
   browser to manually select its `.vst3` bundle.
5. Once loaded, click **OPEN PLUGIN UI** to use the third-party editor.
6. In the left rack, drag the circular wet ring for per-effect 0–100% blend,
   click the far-right green dot to bypass/enable that effect, and double-click
   the effect name to reopen the searchable replacement library. These controls
   continue to automate the original `sN_wet` and `sN_on` host parameters.
   The hosted effect receives genuine multi-sample blocks, not one process call per sample.
   The selected insert panel shows its own reported latency in samples and
   milliseconds, while the rack header shows combined PDC. Search results
   are filenames, not verified plugin metadata; only LOAD validates the VST3.

### Installed bundle discovery
The browser indexes *file and folder names only*, without loading or executing
third-party plugin binaries merely to populate search results. It checks the
Windows standard system `%COMMONPROGRAMFILES%\\VST3` and per-user
`%LOCALAPPDATA%\\Programs\\Common\\VST3` folders. Files and directories
ending in `.vst3` are treated as opaque bundles; their interior is never
searched. Results are case-insensitive and show the actual path, not guessed
vendor metadata or a fabricated classification.

To bound GUI work, the index walks a few folders per timer tick, with limits
of 512 folders, 1,024 bundles and five directory levels. A partial-index
warning is shown if limits are reached. Manual file selection remains available
for custom VST3 paths. This is an initial built-in effect browser, **not a
sandboxed or exhaustive VST3 metadata scanner**. Loading a result still calls
the existing in-process JUCE VST3 scanner/loader and may block or crash a DAW
if the third-party plugin is faulty.

The host accepts **stereo-input/stereo-output audio effects only**. Instruments,
MIDI routing, arbitrary sidechains and external VST2/AU/CLAP formats are not
supported. The native rack forwards available DAW tempo, playing/recording state,
timeline position and timing to nested VST3s. Skell-Preset reads the host
playhead once per callback, passes a stable snapshot to the hosted effect, and
advances sample/seconds/PPQ for internal chunks. This is a read-only timeline
bridge, not transport control, MIDI forwarding or note-trigger support. Timing
fields absent from FL Studio remain absent rather than guessed. Host loop
wraps inside unusually large audio buffers are not yet simulated; test real
looping transport before calling this production-ready. Plugin scanning and creation are performed on the JUCE message
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

**Latency:** The external insert panel shows each loaded plugin's reported PDC
in samples and milliseconds at the current sample rate. Values above 5 ms are
highlighted as a tracking caution, not a failure. The toolbar also shows
Skell-Preset's whole-rack PDC. Hosting does **not** reduce an external plugin's
own lookahead or guarantee low round-trip monitoring latency. Skell-Preset adds
reported hosted latency to its own host PDC and delays the slot dry signal to
preserve wet/bypass alignment. The total hosted latency is capped at 65,536
samples at plugin load. Dynamic latency changes can still click or briefly
misalign while a project is playing; stop playback before changing external
plugin quality/oversampling. Recursive loading of Skell-Preset itself is
rejected before the nested instance is created.

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
