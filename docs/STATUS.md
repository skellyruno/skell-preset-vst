# Build and feature status

As of the initial 2026-11-07 source archive:

| Component | State | Evidence / next gate |
| --- | --- | --- |
| JUCE 8 VST3/Standalone target | Source prepared; unverified compilation | Run Windows Actions workflow |
| Pin and link SkellCore v0.2 | Configured | CMake FetchContent commit fixed |
| Realtime 10 serial slots | Implemented in source; DAW untested | Run mono/stereo and automation tests |
| Eight sound processing types | Implemented in source, not release-audited | Benchmarks & listening A/B |
| Native factory bank browser | 24 factory settings + search | User banks / import/export / preview later |
| Four macro controls | Connected to DSP | Dynamic mapping later |
| UI 1168x688 + four sizes | Implemented in source | Visual UI/HiDPI regression on Windows |
| MATCH alignment / gain / polarity | Portable test passed | Multi-take and noise/drift tests |
| MATCH 3-module restricted fitter | Portable test passed >95% on synthetic same-take fixture | Generic free-form analysis still missing |
| MATCH wizard and real dry/wet WAV fitting | Not implemented | Worker/UI with cancel, waveform alignment |
| Native ZIP format | Design only | Implement secure archive validation |
| Adlibs, vocoder, pitch correction | Not implemented | Integrate validated SKELL backends |
| Parallel lanes / PDC / true-peak | Not implemented | Graph swap + latency tests |
| Host/plugin release validation | Not performed | Windows CI and FL Studio/Reaper tests |

Supported bus layouts in source: mono→mono, mono→stereo, and stereo→stereo. Stereo→mono is unsupported. In mono→stereo, the mono input feeds both effect channels. Host verification is pending.
