# Skell-Preset Stage 6 — UI and workflow refinement

## Implemented

- Three original UI palettes: Obsidian/Neon (default), Ivory/Cream, Crimson/Red. Click the **vector winged skull** at top-left to choose a palette. The selection is stored as `uiTheme` in the processor ValueTree (UI-only state, not an automatable parameter).
- Rack modules can now be reordered by dragging **anywhere on a row except the on/off status dot**. The drag starts only after a 6-pixel threshold; clicking a row merely selects it. Status dots keep their bypass action, and right-click still opens the module menu.
- The `+ ADD MODULE` footer places a new module into the first unoccupied slot, preserving the existing 10-slot/16-generic-parameter APVTS layout and old host automation IDs. When all ten are occupied, the footer says `RACK FULL`. This is a convenience action, **not** arbitrary dynamic slot creation beyond 10.
- Preset browser stays open after factory preset selection and native preset import. The selected card gets an `ACTIVE` indicator. Existing click-again, outside-click, and Escape dismissal remain. The six repetitive raster skulls were replaced by a light themed waveform motif; the original assets are retained in the package for backwards compatibility.
- The Bypass button is now **a reversible toggle**. The prior Simple or Advanced workspace remains visible but visibly dimmed and locked while latency-aligned bypass DSP runs. Clicking Bypass again restores the previous workspace. UI-only `uiPreviousMode` saves the last workspace.
- Accurate logarithmic EQ Hz tick placement, clearer labels for the **one currently implemented EQ band**, aligned lower strip, cleaned-up navy overlays, and theme-consistent controls.
- Lightened UI resource usage: preset covers are no longer decoded into per-editor images; no animated glow or per-frame raster blur is introduced.

## Important limitations

- The rack still supports ten slots. Adding more than ten would require a versioned expansion of host automation and native preset schemas.
- UI colour themes are independent of audio parameters; applying a theme does not affect the sound.
- The full MATCH engine, parallel lanes, additional EQ bands, and FL Studio host validation are **not** claimed complete.
- Screenshot/visual QA must be completed by opening the compiled plugin in FL Studio on a Windows machine. Portable tests cannot simulate host DPI/font rendering.

## Manual FL Studio acceptance checklist

1. Open two Skell-Preset instances and choose different themes; ensure both remain correctly coloured.
2. Reload the DAW project; verify `uiTheme` and last workspace are restored.
3. Drag a row from its label/icon to a new position and release; confirm audio-chain ordering changes. Single click must not reorder.
4. Toggle the status dot and use the right-click menu; neither should accidentally initiate a drag.
5. Click `+ ADD MODULE` with an empty slot, choose an effect, and verify it becomes active; fill all 10 slots and confirm no 11th is created.
6. Open browser and click multiple presets: menu remains open, active card updates, scroll/search persist. Click selector again to close.
7. Enter bypass from Simple and Advanced, verify both are dimmed and audio remains dry and latency-aligned, and click bypass again to restore.
8. Switch between 70%, 85%, 100% and 125%, inspect spacing and text clipping across all three themes.
