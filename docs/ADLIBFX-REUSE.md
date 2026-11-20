# Shared SkellCore AdlibFX proposal for Skell Preset

**Status: not included in the current consolidated production candidate.**
PR #8 remains open as a separate feature review. The current integration
retains the existing rack modules and IDs; it does not yet append the four
AdlibFX modules described below. This document preserves the proposal and
its original technical constraints without claiming those effects ship.

## Proposed effects

- Adlib CHOP
- Adlib MOTION
- Adlib FILTER
- Adlib SATURATE

The original PR proposed sourcing these engines from the pinned SkellCore
revision cf2d182a1c4b1b45f97253f5e1b17dd91343aeaa using the headers-only
AdlibFX component. The intended implementation has one processor per physical
rack slot, prepared off the realtime callback, and retains the plugin's
existing slot wet/bypass smoothing.

Proposed controls 1–4: Amount, Rate, Tone, Mix. CHOP would use valid host tempo
with a safe fallback. No nested VST3 dependency is intended.

## Integration gates before merging PR #8

1. Append module IDs only; never renumber existing module types or automation IDs.
2. Verify independent per-slot DSP state and no allocation/locking in processBlock.
3. Run portable DSP tests at supported sample rates and Windows x64 VST3 CI.
4. Measure CPU, latency, denormals, clipping and stereo stability.
5. Verify all four real editor controls and scaling with local Quick4 screenshots.
6. Keep Windows VST3 as the supported release format.

The original PR's historical test report is not current validation. Do not
close PR #8 or describe these effects as shipped until the implementation is
reviewed and the current gates pass.
