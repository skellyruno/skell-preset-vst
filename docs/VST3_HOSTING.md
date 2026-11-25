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

### External FloopHole pilot (2026-11-25)
Install the separately distributed upstream Windows x64 FloopHole v1.2.0 from https://github.com/FloopHole-Team/FloopHole/releases/tag/v1.2.0 and retain its license notices. A licensed local FL Studio installation is required. Skell does not bundle any bridge or Image-Line binary. Load FloopHole via External VST3; a multi-class bundle now prompts for a specific audio effect, and the DAW state stores the JUCE class identifier alongside path and opaque state. Missing exact classes are not silently replaced; old path-only states retain legacy selection. The installed bundle browser still indexes filenames only. Test actual licensed effects, native editor lifecycle, host tempo, state recall, dry/wet, PDC (upstream currently reports zero), multi-instance and crash behavior before production. Instruments remain excluded. Portable Skell preset export still rejects hosted slots. In-process bridge pilot is not sandboxed. No actual FloopHole runtime tested in CI; owned VST3 fixture and synthetic catalog exercise host selection. Local Quick4 unavailable: JUCE GitHub DNS failed.

### 2026-11-25 — Critical hosted-VST3 field regression
Owner reported none of their ordinary VST3s would load and FloopHole froze FL Studio. The new UI picker returned early unless the slot already was ExternalVST3, contradicting the intentional transactional module-browser behavior. That guard was removed. Added a real editor regression beginning in a Compressor slot and loading the compiled owned VST3 fixture through the actual selection callback. Windows CI now runs it alongside host fixture audio/state tests.

FloopHole v1.2.0 calls FLPluginScanner::discover(true) during Windows InitDll. Scanning or loading it in-process can freeze FL Studio, so reject its bundle before scanning or instantiating it, including DAW recall. Require a genuine out-of-process host before enabling nested FloopHole again; no FL Studio license or third-party binary is bundled. Other VST3s remain enabled. This is a field-reported regression with source-confirmed root cause but exact-head Windows and FL Studio testing pending.

Multi-class menu anchoring: anchor the class-choice popup to the visible SlotEditor, not the LOAD VST3 button, which is hidden while replacing a native effect.
