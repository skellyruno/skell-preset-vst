# Skell Preset — live PR, branch, release and automation audit
**Audited:** 2026-11-23. **Repository:** `skellyruno/skell-preset-vst`. **Baseline main:** `3bc869aee64f9ead5eddc1e750ab949d2b371ecf`.
**Scope:** REST inventory of all 51 branches, 92 historical PR records, all three open PRs, exact branch-vs-main comparisons for all 50 non-main tips, currently configured main workflows, rulesets and representative CI evidence. This is a follow-on to `REPOSITORY_AUDIT_2026-11-22.md`, not a replacement.

## Executive decision
**Do not merge, delete branches, or publish a stable VST3 yet.** All three open PRs are drafts; current main reports `protected=false` and repository rulesets `[]`. Old source refs remain important for historical recovery. The exact latest UI candidate requires passing native Windows UI tests, local Quick4 screenshot inspection, and FL Studio state/preset verification.

## Open PRs and exact state checked

| PR | Branch/head | Relative to main | CI and decision |
|---|---|---|---|
| [#90](https://github.com/skellyruno/skell-preset-vst/pull/90) | `feature/studio-dynamics-delay-ui-20261121` / `93113ac` | 94 ahead / 0 behind, 56 changed files | Main integration; 2026-11-23 portable suite passed; Windows VST3/UI test was **in progress** in [run 45288755174](https://github.com/skellyruno/skell-preset-vst/actions/runs/45288755174) when checked. Prior head `0d32b6e` compiled VST3 but failed `SkellPresetWindowsUIInteractionSmoke`, traced to overly broad visibility assertion for Studio Limiter compact MORE state. `93113ac` corrects assertion, adds expanded/collapsed coverage and Settings hit/hover polish. Do not assert current head passes until this run concludes. |
| [#91](https://github.com/skellyruno/skell-preset-vst/pull/91) | `audit/repo-safeguards-inventory-20261122` / `e6c5acd` before this report | 3 ahead / 0 behind, 6 changed files | Safeguards branch. [Exact-head CI 45024207760](https://github.com/skellyruno/skell-preset-vst/actions/runs/45024207760) passed. Safeguards are **not active on main** until reviewed and merged. |
| [#94](https://github.com/skellyruno/skell-preset-vst/pull/94) | `feature/mic-noise-gate-club-20261122` / `8ce0ad7` | 4 ahead / 0 behind, 11 changed files | Native MIC gate follow-up, [exact-head CI 45030662506](https://github.com/skellyruno/skell-preset-vst/actions/runs/45030662506) passed. Shares core processor, preset-package and MIC editor files with #90 but also adds `Source/chain/MicGateDSP.h` and `tests/test_mic_gate.cpp`. Integrate only after explicit behavior/preset reconciliation, host recall and listening verification. |

The historical AdlibFX branch `feature/adlibfx-compatible-stacked-20261122` is 17 commits ahead of main but **0 commits ahead / 77 behind #90**, so it is already fully reachable from the integration branch. Do not cherry-pick/merge it again. The older `feature/native-skellcore-adlibfx-rack` diverges 5 ahead / 748 behind main; #8 is closed and must not be blindly revived. #89 is also closed, but its historical branch remains.

## Critical protection and workflow findings
1. **Critical, server-side:** `main.protected=false` and `/rulesets=[]` as read on 2026-11-23. Review and activate required checks and pull-request review in repository Settings; tracked in [issue #93](https://github.com/skellyruno/skell-preset-vst/issues/93). This cannot be solved by editing YAML alone.
2. **Critical, active on main:** `.github/workflows/import-stage2.yml` is manually dispatchable, carries `contents: write`, and includes `git push origin HEAD:main`. PR #91 retires this behavior without deleting the historical workflow source.
3. **High, active on main:** `.github/workflows/repo-maintenance.yml` exposes `merge-pr`, `delete-branch` and `prune-merged-branches` commands. The old merge case can move a draft to ready and merge; the original delete case checks little beyond excluding main. PR #91 removes unsafe merge dispatch and guards branch deletion with ancestry / open-PR / protected-name checks.
4. **Medium:** The manual `ui-screenshots.yml` workflow is not the required **LOCAL** SkellCore Quick4 visual capture/inspect/fix/recapture. Never claim CI geometry tests equal human/agent screenshot approval.
5. **Medium:** Root `SOURCE-MANIFEST.json` is historical Stage 1 provenance and not proof of the current `0.7.0` release candidate. Verify exact shipping bundle metadata and SHA separately.

## Branch and PR inventory
51 branch tips: **1 main, 4 strictly ahead of main, 46 diverged**. Historical GitHub PR records returned: **92 total: 3 open draft, 16 merged, 73 closed without merge**. All 50 non-main branches were independently compared to main by the GitHub compare API on this date. Commit-graph comparisons do **not** prove two branches' code is equivalent. Previous 2026-11-22 audit compared recursive trees of the original 47 non-main refs and found five branch-only file paths; the three newly inventoried branches require their own source reconciliation. No deletion is justified by this table.

| Branch | Ahead | Behind | Relation to main | Context |
|---|---:|---:|---|---|
| `audit/alchemist-empty-rack-roundtrip-20261115` | 4 | 316 | diverged | closed #36 |
| `audit/default-preset-slot-ids-20261115` | 4 | 502 | diverged | closed #35 |
| `audit/preset-slot-order-strict-20261113` | 18 | 506 | diverged | closed #17 |
| `audit/repo-safeguards-inventory-20261122` | 3 | 0 | ahead | open draft #91 |
| `chore/audit-retire-legacy-template-build-20261114` | 1 | 502 | diverged | closed #27 |
| `docs/alchemist-factory54-catalog-audit-20261118` | 2 | 51 | diverged | closed #75 |
| `dsp/alchemist-independent-compressor-blends-final-20261117` | 1 | 97 | diverged | closed #52 |
| `dsp/alchemist-skelldizer-recall-20261115` | 5 | 316 | diverged | closed #37 |
| `dsp/vocal-harshness-block-invariant-20261115` | 7 | 316 | diverged | closed #38 |
| `feat/alchemist-causal-matchfinish-20261114` | 1 | 316 | diverged | closed #20 |
| `feat/alchemist-lighter-dynamics-trial-20261116` | 2 | 100 | diverged | closed #50 |
| `feat/alchemist-native-preview-20261114` | 7 | 502 | diverged | closed #18 |
| `feat/alchemist-peak-guard-trial-20261116` | 1 | 110 | diverged | closed #48 |
| `feat/alchemist-use-skelldizer-20261114` | 1 | 313 | diverged | closed #24 |
| `feat/deuce-v25-presence-native-trial-20261115` | 1 | 146 | diverged | closed #44 |
| `feat/preset-eq-hardware-overhaul-20261112` | 1 | 485 | diverged | closed #15 |
| `feat/preset-rack-workflow-ui-20261115` | 3 | 222 | diverged | closed #41 |
| `feat/preset-ui-alchemist-focus-20261114` | 1 | 454 | diverged | closed #19 |
| `feature/adlibfx-compatible-stacked-20261122` | 17 | 0 | ahead | closed #92; merged into #90 only |
| `feature/mic-noise-gate-club-20261122` | 4 | 0 | ahead | open draft #94 |
| `feature/native-skellcore-adlibfx-rack` | 5 | 748 | diverged | closed #8; historical branch preserved |
| `feature/skelldizer-native-dsp` | 8 | 502 | diverged | closed #23 |
| `feature/skellworld-optin-presence` | 1 | 507 | diverged | merged #16 |
| `feature/studio-dynamics-delay-ui-20261121` | 94 | 0 | ahead | open draft #90 |
| `fix/alchemist-audition-window-alignment-20261118` | 6 | 56 | diverged | no PR |
| `fix/deuce-v25-native-trial-ci-20261116` | 3 | 146 | diverged | closed #45 |
| `fix/empty-rack-host-program-identity-20261120` | 33 | 502 | diverged | closed #82 |
| `fix/empty-rack-host-program-identity-55bank-20261120` | 1 | 35 | diverged | closed #83 |
| `fix/empty-rack-new-instance-default-20261119` | 28 | 502 | diverged | closed #81 |
| `fix/host-state-parameter-sanitize-20261118` | 11 | 55 | diverged | closed #72 |
| `fix/host-state-sanitize-55bank-20261119` | 4 | 42 | diverged | closed #78 |
| `fix/legacy-preset-macros-neutral-20261117` | 10 | 83 | diverged | closed #56 |
| `fix/preset-host-state-nan-sanitize-main-20261119` | 23 | 502 | diverged | closed #80 |
| `fix/preset-latency-cpu-stress-20261114` | 1 | 300 | diverged | closed #25 |
| `fix/preset-module33-import-20261114` | 4 | 316 | diverged | closed #33 |
| `fix/preset-package-neutral-legacy-20261118` | 26 | 502 | diverged | closed #67 |
| `fix/quick4-contact-sheet-flush-20261118` | 1 | 55 | diverged | closed #71 |
| `fix/skell-preset-single-extract-artifacts-20261118` | 3 | 97 | diverged | closed #64 |
| `fix/skell-preset-vst3-metadata-flat-artifact-20261118` | 15 | 502 | diverged | closed #69 |
| `improve/rack-pdc-polish-20261112` | 22 | 507 | diverged | closed #14 |
| `perf/alchemist-matchfinish-math-20261117` | 6 | 83 | diverged | closed #59 |
| `perf/alchemist-serial-gain-table-20261117` | 9 | 83 | diverged | closed #55 |
| `test/alchemist-reference-ab-controlled-density-20261120` | 4 | 2 | diverged | closed #89; historical branch preserved |
| `test/alchemist-reference-ab-serial-blend-20261118` | 2 | 49 | diverged | closed #74 |
| `test/alchemist-stage-cpu-profile-20261117` | 1 | 83 | diverged | closed #57 |
| `test/deuce-v25-full-state-recall-20261118` | 2 | 97 | diverged | closed #63 |
| `test/deuce-v25-full-state-recall-54bank-20261118` | 1 | 62 | diverged | closed #65 |
| `test/deuce-v25-native-trials-ctest-20261116` | 6 | 146 | diverged | closed #46 |
| `test/preset-rejected-import-atomicity-20261114` | 5 | 502 | diverged | closed #21 |
| `test/studio-eq-preset-roundtrip-20261117` | 1 | 70 | diverged | no PR |

## Release gates that remain independent
- **Current native editor:** rerun exact-head Windows VST3, editor interaction / all native effect types, package import/export, UI layout, metadata, portable DSP tests and artifact integrity; never reuse earlier-head green checks.
- **Visual:** exact-head local Quick4 real JUCE screenshots for Obsidian/Crimson, Simple/Advanced, Digital Bones, preset browser, MIC and Settings; inspect 70/85/100/125%, both rack docks, popup stacking, text clipping and hit targets. Fix and **recapture** after any defect.
- **Host/state:** FL Studio VST3 scan, project save/reload, known old automation IDs, all 55 factory host indexes (especially 43/45/46), existing user presets, legacy `.skellpreset` imports, move/resize/keyboard/undo/redo and pluginval.
- **Audio/performance:** same-take, level-matched vocal A/B must remain independent from UI release polish, with CPU/PDC/latency, finite-sample, denormal checks. Do not modify DSP merely to make UI tests pass.
- **License/packaging:** verify a commercially suitable JUCE license and any third-party asset/source attribution before distribution; do not commit third-party raw recording/preset binaries without authorization.
- **Source preservation:** all historical branches, old PR discussions and unmerged experiments remain intact until a separate assertion-by-assertion reconciliation is approved.

## Audit change policy
No merge, PR closure, branch deletion, production tag or release was performed by this audit. Changes to #90 were UI-only with the same published APVTS/factory IDs and untouched DSP implementations. Server protection is still an administrator action. This document is an additive inventory and does not mark unresolved gates as passed.
