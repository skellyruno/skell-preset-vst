# Consolidation audit — 2026-11-25

## Branch inventory and source provenance
- PR #90 `01a9f37c5aef2c1ec6b7e482c8e434198ee26959`: latest agent UI/DSP branch; Windows run 45603877296 succeeded, 51/51 native tests, real rack stress and editor layout/interaction smoke. The previous host synchronization already includes the exact #90 commit as an ancestor.
- PR #97 `d37509587467011c45241252d71b98c6659091bb`: experimental host, includes all of #90; latest FloopHole diagnostic policy, cached editors, native resize fallback, and exact-class recall are source changes, not yet confirmed in user's DAW. Keep nested FloopHole blocked.
- PRs #94, #95, #96: old draft branches with successful historical Windows runs 45328485600, 45313794563, 45315598534 respectively. Their old green CI is not proof of compatibility with current source. #95/#96 are stacked; #94 is from main and contains additional Club trial work. Do not wholesale merge those branches or close them while residual changes remain.

## Selected integration from #96 (722588e)
Verified each of six selected files was byte-identical to #96's merge base on the integration branch before importing the exact newer blob. The discovery test's isolated hunk applies once to the current test, preserving newer host tests.
- InstalledVST3Index + InstalledVST3Browser: finish processing admitted folders when folder queue is capped, exact/prefix search ranking, preserve selected path through rescans, keyboard loading and stale-row protection. Regression enumerates 520 folders, expects 511 admitted effect bundles.
- ClipRecorder + its test: protect existing WAV recordings, control-thread start/stop serialization and per-session producer token, atomic sample-rate readout, retain completed recording duration, release-mode assertions, mono and invalid-input tests. No lock added to push().
- RackRealtimePolicy + test: clamp corrupted capacity/count before bit masks; release-mode assertions instead of disabled assert.
Do not include older macro mapping (conflicts with latest agent's actual classic macro targets), whole processor/UI replacements, experimental Club recipes, older factory catalog, or private research/recordings. Other older changes remain for individual review.

## Catalog/compatibility
Current test_factory_catalog_history.cpp asserts exactly 39 curated factory identities, preserving Digital Bones and original retained recipe labels; the Nov 23 owner-approved 16-Adlib pruning is recorded in AGENTS.md. Older 55/52-count documentation is not the current catalog. This audit does not alter factory or parameter IDs. Agent's stereoSeparation is appended, legacy default neutral. No FL-native DLLs or private research assets added. Handoff notes say stereo law still needs rights clearance before distribution and FL numerical parity is unverified.

## Release gates
Exact combined Windows build, editor-cache/native bounds regression, diagnostic report, recorder test, discovery limit and portable tests required. No production merge. Local Quick4 unavailable here: pinned JUCE GitHub resolution failed; use actual Windows renderer/Quick4 on exact head, inspect actual images at multiple scales, fix then recapture. #90 agent reported 4 and 128 local scenes on an earlier revision; those captures are not independently inspected for this combined head. UI layout test is not visual proof. Real FL Studio test, installed plugin compatibility, direct Fruity audio hosting, pluginval, and release rights remain outstanding.
