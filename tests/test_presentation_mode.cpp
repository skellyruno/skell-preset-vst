#include "SkellCore/PresentationMode.h"
#include <cstdio>
#include <type_traits>
#define SKELL_REQUIRE(x) do { if(!(x)){std::fprintf(stderr,"FAIL %s:%d %s\n",__FILE__,__LINE__,#x);return 1;} } while(false)
using namespace skellcore::ui;
static_assert(std::is_trivially_copyable_v<ViewState> && sizeof(ViewState) == 2,
              "ViewState must be exactly two enums: it cannot carry an audio parameter");
int main() {
    // Defaults are the lightest view.
    ViewState v; SKELL_REQUIRE(v.presentation == Presentation::Clean && v.complexity == Complexity::Simple);

    // Setters are independent and report change correctly.
    SKELL_REQUIRE(setPresentation(v, Presentation::Visual));
    SKELL_REQUIRE(!setPresentation(v, Presentation::Visual));
    SKELL_REQUIRE(v.complexity == Complexity::Simple);            // untouched
    SKELL_REQUIRE(setComplexity(v, Complexity::Advanced));
    SKELL_REQUIRE(!setComplexity(v, Complexity::Advanced));
    SKELL_REQUIRE(v.presentation == Presentation::Visual);        // untouched

    // Persistence round trip + hostile input falls back to the cheap defaults.
    for (auto p : {Presentation::Clean, Presentation::Visual}) SKELL_REQUIRE(presentationFromString(toString(p)) == p);
    for (auto c : {Complexity::Simple, Complexity::Advanced}) SKELL_REQUIRE(complexityFromString(toString(c)) == c);
    for (const char* bad : {"", "VISUAL", "Visual ", "2", "immersive", "\xff"}) {
        SKELL_REQUIRE(presentationFromString(bad) == Presentation::Clean);
        SKELL_REQUIRE(complexityFromString(bad) == Complexity::Simple);
    }

    // Exhaustive tier table: 2^5 = 32 contexts.
    int immersive = 0;
    for (int m = 0; m < 32; ++m) {
        ViewContext c;
        c.editorVisible = m & 1; c.rowVisible = m & 2; c.selected = m & 4;
        c.view.presentation = (m & 8) ? Presentation::Visual : Presentation::Clean;
        c.view.complexity   = (m & 16) ? Complexity::Advanced : Complexity::Simple;
        const auto t = tierFor(c);
        if (!c.editorVisible || !c.rowVisible) SKELL_REQUIRE(t == TelemetryTier::Off);   // hidden costs nothing
        else SKELL_REQUIRE(t != TelemetryTier::Off);
        SKELL_REQUIRE(needsAudioTap(t) == (t != TelemetryTier::Off));
        if (c.editorVisible && c.rowVisible && !c.selected) SKELL_REQUIRE(t == TelemetryTier::Compact);
        if (c.view.complexity == Complexity::Simple) SKELL_REQUIRE(t <= TelemetryTier::Compact);
        if (t == TelemetryTier::Immersive) {
            ++immersive;
            SKELL_REQUIRE(c.selected && c.view.complexity == Complexity::Advanced && c.view.presentation == Presentation::Visual);
        }
        // Visual never asks for LESS than Clean in the same context, and Clean never asks for more.
        ViewContext clean = c; clean.view.presentation = Presentation::Clean;
        ViewContext vis = c;   vis.view.presentation = Presentation::Visual;
        SKELL_REQUIRE(tierFor(vis) >= tierFor(clean));
        // Presentation only matters for a selected Advanced face.
        if (!(c.selected && c.view.complexity == Complexity::Advanced)) SKELL_REQUIRE(tierFor(vis) == tierFor(clean));
        SKELL_REQUIRE(updateHz(t) >= 0 && updateHz(t) <= 30);
        SKELL_REQUIRE((updateHz(t) == 0) == (t == TelemetryTier::Off));
    }
    SKELL_REQUIRE(immersive == 1);   // exactly one visible/row/selected/Advanced/Visual context
    std::puts("test_presentation_mode OK");
    return 0;
}
