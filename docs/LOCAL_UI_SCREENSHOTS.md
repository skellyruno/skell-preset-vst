# Skell-Preset Quick4 local screenshot workflow

This repo owns the actual SkellPresetUIRenderer target and its scene definitions
in tests/ui_renderer.cpp. Its default is four current-source screenshots. No screenshot GitHub workflow is required.

From the reusable SkellCore repository checkout:

Windows Developer PowerShell:
    .\quick4.ps1 -Plugin skell-preset-vst -Ref feature/studio-dynamics-delay-ui-20261121 -Update

Windows repeat with source cached:
    .\quick4.ps1 -Plugin skell-preset-vst -Offline

Linux:
    bash quick4.sh --ref feature/studio-dynamics-delay-ui-20261121 --update

Keep the persistent ~/Skelly/Quick4 workspace. It reuses a cached JUCE
checkout, exact Skell-Preset Git source and Ninja build objects. The output
captures/skell-preset-vst directory contains exactly four PNGs, a contact
sheet, manifest.json, local-build-report.json and a SHA-stamped ZIP
suitable for uploading in a separate chat.

Manual deep audit: use -Full in PowerShell / --full on Linux, which captures all native module faces, stage and library directories. The local
renderer also accepts `--full --factory-slots` for every occupied factory rack slot.
Browser QA scenes use explicit in-memory fixture metadata, never saved personal data. The portable test runner and Windows VST3
build must pass separately. No fresh native screenshot is claimed by this
documentation; render and inspect actual PNGs before claiming the current
UI has been verified.
