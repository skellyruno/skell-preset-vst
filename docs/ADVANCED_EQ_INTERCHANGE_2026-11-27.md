# Advanced Studio EQ — 2026-11-27

- Existing Studio EQ has eight bands, seven shapes, 12/24/36 dB/oct cut slopes, and independently adjustable linked stereo dynamic bell attenuation, threshold, attack and release. No new lookahead. Gain-reduction readout is the current smoothed dynamic filter's centre attenuation, not inferred from peak levels. Defaults reproduce earlier static behavior. Existing choice parameter remains unchanged; an appended extra-shape selector enables notch/band-pass.
- Use the existing native .skellpreset import browser for converted settings. The converter is deliberately offline and new-file-only.
- User-provided research: FabFilterStates.cpp (fromProQ4, FFBS v1 600-value layout), translateWavesReq.cpp (REQ RealWorld values), translateWavesApi550.cpp (550A saved values), ParametricEqFl.cpp/.h. No vendor binaries executed. No private presets committed.
- Pro-Q 4: raw component FFBS state only; not general .ffp or other Pro-Q versions. Experimental layout-based settings conversion; actual independent saved Pro-Q 4 fixture not yet verified. Reject non-stereo, >8 bands, unsupported shapes, non-zero-latency processing, non-neutral character/gain scale, positive dynamic ranges, auto threshold. Timing approximated from measured reference law. Spectral dynamics not supported.
- Waves: strict one XML tree, active SETUP_A/B RealWorld selection, REQ and API-550A named states only. Native settings approximation, not vendor tone equivalence; proprietary analog curves/Q laws differ. 550A cut filters/550B rejected.
- FL EQ2: notch and band-pass now translated; source slope/order remains an explicit warning until validated.
- `python tools/import_vendor_eq.py STATE --format pro-q4 --allow-approximate --output NEW.skellpreset`
- `python tools/import_vendor_eq.py MIXER.fst --format waves --fst-record N --allow-approximate --output NEW.skellpreset`
- Run `python -m unittest discover -s tools -p 'test_import*eq.py'` for vendor converter tests. Synthetic fixture tests validate parser/mapping only; a real source comparison and destination host roundtrip are separate release gates.
- Do not merge until Windows host/state and exact-commit native Quick4 validation passes. Shared standalone EQ wrapper should reuse this engine rather than create a divergent implementation.
