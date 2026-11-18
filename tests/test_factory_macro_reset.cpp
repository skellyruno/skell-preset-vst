#include "PluginProcessor.h"
#include "chain/PresetPackage.h"
#include <cmath>
#include <iostream>
#include <memory>

namespace {
bool macrosAreNeutral(const SkellPresetAudioProcessor& processor) {
    for (int i=1;i<=8;++i) {
        const auto* value=processor.state.getRawParameterValue(
            "macro"+juce::String(i));
        if (value==nullptr || std::abs(value->load()-.5f)>1.0e-6f)
            return false;
    }
    return true;
}
bool dirtyMacros(SkellPresetAudioProcessor& processor) {
    for (int i=1;i<=8;++i) {
        auto* parameter=processor.state.getParameter("macro"+juce::String(i));
        if (parameter==nullptr)
            return false;
        parameter->setValueNotifyingHost(.07f*static_cast<float>(i));
    }
    return true;
}
}

int main() {
    std::cerr << "STEP 0: main entered\n" << std::flush;
    // This regression exercises APVTS and the processor state only. It does
    // not construct an editor or use message-loop services, so keep it truly
    // headless on Windows CI rather than starting the GUI subsystem.

    // The processor owns sixteen fully constructed DSP engine slots, including
    // reverb and delay state. Keep it off the small default Windows test stack.
    auto processor=std::make_unique<SkellPresetAudioProcessor>();
    std::cerr << "STEP 2: processor constructed on heap\n" << std::flush;

    if (!dirtyMacros(*processor)) {
        std::cerr << "FAIL: all eight serialized macro parameters exist\n";
        return 1;
    }
    std::cerr << "STEP 3: macros dirtied\n" << std::flush;
    processor->loadFactoryPreset(0);
    std::cerr << "STEP 4: factory 0 loaded\n" << std::flush;
    if (!macrosAreNeutral(*processor)) {
        std::cerr << "FAIL: factory 0 did not reset all eight macros\n";
        return 2;
    }

    const auto file=juce::File::getSpecialLocation(juce::File::tempDirectory)
        .getNonexistentChildFile("skell-factory-macro-reset",".skellpreset",false);
    std::cerr << "STEP 5: defaults checked\n" << std::flush;
    const auto saved=processor->exportPreset(file);
    std::cerr << "STEP 6: export returned\n" << std::flush;
    if (saved.failed()) {
        std::cerr << "FAIL: factory preset export: " << saved.getErrorMessage() << '\n';
        return 3;
    }

    skellpreset::package::Document document;
    const auto loaded=skellpreset::package::read(file,document);
    std::cerr << "STEP 7: package read returned\n" << std::flush;
    file.deleteFile();
    if (loaded.failed()) {
        std::cerr << "FAIL: exported factory package could not be read\n";
        return 4;
    }
    for (float value:document.macros) {
        if (std::abs(value-.5f)>1.0e-6f) {
            std::cerr << "FAIL: exported factory retained hidden macro state\n";
            return 5;
        }
    }

    if (!dirtyMacros(*processor)) {
        std::cerr << "FAIL: macro parameters disappeared after export\n";
        return 6;
    }
    std::cerr << "STEP 8: export verified\n" << std::flush;
    processor->loadFactoryPreset(24);
    std::cerr << "STEP 9: factory 24 loaded\n" << std::flush;
    if (!macrosAreNeutral(*processor)) {
        std::cerr << "FAIL: second factory switch did not reset all eight macros\n";
        return 7;
    }

    std::cout << "PASS: factory switching resets all eight macros and export preserves neutral state\n";
    return 0;
}
