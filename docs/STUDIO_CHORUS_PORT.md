# Studio Chorus audio import A/B contract

The direct C++17 port uses Lemon Studio's actual three-LFO crossover chorus
algorithm and lookup tables. The DSP source includes its original
GPL-3.0-or-later attribution. `tests/test_studio_chorus.cpp` compares
sample vectors from the original Studio Rust source.

Important routing rule: the **Studio Chorus default is not wet-only**.
Its own processing combines 70% direct signal with the three modulated
tap outputs. The host slot's existing wet parameter crossfades the full
Studio Chorus output against the unprocessed input, preserving vocal onset.
Forcing the Studio engine into wet-only mode at 100% host wet suppresses
all direct consonants until the delay fills, and is not the correct sound.

The Studio default reports zero **added processing delay** to the host,
although modulated chorus taps are delayed by design. An audible delayed
tap is not the same thing as a PDC-required lookahead buffer.

This is not a claim to have directly ported Skell Studio's complete
Fruity Limiter or Parametric EQ 2 engines yet. A full copy requires their
compressor/lookahead-gate path and EQ mode-specific DSP (plain/HQ/LIN).
