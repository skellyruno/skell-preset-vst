# Release candidate evidence ledger — draft PR 90

Baseline: cd55bf5fa8c6cfc53af7b52b6cea168f786ceffc, confirmed from GitHub and fetched branch. Run 45388428333 compiled the VST3 but failed the native regression build: masterSignalSmoke's source lambda did not capture sr. Portable CI passed. Earlier UI passes explicitly lacked local screenshots. Source changes and preview artifacts are not visual or release proof.

This ledger is updated with actual outcomes; pending means UNVERIFIED.

| Request | Previous implementation / gap | Change and evidence required | Status |
|---|---|---|---|
| Review continuity, shared rules, SkellCore, recent history and builds | Repeated UI passes without captures | Read instructions/history; inspect baseline and exact-head compiled UI | Baseline audited; capture pending |
| All 45 effect states, hardware interfaces, meaningful feedback | Shared 835×206 graph with a bottom control deck | Individual composition and real control mappings; capture every module | Pending |
| Classic and Studio limiter: Threshold, Release, Attack only; no MORE | Classic has fixed 70ms release/instant attack; Studio pages hide original controls | Append compatible timing controls, preserve original parameters; audio, state, ZIP and UI tests | Pending |
| Remove help, MATCH, FX Down, VISUAL, duplicate filters | Help still visible; generic graph interaction remains | Remove controls and obsolete hit regions; verify compiled UI | Pending |
| Detailed anatomical performer, natural animation | Native joint/rib/skull geometry, uniform rigid body motion | Refine geometry, lighting, head/breath/mic motion; inspect idle/active | Pending |
| Cinematic graveyard, architecture, trees, moon, fog, depth | Six small markers on a nearly empty stage | Cached static environment; restrained lighting and contact shadows | Pending |
| Larger varied lively skeleton audience | 28 similar figures in two layers | More layers/poses/phases; actual telemetry; measure paint cost | Pending |
| Logo white SKELL/green PRESET, tight exact tagline, v.01 below ET | Version beside/above title at y15 | Text-metric placement; inspect 70/85/100/125% | Pending |
| Half-height IN/OUT bar and reclaimed canvas | 53px footer; tiny master dials embedded there | Compact meter rail and readable Simple master area | Pending |
| Simple-only live Input dB / Mix % / Output dB | Labels added; master rendered test does not compile | Compile and run real audio input/blend/output/smoothing/recall tests | Pending |
| Browser Thumbnail/List actual layout + persisted UI preference | Directory grid only, preset rows only | Shared layout/hit testing, preserve filters/selection/search/scroll; interaction test | Pending |
| Six primary destinations; separate library management | Present | Preserve and inspect main/adlib, genres, tiers, artists, favorites | Pending |
| Multi-genre safe extension, unassigned, metadata/art/favorites/pairs | Single-genre sidecar; manual organization | Audit extension safety, preserve all personal data and no duplicate presets | Pending |
| Browser search, breadcrumbs/back, counts, tabs, empty states, recent, keyboard, scrolling | Partial implementation and synthetic tests | Interaction/capture audit at supported scales | Pending |
| Browser dragging, all edges/corners, responsive min/max, retained bounds | Right/bottom edges only; bounds retained only during editor life | Persist UI-only preference and bounded gestures; native test/captures | Pending |
| Previous unresolved roadmap and compatibility gates | Recent docs record missing host/listening/pluginval/CPU evidence | Review and keep explicit unresolved list | Pending |
| DSP allocations/locks/blocking, denormals, finite samples, smoothing/PDC/CPU/memory | Portable suites; current native build failed | Real processor and portable suites; multi-instance 96k benchmark; audit findings | Pending |
| Protected factories, Alchemist, Deuce, Digital Bones, personal files, IDs, VST3/FL recall | Approved compact factory catalog on branch | Preserve recipes/IDs; recall, serialization, package and hosting tests | Pending |
| Local JUCE 8.0.12 + pinned SkellCore Quick4 → inspect → fix → recapture | Earlier local network unavailable | Use local cached JUCE and exact dependency checkouts; genuine PNGs | Dependencies located |
| Deep screenshots including all factory slots | Existing --full --factory-slots renderer | Compile/capture/open every relevant image; separate assertions from visual QA | Pending |
| Portable, VST3, UI, mapping, master, browser, packages, factory, state, automation, stress, external host | Existing test targets; some required additions | Run actual tests without weakening assertions | Pending |
| Pluginval, real FL Studio, musical reference listening | Previously unverified | Check installed runtime/reference availability; report exact limits | Pending |
| Same branch/draft PR, meaningful commits, no main merge, safe CI | Existing concurrency protects main/manual builds | Commit coherent changes, update PR with evidence, verify exact commit | Pending |
| Downloadable x64 VST3 ZIP, PE/metadata/provenance/hash/integrity | Prior preview only | Package exact tested binary; link ZIP + checksum + build/commit | Pending |
| Before/after, effects/stage/browser images, audio/CPU/latency results, honest status | Prior source assertions | Deliver actual artifacts and label IMPLEMENTED/COMPILED/TESTED/VISUALLY VERIFIED/FAILED/BLOCKED/UNVERIFIED | Pending |

Individual module review (ID is a compatibility contract):

| IDs | Modules requiring compiled face and control review |
|---|---|
| 0–8 | Empty; EQ Classic; Compressor; Limiter; Gate; Saturator; Reverb; Delay; Exciter |
| 9–16 | Adlib Throw; Adlib Doubler; De-Esser; External VST3; Broadcast Radio; Retro Crush; Tremolo; Stereo Width |
| 17–25 | Three-Band Dynamics; VoxRack Room; VoxRack Smooth Comp; Studio Chorus; Vocal Flanger; Ring Modulator; Vocal Rider; Ducking Ping-Pong; Vocal Vibrato |
| 26–34 | 8-Band EQ; FET + Opto; Harshness Tamer; Adaptive Noise Control; Studio Pitch + Formant; Alchemist Match Finish; Mic Modeling; Skelldizer; Studio EQ |
| 35–44 | Studio Limiter Hybrid; Studio Compressor; Studio Fast Dist; Studio Delay; Adlib CHOP; Adlib MOTION; Adlib FILTER; Adlib SATURATE; Vocal Feel Opto; Vocal Feel Suite |

Previous unresolved requests: source-accurate nine-mode Vocal Feel reverb is not implemented and must not be claimed; #94's distinct gate detector remains preserved rather than silently substituted; excluded roadmap snapshots and slot gain staging remain excluded. FL Studio project reload, nested third-party commercial editors, real-voice A/B and pluginval remain independent release gates.

## Saved development checkpoint — user requested early wrap-up

IMPLEMENTED in this checkpoint: hardware control compositions; append-only final-limiter attack/release with historical 0ms/70ms defaults and three visible controls; removal of the MORE component and question-mark component; 26px meter footer and Simple-only enlarged master controls; cached graveyard scenery and 88 audience figures; real Thumbnail/List browser toggle, bounded all-edge resizing, separate UI preferences, recent sorting and optional multi-genre metadata. Physical-unit display conversions mirror the native set() calls.

TESTED: an earlier local portable run passed all 40 tests, including default limiter bit-exact parity and timing deltas at 44.1/48/96k. The latest source requires a fresh run. COMPILED: an intermediate dirty-tree Windows x64 VST3 compiled successfully; it is not a verified final release for this checkpoint. Native audio/package/host fixtures remain incomplete.

VISUALLY VERIFIED: four untouched baseline Quick4 PNGs were captured and opened locally. They confirm the old version placement, thick footer, tiny master controls and graph-heavy effect face. Baseline renderer reported one existing Radio graph-title pixel assertion failure, and its contact sheet was black. Current renderer source fixes graphics-context flushing before encoding and updates that assertion to the new hardware readout location without lowering its pixel threshold. No redesigned screenshots have yet been inspected.

UNVERIFIED / remaining: complete renderer build, fresh Quick4/full/factory-slot capture and visual correction cycle; professional refinement of each effect; remove classic Reverb/Delay/Saturator/Exciter controls that source audit found unused by DSP while retaining IDs; dedicated Empty/external face refinement; active/idle stage captures and measured paint cost; broader browser and multi-genre persistence QA; native master/PDC and limiter host/ZIP tests; final portable/native suites; 96k multi-instance CPU/latency/memory measurements; pluginval; exact-commit VST3 packaging, metadata, checksum and integrity. No finished VST3 ZIP is claimed.

Owner will scan and test in FL Studio; further FL UI automation was stopped at their request. Keep PR 90 draft and do not merge.
