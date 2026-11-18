#include "PluginProcessor.h"
#include "chain/PresetPackage.h"
#include <cmath>
#include <iostream>

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
    SkellPresetAudioProcessor processor;

    if (!dirtyMacros(processor)) {
        std::cerr << "FAIL: all eight serialized macro parameters exist\n";
        return 1;
    }
    processor.loadFactoryPreset(0);
    if (!macrosAreNeutral(processor)) {
        std::cerr << "FAIL: factory 0 did not reset all eight macros\n";
        return 2;
    }

    const auto file=juce::File::getSpecialLocation(juce::File::tempDirectory)
        .getNonexistentChildFile("skell-factory-macro-reset",".skellpreset",false);
    const auto saved=processor.exportPreset(file);
    if (saved.failed()) {
        std::cerr << "FAIL: factory preset export: " << saved.getErrorMessage() << '\n';
        return 3;
    }

    skellpreset::package::Document document;
    const auto loaded=skellpreset::package::read(file,document);
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

    if (!dirtyMacros(processor)) {
        std::cerr << "FAIL: macro parameters disappeared after export\n";
        return 6;
    }
    processor.loadFactoryPreset(24);
    if (!macrosAreNeutral(processor)) {
        std::cerr << "FAIL: second factory switch did not reset all eight macros\n";
        return 7;
    }

    std::cout << "PASS: factory switching resets all eight macros and export preserves neutral state\n";
    return 0;
}
