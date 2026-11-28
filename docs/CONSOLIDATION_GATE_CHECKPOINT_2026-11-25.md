# Consolidation and gate research checkpoint — 2026-11-25

## Current release catalog
Only Alchemist, Deuce, and Digital Bones (Adlib) are selectable current factory sounds in the preset browser. The owner confirmed retirement of all other legacy factory sounds. Keep personal presets and automation untouched; distinguish old state recall from current factory selection. No host program index migration is implemented in this checkpoint.

## Actual UI and save changes
Commit 623d04c simplifies the Simple macro panel, empty-slot Add Effect face and rack affordances, corrects the Simple waveform dirty rectangle to the actual identity strip, narrows the rack scrollbar, and corrects the displayed slot count. It also retains the save-in-flight guard until completion, scans after copying the cover but before indexing new saved-preset metadata, and preserves the actual cover extension. Library scan and copy still run on the message thread. No DAW reproduction, UI capture, or Windows verification yet. Do not call the Save freeze fully solved.

## Noise gate
The owner supplied a limiter research implementation with a threshold, noise floor gain, and a release law in which the residual closes to 1% after twice the release time. A separately authored portable candidate implements only that recurrence with a supplied detector peak. It does NOT reproduce the full limiter peak window, compression, saturation, or lookahead. Existing mic gate runs before mic coloration and remains unchanged; explicit factory selection enables it, legacy recall and Empty Rack retain their previous behavior. No new parameter IDs or state migration.

Standalone strict C++17 g++ -Wall -Wextra -Werror -pedantic -fsanitize=undefined -O2 tests passed at 44.1/48/96/192 kHz. For a 90 ms setting and linear floor 0.025, gain reaches 0.034750 after 180 ms. Checks include monotonic bounded decay, 127-sample partition invariance, opening, shared stereo gain, finite handling, and neutral floor. Header/test Git blob hashes were checked against the exact locally compiled source before creating this commit. No real vocal A/B or full limiter parity claimed. Gate candidate remains offline and unwired until current gate vs candidate vs same-take Club Gate B and Fruity Limiter reference comparison; evaluate detector history, quiet syllables, transients and opening clicks. Verify source licensing before shipping any third-party code; this checkpoint does not commit the supplied source or private audio.

## Seven workstreams
1. Save freeze: partial code fix only; move expensive library scans and cover/copy work off the message thread with race-safe completion and repeated host tests.
2. Consolidation: primary #90; #95/#96 overlapping; #97 includes selected older fixes but failed native hosted-editor smoke. Reconcile remaining diffs without overwriting current UI. #94 already partially superseded by current gate; retain its distinct detector for A/B.
3. Simple/empty: initial source changes committed, verify local Quick4 and fix visual defects.
4. Stereo: dedicated Advanced rack processor migration remains outstanding. Preserve old post-rack parameter and exact serialized behavior; avoid double processing and retain automation.
5. Visualizer: wrong Simple dirty rectangle fixed; stage frame-time profiling and elapsed-time animation still pending.
6. External host: exit 96 needs granular test diagnostics and actual root-cause fix, keep nested FloopHole blocked.
7. Verification and merge: local JUCE 8.0.12 retrieval blocked by GitHub DNS. No real current Quick4 images or Windows build verified for these commits. Keep PR draft; do not merge unverified changes to production.
