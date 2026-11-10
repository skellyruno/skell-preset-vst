# Stage 7 — realtime gate and EQ display
This revision keeps the existing VST3 plugin ID, automation parameters, factory program IDs and native preset schema.
Gate Hold and Hysteresis now work in the DSP; the EQ display is an analytical response of live Biquad coefficients.
Time-FX state continues advancing while bypassed to prevent frozen feedback returning later.
The preset browser has card hover emphasis and a scrollbar. Its selection stays open.
Portable validation: 6/6 tests passed locally (44.1/48/96 kHz). Windows JUCE compile and FL Studio host playback require independent checks.
Limitations: the EQ still has one bell plus one high-pass; module-type replacement is not yet click-free and lanes are not active.
