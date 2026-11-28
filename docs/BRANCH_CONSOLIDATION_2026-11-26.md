# Branch consolidation — 2026-11-26

Owner authorized branch retirement and consolidation. Primary draft #90: fd0465f888c905a4eaafa1c8410c1a3d60774f54. Exact-head portable and Windows CI passed before this batch (45813578157, 45813578147). Current batch requires new CI; no release/production merge.

Recovered seven previously absent files from closed preview #96 (2b5db26c14510db36f099a2ba7556753bee40165): offline FL raw-state codec, standalone inspector, inventory audit, historical research notes, peak-history utility and two regressions. Register both tests and build the inspector in portable CMake. No editor or shipping DSP wiring, preset IDs or factory catalog changed. Historical real-payload validation in R29 notes is prior evidence, not re-run here.

#92 AdlibFX tip ccd27d7 is reachable from #90 (0 ahead), safe for ref retirement after rechecking open PR dependencies. #91 safeguards PR is merged into main; its original tip is divergent due to integration history. Do not prune that ref until tree equivalence/ancestry is established. #94 MIC Club, #95 release polish, #96 preview and #97 external bridge are closed but retain divergent commits; selectively reconcile remaining source before deleting. No old factory catalog restoration; count is not a fixed requirement. Do not enable experimental nested plugin hosting by accident.

Branch deletion and maintenance dispatch are not available through the current connector. Local GitHub DNS and full JUCE checkout unavailable. Do not report deletion or local UI verification until actually performed.
