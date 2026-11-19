#include "PluginProcessor.h"
#include "chain/PresetPackage.h"
#include <cmath>
#include <iostream>
#include <memory>

namespace {
constexpr float tolerance = 1.0e-6f;

float dirtyMacroValue(int index) {
    return .07f * static_cast<float>(index);
}

bool macroParameterContractIsStable(const SkellPresetAudioProcessor& processor) {
    // These IDs are the published automation contract. They must remain
    // discoverable, normalized 0..1 host parameters after state restoration.
    for (int i=1;i<=8;++i) {
        const auto id="macro"+juce::String(i);
        const auto* parameter=processor.state.getParameter(id);
        const auto* value=processor.state.getRawParameterValue(id);
        if (parameter==nullptr || value==nullptr)
            return false;
        const auto range=parameter->getNormalisableRange();
        if (std::abs(range.start)>tolerance ||
            std::abs(range.end-1.f)>tolerance ||
            std::abs(parameter->getValue()-value->load())>tolerance)
            return false;
    }
    return true;
}

bool macrosAreNeutral(const SkellPresetAudioProcessor& processor) {
    for (int i=1;i<=8;++i) {
        const auto id="macro"+juce::String(i);
        const auto* value=processor.state.getRawParameterValue(id);
        const auto* parameter=processor.state.getParameter(id);
        if (value==nullptr || parameter==nullptr ||
            std::abs(value->load()-.5f)>tolerance ||
            std::abs(parameter->getValue()-.5f)>tolerance)
            return false;
    }
    return true;
}

bool dirtyMacros(SkellPresetAudioProcessor& processor) {
    for (int i=1;i<=8;++i) {
        auto* parameter=processor.state.getParameter("macro"+juce::String(i));
        if (parameter==nullptr)
            return false;
        parameter->setValueNotifyingHost(dirtyMacroValue(i));
    }
    return true;
}

bool macrosMatchDirty(const SkellPresetAudioProcessor& processor) {
    for (int i=1;i<=8;++i) {
        const auto id="macro"+juce::String(i);
        const auto* parameter=processor.state.getParameter(id);
        const auto* value=processor.state.getRawParameterValue(id);
        if (parameter==nullptr || value==nullptr ||
            std::abs(value->load()-dirtyMacroValue(i))>tolerance ||
            std::abs(parameter->getValue()-dirtyMacroValue(i))>tolerance)
            return false;
    }
    return true;
}
}

int main() {
    std::cerr << "STEP 0: main entered\n" << std::flush;
    // This regression exercises APVTS and processor state only. It does not
    // construct an editor or use message-loop services, so keep it headless.
    // The processor owns sixteen DSP engine slots; keep it off the small
    // default Windows test stack.
    auto processor=std::make_unique<SkellPresetAudioProcessor>();
    std::cerr << "STEP 2: processor constructed on heap\n" << std::flush;

    if (!macroParameterContractIsStable(*processor) || !macrosAreNeutral(*processor)) {
        std::cerr << "FAIL: eight published macro IDs must exist with 0..1 ranges and neutral defaults\n";
        return 1;
    }

    // Host-session roundtrip: distinct values must survive get/setStateInformation
    // without changing the IDs, ranges, or normalized automation values.
    if (!dirtyMacros(*processor)) {
        std::cerr << "FAIL: all eight serialized macro parameters exist\n";
        return 2;
    }
    juce::MemoryBlock hostState;
    processor->getStateInformation(hostState);
    auto hostRestored=std::make_unique<SkellPresetAudioProcessor>();
    hostRestored->setStateInformation(hostState.getData(),
                                       static_cast<int>(hostState.getSize()));
    if (!macroParameterContractIsStable(*hostRestored) ||
        !macrosMatchDirty(*hostRestored)) {
        std::cerr << "FAIL: host state save/reload changed macro automation values or ranges\n";
        return 3;
    }
    std::cerr << "STEP 3: host state roundtrip verified\n" << std::flush;

    // Factory switching must clear stale values in all eight serialized
    // macro controls, including hidden compatibility slots 5..8.
    hostRestored->loadFactoryPreset(0);
    std::cerr << "STEP 4: factory 0 loaded\n" << std::flush;
    if (!macrosAreNeutral(*hostRestored)) {
        std::cerr << "FAIL: factory 0 did not reset all eight macros\n";
        return 4;
    }

    const auto file=juce::File::getSpecialLocation(juce::File::tempDirectory)
        .getNonexistentChildFile("skell-factory-macro-reset",".skellpreset",false);
    const auto saved=hostRestored->exportPreset(file);
    if (saved.failed()) {
        std::cerr << "FAIL: factory preset export: " << saved.getErrorMessage() << '\n';
        return 5;
    }

    skellpreset::package::Document document;
    const auto loaded=skellpreset::package::read(file,document);
    file.deleteFile();
    if (loaded.failed()) {
        std::cerr << "FAIL: exported factory package could not be read\n";
        return 6;
    }
    for (float value:document.macros) {
        if (std::abs(value-.5f)>tolerance) {
            std::cerr << "FAIL: exported factory retained hidden macro state\n";
            return 7;
        }
    }

    // Custom .skellpreset packages must preserve all eight macro values too;
    // the factory reset above must not turn valid user values into defaults.
    if (!dirtyMacros(*hostRestored)) {
        std::cerr << "FAIL: macro parameters disappeared after export\n";
        return 8;
    }
    const auto customFile=juce::File::getSpecialLocation(juce::File::tempDirectory)
        .getNonexistentChildFile("skell-custom-macro-roundtrip",".skellpreset",false);
    const auto customSaved=hostRestored->exportPreset(customFile);
    if (customSaved.failed()) {
        std::cerr << "FAIL: custom macro package export: "
                  << customSaved.getErrorMessage() << '\n';
        return 9;
    }
    auto packageRestored=std::make_unique<SkellPresetAudioProcessor>();
    const auto customLoaded=packageRestored->importPreset(customFile);
    customFile.deleteFile();
    if (customLoaded.failed() || !macroParameterContractIsStable(*packageRestored) ||
        !macrosMatchDirty(*packageRestored)) {
        std::cerr << "FAIL: custom .skellpreset import changed macro values or automation ranges\n";
        return 10;
    }

    packageRestored->loadFactoryPreset(24);
    if (!macrosAreNeutral(*packageRestored)) {
        std::cerr << "FAIL: second factory switch did not reset all eight macros\n";
        return 11;
    }

    std::cout << "PASS: macro IDs/ranges, host state save-reload, custom package roundtrip, "
                 "and eight-macro factory reset\n";
    return 0;
}
