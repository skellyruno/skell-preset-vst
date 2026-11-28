# SkellCore AdlibFX compatible reconciliation (stacked after #90)

The old PR #8 was built against a 23-module rack, lacks current append-only APVTS selectors, and conflicts with production. This draft is built on `67e75dc39afd4ae517901dde1ef2ff1224765649` (#90).

Four additional opt-in effect IDs follow Studio Delay 38: **39 CHOP**, **40 MOTION**, **41 FILTER**, **42 SATURATE**. All use pinned, shared `SkellCore/AdlibFX.h` from `cf2d182a1c4b1b45f97253f5e1b17dd91343aeaa`. Each of the sixteen slots has its own prepared JUCE-free processor. Audio `process()` is allocation-free and adds zero PDC.

Existing module IDs **0–38** and original per-slot `sN_type`, `sN_fxExt` through `sN_fxExt8` choice counts and all host automation indices stay intact. New selector `s1_fxExt9` through `s16_fxExt9` is appended at the end of the entire APVTS layout; no existing parameter is inserted or renormalized. Four existing controls p1..p4 are Amount / Rate / Tone / Mix; fresh selection defaults are non-destructive. No existing factory changes.

Compiled Windows `--studio-smoke` tests each effect in the real JUCE plugin processBlock path: finite/different stereo samples, no new latency and saved-state restore. Portable registry test asserts names and IDs 0..42.

**Outstanding gates:** exact-head Windows tests/artifacts, real Quick4 capture and inspection, FL Studio host/preset/automation recall, CPU/latency budgets and listening A/B. The stacked draft must not merge before #90; do not close old #8 until unique changes are reconciled and verified.
