# Skell Preset — full factory catalog and UI/menu review
**Date:** 2026-11-23. **Source:** exact `FactoryPresets.h` on integration branch `2c722039`, before the documented browser polish follow-up. **Scope:** source-level inventory and UI interaction review. This is NOT a claim that all 55 audio presets were individually auditioned or their images visually inspected.

## Preserved host catalog
- **55/55** original fixed factory IDs, no duplicate names, no blank descriptions, all original Tone/Punch/Space/Grit values in [0,1], original cover colour IDs in 0–5. The oldest 43 are indexed 0–42, and the 12 later Alchemist/Deuce candidates are 43–54; all retained without renumbering.
- Metadata bank breakdown across all 55: **30 Main Vocals**, **17 Adlibs**, **8 Vocoding**. The 55-entry test in safeguards PR #91 independently pins every bank/name ID, enforces original first-43 split **18/17/8**, no duplicates and range-valid metadata.
- User-approved **curated Main Vocals browser** deliberately offers only **Alchemist (host index 43)** and **Deuce v2.5 (host index 46)**, renamed for browsing but not at the API/host level. The remaining Main Vocals and Vocoding entries remain loadable by historical host program IDs rather than presented as 30 indistinguishable launch choices. Do not expand the curated browser or delete historical entries without explicit product approval.
- **Main Vocals → ADLIBS** indented submenu renders the 17 factory Adlibs (plus any personal presets tagged `Ad-libs`). **My Presets**, **Artists**, and **Favorites** depend on saved user presets/sidecar metadata and do not alter factory host IDs.
- Full factory covers are deterministic native vector motifs with 4 safe accent indices (no filesystem access during paint). User images are loaded, bounded and cached separately. Current UI browser is not 55 painted cards at once and must not be called 55 inspected screenshots.

## Menu and visual behaviors examined / improved
| Surface | Source review / change | Remaining release gate |
|---|---|---|
| Main/ADLIBS/My Presets/Artists/Favorites | Bank and ADLIBS hit boxes already share their painting geometry; added sidebar hover, pointer cursor, stable keyboard Enter on highlighted card | Real pointer/keyboard focus screenshots at min/max scale |
| Search + genre + artist/category editors | Search is filtered on message thread; history and existing sidecars preserved; details controls have fixed bounds checked by UI smoke | Long user filenames, non-Latin text and native popup scaling |
| Factory/personal cover editor | 8 MB and 4096px image guards, temporary-file atomic cover update; no painter disk I/O | User-provided art loading, letterboxing and preview on Windows |
| Effect Library | Native category search, fuzzy terms, keyboard arrows, resize/drag; corrected row click gaps, VST3 footer hit zone and close hover | Actual Windows screenshot and wheel/keyboard navigation |
| Installed VST3 browser | Incremental directory-only name scan. **Fixed selection drift** when another bundle sorts before currently selected row; reduced unnecessary redraws | Test with dozens of installed plugins while scanning, cancelled chooser |
| Settings | True keyboard focus/arrow/Enter/Escape, hover states, and vector close icon | Real theme/scale focus ring and hit-test inspection |
| MIC, EQ, limiter, delay, Vocal Feel Suite | Native parameter editor and frozen automation IDs; Studio Limiter test now distinguishes compact and MORE states | Host automation, pluginval and Quick4 full effect-page audit |
| Live performer | Real post-FX waveform, pre-rack and post-FX peaks, measured waveform RMS; stale LIVE state cleared by UI timer when processing stops | Real host start/stop, pause/resume, bypass, 70–125% captures |

## Every factory host entry (source-verified)
| Host ID | Factory name | Bank | Source description | Cover colour ID |
|---:|---|---|---|---:|
| 0 | Clean Lead | Main Vocals | Balanced modern vocal | 0 |
| 1 | Dark Trap Lead | Main Vocals | Warm focus, tight dynamics | 1 |
| 2 | Airy Pop | Main Vocals | Open high-frequency sheen | 2 |
| 3 | Gritty Rap | Main Vocals | Forward and aggressive | 3 |
| 4 | Warm R&B | Main Vocals | Smooth controlled intimacy | 4 |
| 5 | Bright Radio | Main Vocals | Presence with restraint | 5 |
| 6 | Bedroom Intimate | Main Vocals | Close, natural and soft | 0 |
| 7 | Aggressive Hook | Main Vocals | Hard punch, wide presence | 2 |
| 8 | Demon Adlibs | Adlibs | Dark fuzz and low space | 1 |
| 9 | Ghost Echo | Adlibs | Airy trailing repeats | 4 |
| 10 | Radio Adlib | Adlibs | Narrow grit in the middle | 3 |
| 11 | Wide Stack | Adlibs | Layer-ready wide room | 0 |
| 12 | Stutter Throw | Adlibs | Tempo echoes | 5 |
| 13 | Alien Pulse | Adlibs | Unusual bright resonance | 2 |
| 14 | Whisper Trail | Adlibs | Soft intimate repeats | 4 |
| 15 | Distorted Shout | Adlibs | Shout-ready punch | 3 |
| 16 | Robot Chorus | Vocoding | Electronic robotic colour | 5 |
| 17 | Harmony Pad | Vocoding | Ambient vocal foundation | 4 |
| 18 | Talkbox Glide | Vocoding | Bright talking colour | 2 |
| 19 | Neon Droid | Vocoding | Synthetic shimmer | 1 |
| 20 | Chrome Choir | Vocoding | Wide metallic glow | 5 |
| 21 | Digital Formant | Vocoding | Resonant vocal colour | 3 |
| 22 | Phase Broadcast | Vocoding | Filtered broadcast | 0 |
| 23 | Spectral Glow | Vocoding | Soft futuristic layers | 4 |
| 24 | Skell Vocal Polish | Main Vocals | Natural de-essing and smooth finish | 2 |
| 25 | Skell Live Lead | Main Vocals | Zero-lookahead vocal tracking chain | 0 |
| 26 | Broadcast Throw | Adlibs | Radio bite with VoxRack-filtered phrase throws | 3 |
| 27 | Digital Bones | Adlibs | Bit-crushed vocal and rhythmic VoxRack echoes | 1 |
| 28 | Three-Band Silk | Main Vocals | Controlled multiband focus and subtle width | 2 |
| 29 | VoxRack Velvet | Main Vocals | Soft-knee dynamics and a room-sized vocal finish | 4 |
| 30 | VoxRack Chamber | Adlibs | VoxRack room reflections with phrase-end echoes | 5 |
| 31 | Wide Choir Adlib | Adlibs | Lush moving vocal layers and stereo chorus | 0 |
| 32 | Phantom Sweep | Adlibs | Phased comb motion for energetic adlib sends | 5 |
| 33 | Alien Ring | Adlibs | Metallic ring voice with dark phrase throws | 1 |
| 34 | Rider Ready | Main Vocals | Steady phrase levelling before compression | 0 |
| 35 | Ducked Echo | Adlibs | Tempo-locked stereo repeats that duck under the vocal | 2 |
| 36 | Skell Flutter | Adlibs | Expressive stereo vibrato with soft throws | 4 |
| 37 | Eight-Band Air | Main Vocals | Eight individually tuned vocal frequency bands | 2 |
| 38 | Opto Punch | Main Vocals | Fast FET peaks into slow optical leveling | 0 |
| 39 | Smooth Edge | Main Vocals | Dynamic resonance tamer in the presence region | 4 |
| 40 | Noise Guard | Main Vocals | Gentle adaptive background-noise floor control | 1 |
| 41 | Formant Sketch | Adlibs | Creative spectral pitch/formant colour; studio PDC | 5 |
| 42 | Studio Six | Main Vocals | Six native Skell rack stages: EQ, comp, drive, delay, reverb and chorus | 0 |
| 43 | Alchemist (Native Preview) | Main Vocals | Experimental vocal match: EQ, FET/Opto-role dynamics and gentle multiband colour | 4 |
| 44 | Deuce D/Max (Native Trial) | Main Vocals | Editable Deuce REC to MAIN native chain based on documented references | 5 |
| 45 | Alchemist Refined (Trial) | Main Vocals | Protected Alchemist preview with five opt-in causal polish refinements | 4 |
| 46 | Deuce v2.5 (Native Trial) | Main Vocals | Preferred v2.5 reference: native REC/MAIN approximation with 7 kHz correction | 5 |
| 47 | Alchemist Peak Guard (Trial) | Main Vocals | Alchemist refinement plus 4x interpolated-peak guard (~-0.97 dB, 3 ms PDC; not BS.1770 certified) | 4 |
| 48 | Alchemist Tone Match (Trial) | Main Vocals | Measured 2.93k +1.0dB / 5.1k -0.6dB / 12k -1.1dB correction plus peak guard; 3 ms PDC | 4 |
| 49 | Alchemist Lighter Dynamics (Trial) | Main Vocals | Factory 48 tone match with reduced fast/slow compressor wet blend for envelope A/B | 4 |
| 50 | Alchemist Fast Compressor Blend (Trial) | Main Vocals | Factory 48 tone match with only fast compressor wet reduced to 20%; smooth stays 70% | 4 |
| 51 | Alchemist Smooth Compressor Blend (Trial) | Main Vocals | Factory 48 tone match with only smooth compressor wet reduced to 50%; fast stays 35% | 4 |
| 52 | Alchemist Serial Dynamics (Trial) | Main Vocals | Factory 48 tone match with native FET peak catcher and slower RMS leveling; 3 ms peak guard | 4 |
| 53 | Alchemist Low-End Contour (Trial) | Main Vocals | Measured 20–120 Hz correction: 30 Hz high-pass and neutral 70 Hz bell; retains serial dynamics and peak guard | 4 |
| 54 | Alchemist Serial Dynamics Blend (Trial) | Main Vocals | Factory 53 contour with -27 dB serial threshold and 45% FET/RMS wet blend; no makeup gain | 4 |

## Release decisions / preservation
No bank/index, factory parameter, VST3 identity, automation ID, native preset schema, waveform DSP producer or sound was edited in this UI pass. The older Alchemist trials are for preservation and controlled A/B, **not** promoted as the current approved Main Vocals launch choices. No branch or old preset is safe to delete simply because its UI entry is hidden. Local Quick4 screenshots, Windows pluginval, FL Studio recall, measured CPU/PDC and same-take listening must be independently recorded before a stable release.
