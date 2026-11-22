# Skell Preset audit — 2026-11-22

**Repository:** `skellyruno/skell-preset-vst`; **baseline:** `main` `3bc869aee64f9ead5eddc1e750ab949d2b371ecf`.

## Inventory and preservation
- **48 branches**: 1 main, 47 non-main. Branch-tip commit-graph comparisons: 1 ahead without being behind (#90), **46 diverged**.
- **3 open draft PRs:** #8, #89, #90. **89 PRs** in GitHub history: **15 merged**, **71 closed without merge**, 3 open.
- Of the 47 non-main branch names: 3 open PRs, 1 historical merged PR, 41 closed-unmerged PRs, 2 not associated with any of these PRs.
- **#88** merged on November 21: 500 commits, 86 files, historic production consolidation; old branch divergence does **not** establish that unique features are absent or safe to delete.
- No GitHub Releases returned. Main was reported `protected=false`; repository rulesets API returned `[]`. Enable required checks/reviews using repository administration.

## PR-specific decisions
- **#90 Studio dynamics, Studio Delay, mic gate and tutorial:** four commits ahead, zero behind. Exact-commit Windows CI, FL Studio automation/state, CPU/PDC measurements, A/B listening and local Quick4 screenshots are release gates.
- **#89 Alchemist Factory 55 controlled-density trial:** four ahead, two behind. Exact-head Windows VST3 and portable SkellCore CI passed; controlled same-take 19–34s A/B and user approval remain.
- **#8 native shared AdlibFX:** five ahead, 748 behind, `mergeable=false`, conflicts. Historical CI passed on 2026-11-12 but is not proof of compatibility with current architecture. Four AdlibFX modules were *excluded* from merged #88. Reconcile on a separate compatible branch; do not merge old PR directly.

## Maintenance and security
- **Critical:** legacy `import-stage2.yml` was manually runnable with `contents:write`, deleted `bootstrap` and `windows-vst3.yml`, and pushed to `main`. This safeguard PR retires the active job and preserves the original YAML as documentation.
- **Critical:** `repo-maintenance.yml` could automatically mark draft PRs ready and merge them without verifying CI. This safeguard PR disables that action.
- **High:** maintenance `delete-branch` could remove unmerged or active/release branch refs. This PR requires ancestry confirmation, blocks open PR head/base refs, and protects release/production names.
- **Medium:** manual-only `ui-screenshots.yml` remains an old headless CI screenshot action, not a substitute for the repository's required **local Quick4** capture/inspect/fix/rescreenshot workflow.
- **Medium:** root `SOURCE-MANIFEST.json` describes historical `0.1.0-stage1`, not current CMake project `0.7.0`; do not advertise it as current release provenance.
- **Medium:** open issue #22's 2026-11-14 description says no Skelldizer exists, which is now outdated; genuine sound matching and FL Studio audio/recall validation are still open.

## Complete branch comparison register

Ahead/behind values count branch-tip commits relative to `main`; these are **not** equivalent to a verified three-way source/diff reconciliation.

| Branch | Ahead | Behind | PR disposition |
|---|---:|---:|---|
| `audit/alchemist-empty-rack-roundtrip-20261115` | 4 | 316 | closed #36 |
| `audit/default-preset-slot-ids-20261115` | 4 | 502 | closed #35 |
| `audit/preset-slot-order-strict-20261113` | 18 | 506 | closed #17 |
| `chore/audit-retire-legacy-template-build-20261114` | 1 | 502 | closed #27 |
| `docs/alchemist-factory54-catalog-audit-20261118` | 2 | 51 | closed #75 |
| `dsp/alchemist-independent-compressor-blends-final-20261117` | 1 | 97 | closed #52 |
| `dsp/alchemist-skelldizer-recall-20261115` | 5 | 316 | closed #37 |
| `dsp/vocal-harshness-block-invariant-20261115` | 7 | 316 | closed #38 |
| `feat/alchemist-causal-matchfinish-20261114` | 1 | 316 | closed #20 |
| `feat/alchemist-lighter-dynamics-trial-20261116` | 2 | 100 | closed #50 |
| `feat/alchemist-native-preview-20261114` | 7 | 502 | closed #18 |
| `feat/alchemist-peak-guard-trial-20261116` | 1 | 110 | closed #48 |
| `feat/alchemist-use-skelldizer-20261114` | 1 | 313 | closed #24 |
| `feat/deuce-v25-presence-native-trial-20261115` | 1 | 146 | closed #44 |
| `feat/preset-eq-hardware-overhaul-20261112` | 1 | 485 | closed #15 |
| `feat/preset-rack-workflow-ui-20261115` | 3 | 222 | closed #41 |
| `feat/preset-ui-alchemist-focus-20261114` | 1 | 454 | closed #19 |
| `feature/native-skellcore-adlibfx-rack` | 5 | 748 | open #8 |
| `feature/skelldizer-native-dsp` | 8 | 502 | closed #23 |
| `feature/skellworld-optin-presence` | 1 | 507 | merged #16 |
| `feature/studio-dynamics-delay-ui-20261121` | 4 | 0 | open #90 |
| `fix/alchemist-audition-window-alignment-20261118` | 6 | 56 | no PR |
| `fix/deuce-v25-native-trial-ci-20261116` | 3 | 146 | closed #45 |
| `fix/empty-rack-host-program-identity-55bank-20261120` | 1 | 35 | closed #83 |
| `fix/empty-rack-host-program-identity-20261120` | 33 | 502 | closed #82 |
| `fix/empty-rack-new-instance-default-20261119` | 28 | 502 | closed #81 |
| `fix/host-state-parameter-sanitize-20261118` | 11 | 55 | closed #72 |
| `fix/host-state-sanitize-55bank-20261119` | 4 | 42 | closed #78 |
| `fix/legacy-preset-macros-neutral-20261117` | 10 | 83 | closed #56 |
| `fix/preset-host-state-nan-sanitize-main-20261119` | 23 | 502 | closed #80 |
| `fix/preset-latency-cpu-stress-20261114` | 1 | 300 | closed #25 |
| `fix/preset-module33-import-20261114` | 4 | 316 | closed #33 |
| `fix/preset-package-neutral-legacy-20261118` | 26 | 502 | closed #67 |
| `fix/quick4-contact-sheet-flush-20261118` | 1 | 55 | closed #71 |
| `fix/skell-preset-single-extract-artifacts-20261118` | 3 | 97 | closed #64 |
| `fix/skell-preset-vst3-metadata-flat-artifact-20261118` | 15 | 502 | closed #69 |
| `improve/rack-pdc-polish-20261112` | 22 | 507 | closed #14 |
| `perf/alchemist-matchfinish-math-20261117` | 6 | 83 | closed #59 |
| `perf/alchemist-serial-gain-table-20261117` | 9 | 83 | closed #55 |
| `test/alchemist-reference-ab-controlled-density-20261120` | 4 | 2 | open #89 |
| `test/alchemist-reference-ab-serial-blend-20261118` | 2 | 49 | closed #74 |
| `test/alchemist-stage-cpu-profile-20261117` | 1 | 83 | closed #57 |
| `test/deuce-v25-full-state-recall-54bank-20261118` | 1 | 62 | closed #65 |
| `test/deuce-v25-full-state-recall-20261118` | 2 | 97 | closed #63 |
| `test/deuce-v25-native-trials-ctest-20261116` | 6 | 146 | closed #46 |
| `test/preset-rejected-import-atomicity-20261114` | 5 | 502 | closed #21 |
| `test/studio-eq-preset-roundtrip-20261117` | 1 | 70 | no PR |

## Follow-up gates
1. No branch may be deleted based only on its age or closed PR. Confirm exact unique code/assets, factory/automation ID parity, and saved FL Studio state.
2. Do not merge drafts #8, #89 or #90 without exact-head tests and the relevant audio/Quick4 gates.
3. GitHub main protection requires repository settings (the workflow edits in this PR cannot substitute).
4. No branches deleted and no PRs merged or closed by this audit.
