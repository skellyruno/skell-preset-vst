#pragma once
// Presentation state for effect faces / rack rows.
//
// Clean vs Visual is PRESENTATION ONLY. It selects how much the UI draws and
// therefore how much telemetry work the audio thread is asked to do. It owns
// no audio parameter, never writes a parameter, and is independent of the
// Simple/Advanced complexity switch. Persist it as UI state, not as an
// automatable parameter, so toggling it can never move sound or automation.
#include <cstdint>
#include <string_view>

namespace skellcore::ui {

enum class Presentation : std::uint8_t { Clean = 0, Visual = 1 };
enum class Complexity   : std::uint8_t { Simple = 0, Advanced = 1 };

struct ViewState {
    Presentation presentation = Presentation::Clean;
    Complexity   complexity   = Complexity::Simple;
    friend bool operator==(const ViewState& a, const ViewState& b) noexcept {
        return a.presentation == b.presentation && a.complexity == b.complexity;
    }
};

// Each setter changes exactly one field and reports whether anything changed.
inline bool setPresentation(ViewState& v, Presentation p) noexcept {
    if (v.presentation == p) return false;
    v.presentation = p;
    return true;
}
inline bool setComplexity(ViewState& v, Complexity c) noexcept {
    if (v.complexity == c) return false;
    v.complexity = c;
    return true;
}

// Stable text form for state files. Unknown / missing -> safe defaults
// (Clean, Simple) so an old or corrupt file can never force a heavy view.
inline const char* toString(Presentation p) noexcept { return p == Presentation::Visual ? "visual" : "clean"; }
inline const char* toString(Complexity c) noexcept { return c == Complexity::Advanced ? "advanced" : "simple"; }
inline Presentation presentationFromString(std::string_view s) noexcept {
    return s == "visual" ? Presentation::Visual : Presentation::Clean;
}
inline Complexity complexityFromString(std::string_view s) noexcept {
    return s == "advanced" ? Complexity::Advanced : Complexity::Simple;
}

// How much measured telemetry a view needs. Off means the audio thread skips
// the tap entirely (EffectTelemetry::setConsumerActive(false)).
enum class TelemetryTier : std::uint8_t { Off = 0, Compact = 1, Graph = 2, Immersive = 3 };

struct ViewContext {
    bool editorVisible = false;   // plugin window open and not minimised/occluded
    bool rowVisible    = false;   // rack row actually on screen (not scrolled out)
    bool selected      = false;   // this effect's face is the open/selected one
    ViewState view;
};

inline TelemetryTier tierFor(const ViewContext& c) noexcept {
    if (!c.editorVisible || !c.rowVisible) return TelemetryTier::Off;
    if (!c.selected) return TelemetryTier::Compact;                  // rack-row meter only
    if (c.view.complexity == Complexity::Simple) return TelemetryTier::Compact; // Simple face: meters, no graphs
    return c.view.presentation == Presentation::Visual ? TelemetryTier::Immersive : TelemetryTier::Graph;
}
// UI refresh ceilings. 30 Hz everywhere until measured otherwise on a real host.
inline int updateHz(TelemetryTier t) noexcept {
    switch (t) { case TelemetryTier::Off: return 0; case TelemetryTier::Compact: return 15; default: return 30; }
}
inline bool needsAudioTap(TelemetryTier t) noexcept { return t != TelemetryTier::Off; }

} // namespace skellcore::ui
