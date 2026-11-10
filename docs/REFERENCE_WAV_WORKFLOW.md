# Skell-Preset — reference WAV intake (planned MATCH workflow)

The goal is to recreate **your own** FL Studio main-vocal and adlib chains with
measurable, repeatable comparisons. WAVs are not native .skellpreset files.
Audio matching needs an analysis / optimization pass before a preset is generated.

For each sound you eventually provide:

- `<name>_DRY.wav`: exactly the input to the original FL mixer chain.
- `<name>_WET.wav`: the same performance, same sample start, through that chain.
- Optional `<name>_FX_ONLY.wav`: 100% wet delay/reverb sends or throws.
- Optional plugin-chain screenshots or exported FL .fst states, including routing,
  sidechains, BPM, key/scale and unusual pitch/formant settings.
- Prefer lossless 24-bit or 32-bit float WAV, common sample rate (44.1/48 kHz),
  no normalization, no independent time stretch, and leave reverb tails intact.

Evaluation must align the recordings, estimate latency, trim common silence,
measure level / spectral / temporal and stereo differences, and compare withheld
phrases. A preset should not be labelled “exact” without an A/B null or feature
benchmark with an audible tolerance. Nonlinear, stochastic, sidechained, pitch
corrector and third-party proprietary processing may not be reconstructable
from a single dry/wet pair.

**Current capability:** the tested deterministic MATCH alignment/level harness
and native .skellpreset import/export exist. Automated WAV-driven full-chain
fitting, FL .fst parsing, reference-file UI import and sound-equivalence claims
are *not* implemented yet. Keep this note accurate until wired and tested.

## Offline WAV intake diagnostic

The first validated preparation utility is `tools/reference_wav_inspect.py`.
It supports RIFF PCM16/24/32 and float32 (mono or stereo), verifies rate/chunk
integrity, checks peak/RMS and approximate polarity-agnostic sample alignment,
and writes a JSON report. It does **not** load sounds inside the plugin.

```sh
python tools/reference_wav_inspect.py Dry_Main.wav Wet_Main.wav --output main-reference.json
python -m unittest discover -s tests -p test_reference_wav_inspect.py -v
```

A low waveform correlation can be completely legitimate for pitch-shifted,
formant-changed, highly nonlinear or layered adlibs. This tool is a quality
screen before fitting, not a fidelity percentage or automatic chain creator.
