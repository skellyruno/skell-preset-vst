# Hierarchical preset browser — 2026-11-23

Build baseline: 7c1eccab94750ec6a0d0fced01dcc713fce8c46a, Windows/portable CI 45318506274 passed, including native UI, full-rack PDC/stress, hosted VST3, and preset package regression. Previous verified test ZIP: artifact 14888195253.

This change introduces actual clickable Genres, artist tiers, and artist directory cards, with per-group Main/Adlib/Unassigned counts, cached existing user artwork, keyboard navigation and back navigation. Category clicks never audition a sound, and typed searches still resolve to preset results. Directory tests exercise tier → artist → preset → back, plus genre navigation without writing user files or loading audio.

No factory, DSP or APVTS changes. Manual labels remain in the existing library sidecars. No inferred artist identities or new third-party artwork. Local Quick4 remains blocked by unavailable GitHub DNS/JUCE sources in this runtime; actual PNG inspection and FL Studio listening/recall remain separate release gates. The current revision requires exact-head Windows CI.
