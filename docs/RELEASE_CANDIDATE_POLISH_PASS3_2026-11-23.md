# Release polish pass 3 — November 23, 2026

Scope: 25 higher-impact implementation/verification items and 20 detailed edge-case fixes. These are related changes, not 45 independent new DSP modules. All 39 factory programs, Digital Bones, module IDs and APVTS identities are retained. No intentional audible effect-transfer change; the MIC coefficient cache was checked for exact parity with the previous implementation.

## 25 higher-impact items

1. Prevent WAV capture from appending to an existing take; require a fresh filename.
2. Serialize recorder start/stop operations across editor and host preparation threads without putting a mutex on the audio callback.
3. Give every capture a unique session token so a delayed callback from an earlier take cannot enter a newly started take.
4. Treat a failed writer-thread start as a recording failure rather than showing an armed recording with no worker.
5. Validate destination and sample rate before stopping an active take.
6. Count pending queued frames when the writer fails, rather than reporting only the current failed write.
7. Preserve completed recording duration and counters until the next successful take.
8. Make the real WAV integration fixture enforce checks even under NDEBUG.
9. Expand the actual WAV fixture to cover a second take, mono-to-stereo recording and 44.1 kHz headers.
10. Fix VST3 discovery truncation so all admitted folders are still enumerated after hitting the folder cap.
11. Rank exact and prefix VST3 name searches ahead of path-only hits.
12. Preserve rescan selection by file identity; disable Load while the chosen file has not been rediscovered.
13. Allow keyboard-selected VST3s to override the pending rescan target and load with Enter.
14. Cancel browser scanning when the component is hidden, even when dismissal bypasses its dedicated close action.
15. Cache effective MIC tone parameters and avoid six repeated power calculations per unchanged block.
16. Add MIC regression coverage for cache hits, cache invalidation after reset and invalid automation; independently compare against the uncached source.
17. Use the expanded front-end MIC panel space for five actual model-zone deltas, responsive to strength.
18. Replace the legacy rack MIC's three-point sketch with a five-zone response card and numerical dB values.
19. Add deliberate amount and trim double-click resets to their real defaults.
20. Distinguish the independent noise-gate page and its actual bypass state with appropriate title/status.
21. Use a 64-bit waveform capture cursor end-to-end through the processor/editor for long-running sessions.
22. Stop treating a reset-to-zero waveform counter as new live audio; prevent stale peak values from refreshing the performer.
23. Keep stage reflections and contact shadow out of the waveform lane, and paint the normal panel even at unsupported tiny sizes.
24. Clamp malformed rack capacity before applying active-slot masks; preserve latency/bypass policy.
25. Expand release-mode rack and scope fixtures to cover negative/oversized state, duplicates, tiny snapshots and exact 64-bit width.

## 20 niche items

1. Reset the recorder FIFO explicitly before a new take.
2. Break the producer loop if the FIFO makes zero progress, preventing an unintended real-time spin.
3. Suggest a unique default WAV filename rather than repeatedly offering the last capture destination.
4. Publish the recording sample rate atomically so elapsed-time queries do not race with control operations.
5. Clean up a newly created destination if WAV writer creation fails.
6. Close and clean up the newly created file on writer-thread startup failure.
7. Verify repeat stop retains the completed duration.
8. Verify a rejected existing destination keeps the original file size and does not stop the currently active take.
9. Verify null and zero-length capture pushes are harmless and pushes after stop do not extend the WAV.
10. Verify both channels of the mono recording are identical.
11. Deduplicate discovery folders before checking the cap so duplicate roots do not spuriously mark truncation.
12. Reserve VST3 filter-result storage once per search.
13. Display search match count and pending-selection restoration state.
14. Guard programmatic list refreshes so they cannot accidentally count as a manual selection.
15. Check the underlying VST3 index entry again before attempting load.
16. Add explicit Load/Rescan/Browse tooltips explaining bundle discovery vs instantiation.
17. Reserve separate label space in the expanded five-zone MIC chart; keep the rack graph bars clear of dB captions.
18. Guard movable MIC resize when its parent is smaller than supported minimum dimensions.
19. Test full-ring, one-sample and zero-sample scope snapshots independently of the usual 128-sample editor view.
20. Test malformed rack count and capacity in release-mode checks, including duplicate physical IDs and out-of-capacity IDs.

## Evidence actually executed

Local environment lacks JUCE/SkellCore/Quick4; direct GitHub clone failed DNS again. No real editor screenshots or native Windows test results are claimed for this revision.

Copied three exact-source headers into an isolated local harness and verified Git blob hashes:
- MIC: c20cdc8b078fc3630bf7398906f771bf42cdf982
- Prior uncached MIC: 5849b16556b11ae9c4e6ebab6594895619f2e2af (restored original class spelling for hash)
- Scope: 0cec0a640a0833a94a3fc0fd8e362ae4748c543c
- Rack realtime policy: 5bf0061c887bb6ad46296608bbbd27c7d05f2627

Compiled and ran the independent C++17 harness with g++ -O2 -DNDEBUG -Wall -Wextra -Wpedantic -Werror -fsanitize=undefined:
- PASS: 1,920,000 stereo frames, five rates (8, 44.1, 48, 96 and 192 kHz), exact cached/uncached MIC output, randomized parameter changes, invalid parameters and resets.
- PASS: 200 randomized scope block partitions, 6,553,600 input frames, full/one/zero sample snapshots.
- PASS: 3,721 malformed/valid rack capacity and count combinations.
- No undefined-behavior sanitizer diagnostics during these checks.

These isolated results do not certify JUCE recorder, browser, real editor rendering or FL Studio behavior. Full native exact-head Windows and portable CI are required. Keep earlier PR #90's native workflow alive while this incremental pass is prepared; avoid cancelling its near-complete validation solely to add another polish commit. No merge or release approval.
