# VoxRack DSP — authorized integration into Skell-Preset

The original VoxRack owner explicitly authorized reusing its source in
Skell-Preset. Source reference: `voxrack-main/apo/fx.h::FxDelay`.

## First port: tone-filtered delay feedback

`Source/chain/BasicDSP.h::Delay` now uses VoxRack's one-pole feedback
low-pass (500–18,000 Hz) and 150 Hz high-pass. The implementation is adapted
for two independent stereo lanes and retains Skell-Preset's preallocated ring
buffers and 20 ms read-head crossfades. This adds **zero algorithmic latency**.

The previously labeled but unused Filter knob now maps to the feedback tone.
The Adlib Throw Tone knob also colours repeats, without overwriting the
existing post-delay throw filter.

The port avoids VoxRack's APO registration, system microphone services and
shared memory; those are not appropriate inside a VST3. Future candidates
for benchmarking are its RBJ multi-band crossovers, gate detector and soft-knee
compressor. Any replacement must pass real vocal A/B tests and CPU profiling
before altering a user preset's sound.

## Validation

The portable regression test `SkellPresetVoxRackDelayTest` checks the first
echo's timing, bright/dark feedback differences, stereo independence and
NaN/Inf resistance at 44.1, 48 and 96 kHz. JUCE VST3 and real-editor builds
run separately in GitHub Actions; FL Studio host tests still need to be done.


## Second VoxRack source port

Added **VoxRack Room** from the owner's apo/fx.h::FxReverb: eight damped comb lines, four allpass stages and predelay with stereo decorrelation. Added **VoxRack Smooth Comp** from apo/dsp.h::stComp: stereo-linked soft-knee compression with level-dependent attack/release. Both are allocation-free during audio processing, have zero added host PDC, and are kept separate from the existing SkellCore FDN and Compressor. The wet room predelay is intentional creative delay rather than host PDC. Source tests: tests/test_voxrack_fx.cpp. Factories: VoxRack Velvet and VoxRack Chamber.
