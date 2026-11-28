# Consolidated UI audit (draft PR #90)

Baseline 30d27c8bcd2f3ece6ee2e125b50fd59bf895f8ee: Windows VST3 compiled, packaged and preview uploaded on run 45327635503; portable suite passed. Native tests were still in progress at source audit. Do not infer a passing native test or current screenshots from the preview.

- Browser: guard search Enter during management; keep search availability synchronized; reset favorite focus after sorting; use actual visible-row count in clamp and keyboard regression; artist-leaf sorting now honors the sort selector; fix cursor reset and narrow-window text. Real vocal tab callbacks tested against the paired in-memory fixture.
- De-Esser: move overlapping graph labels to the header rail; give maximum-reduction response explicit 0/-6/-12/-18/-24 dB guide lines and scale; use effective DSP cutoff for plotted marker; reduce path point count. No processing or parameter identity changes.
- Local renderer: default Quick4 and explicit full mode, coverage assertion for every module type, stage, browser min/max and directory/leaf scenes, optional every occupied factory slot by stable recipe ID. QA fixture artist/genre labels live only in memory during captures; no metadata files created. No screenshot Actions triggered.
- Local JUCE 8.0.12 fetch attempted again: GitHub DNS unavailable. Native screenshots and visual review remain blocked here. Exact-head Windows/native/DSP/host/CPU gates remain separate. No claim of completion until captures are inspected.
