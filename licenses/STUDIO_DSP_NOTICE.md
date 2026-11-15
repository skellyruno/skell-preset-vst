# Skell Studio DSP import notice

## Studio Chorus (direct C++ port)

- Upstream: [Lemon Studio chorus](https://github.com/ItsAlphaNeon/Lemon-Studio/blob/main/crates/os-engine/src/effects/chorus.rs)
- Original copyright: Copyright (C) 2026 ItsAlphaNeon
- Upstream license: GPL-3.0-or-later
- Imported implementation: `Source/chain/StudioChorusDSP.h` (C++17 translation for Skell Preset)
- Modifications: Original Rust per-sample delay, crossover, three oscillator tables and fixed-seed stereo modulation translated into C++; 4-control mapping added; nonfinite-sample guards and table initialization restricted to prepare; no sample-thread allocation.
- Verification: `tests/test_studio_chorus.cpp` compares published Rust reference vectors and stresses sample rates, automation and extreme inputs.

The full GPL v3 license text is included in `licenses/StudioDSP-GPL-3.0.txt`.

This import changes the licensing obligations of any distributed combined
Skell Preset binary; do not ship a closed-source release of this combined
GPL implementation without appropriate rights/permissions. Keep the full
corresponding source and required copyright/license notices with a release.
Other dependencies and sources may carry additional licenses. Skell Studio
limiter and EQ were not yet directly ported, and this notice does not
represent them as completed.
