# Shared SkellCore AdlibFX in Skell Preset

Four native rack effects are appended to the module type enumeration:
Adlib CHOP, Adlib MOTION, Adlib FILTER, and Adlib SATURATE.
Existing type values and per-slot automation IDs remain unchanged.

The engines are sourced from the exact tested SkellCore commit
cf2d182a1c4b1b45f97253f5e1b17dd91343aeaa, the same version
used by Skell AdlibLab 0.10. Each physical slot has its own processor,
prepared off the realtime callback. Existing slot wet/bypass smoothing
continues to apply; no nested VST3 dependency is required.

Slot controls 1–4: Amount, Rate, Tone, Mix. Host tempo drives CHOP.
Original factory presets and module numeric IDs are preserved.
Windows VST3 is the only plugin release format.

The historical feature PR reported portable effect tests at five sample
rates. Re-run the current repository's full Windows VST3 and DSP gates on
the consolidated commit; do not treat this historical report as current
validation.
