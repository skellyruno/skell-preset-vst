# UI follow-up / build repair (draft #90)

Baseline cb40c4682031b7111fc45d262fbb50d0317d3d4f: run 45329809393 portable tests passed; Windows shipping build failed C2662/C2088 at PluginEditor.cpp:559 (const JUCE Slider calling non-const getTextFromValue). Fixed the regression check by invoking the installed const-callable formatter. Kept live label implementation, added zero margins and host-notified endpoint readout checks. Master controls remain Simple-only.

Actual effect control improvements: compact Studio Limiter now expands its two visible controls to fill the deck, with MORE retaining the four original parameters. Filter and radio faces use precise paired sliders; three-band and serial dynamics use matched rotary controls. Existing IDs/DSP unchanged.

Library: separated thumbnails from names and favorite controls, removed artwork wash behind text, widened labels, protected sparse artwork indexing. Tier pages have a larger 2-column/2-row grid; directional keys navigate focused cards, Alt+Left/Right switch sections. Added focused grid regression. No change to preset files.

Local JUCE 8.0.12 git ls-remote attempted: DNS could not resolve github.com. No screenshots captured or inspected. Exact-head Windows compilation, native/DSP/host checks and local real-editor visual validation remain required. Do not distribute prior preview as this build.
