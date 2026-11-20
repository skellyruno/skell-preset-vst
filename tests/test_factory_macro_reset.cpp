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

bool macroParameterContractIsStable(SkellPresetAudioProcessor& processor) {
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

bool emptyRackDefaultAndLegacyRestore() {
    auto empty=std::make_unique<SkellPresetAudioProcessor>();
    if(empty->loadedFactoryPresetIndex()!=-1 || empty->presetName()!="Empty Rack" ||
       empty->getCurrentProgram()!=0 ||
       empty->activeSlotCount()!=SkellPresetAudioProcessor::defaultSlotCount) {
        std::cerr << "DIAG: new instance is not an unselected empty rack\\n";
        return false;
    }
    for(int slot=0;slot<SkellPresetAudioProcessor::slotCount;++slot) {
        if(empty->slotType(slot)!=SkellPresetAudioProcessor::Empty) {
            std::cerr << "DIAG: default slot is not Empty: " << slot << '\\n';
            return false;
        }
        const auto onId="s"+juce::String(slot+1)+"_on";
        const auto* on=empty->state.getRawParameterValue(onId);
        if(!on || std::abs(on->load())>tolerance) {
            std::cerr << "DIAG: default slot is not OFF: " << slot << '\\n';
            return false;
        }
        if(empty->slotIdAt(slot)!=slot) return false;
    }

    empty->prepareToPlay(44100.0,256);
    if(empty->getLatencySamples()!=0) {
        std::cerr << "DIAG: empty rack added host latency\\n";
        return false;
    }
    juce::AudioBuffer<float> audio(2,256);
    for(int i=0;i<audio.getNumSamples();++i) {
        const float sample=0.25f*std::sin(2.0f*juce::MathConstants<double>::pi*997.0*i/44100.0);
        audio.setSample(0,i,sample);
        audio.setSample(1,i,sample*0.73f);
    }
    juce::MidiBuffer midi;
    empty->processBlock(audio,midi);
    for(int i=0;i<audio.getNumSamples();++i) {
        const float expected=0.25f*std::sin(2.0f*juce::MathConstants<double>::pi*997.0*i/44100.0);
        if(!std::isfinite(audio.getSample(0,i)) || !std::isfinite(audio.getSample(1,i)) ||
           std::abs(audio.getSample(0,i)-expected)>1.0e-5f ||
           std::abs(audio.getSample(1,i)-expected*0.73f)>1.0e-5f) {
            std::cerr << "DIAG: empty rack was not finite identity DSP\\n";
            return false;
        }
    }

    juce::MemoryBlock emptyState;
    empty->getStateInformation(emptyState);
    auto emptyRestored=std::make_unique<SkellPresetAudioProcessor>();
    emptyRestored->setStateInformation(emptyState.getData(),static_cast<int>(emptyState.getSize()));
    if(emptyRestored->loadedFactoryPresetIndex()!=-1 ||
       emptyRestored->presetName()!="Empty Rack" ||
       emptyRestored->activeSlotCount()!=SkellPresetAudioProcessor::defaultSlotCount)
        return false;
    for(int slot=0;slot<SkellPresetAudioProcessor::slotCount;++slot)
        if(emptyRestored->slotType(slot)!=SkellPresetAudioProcessor::Empty)
            return false;

    // Existing projects saved with factory program 0 must still reopen as Clean Lead.
    auto legacy=std::make_unique<SkellPresetAudioProcessor>();
    legacy->loadFactoryPreset(0);
    if(legacy->loadedFactoryPresetIndex()!=0 || legacy->presetName()!="Clean Lead")
        return false;
    juce::MemoryBlock legacyState;
    legacy->getStateInformation(legacyState);
    auto legacyRestored=std::make_unique<SkellPresetAudioProcessor>();
    legacyRestored->setStateInformation(legacyState.getData(),static_cast<int>(legacyState.getSize()));
    const auto* legacyOn=legacyRestored->state.getRawParameterValue("s1_on");
    if(legacyRestored->loadedFactoryPresetIndex()!=0 ||
       legacyRestored->presetName()!="Clean Lead" ||
       legacyRestored->activeSlotCount()!=SkellPresetAudioProcessor::defaultSlotCount ||
       legacyRestored->slotType(0)!=SkellPresetAudioProcessor::EQ ||
       !legacyOn || legacyOn->load()<0.5f) {
        std::cerr << "DIAG: legacy factory session did not restore its original active chain\\n";
        return false;
    }
    return true;
}

bool hostStateSanitizerRoundtrip() {
    auto source=std::make_unique<SkellPresetAudioProcessor>();
    juce::MemoryBlock baseline;
    source->getStateInformation(baseline);
    auto xml=juce::AudioProcessor::getXmlFromBinary(
        baseline.getData(),static_cast<int>(baseline.getSize()));
    if(!xml)return false;
    auto tree=juce::ValueTree::fromXml(*xml);
    if(!tree.isValid())return false;

    const auto setText=[&](const juce::String& id,const juce::String& value)->bool {
        for(int i=0;i<tree.getNumChildren();++i) {
            auto child=tree.getChild(i);
            if(child.getProperty("id").toString()==id) {
                child.setProperty("value",value,nullptr);
                return true;
            }
        }
        std::cerr << "FAIL: sanitizer fixture references missing APVTS parameter ID: "
                  << id << '\n';
        return false;
    };
    if(!setText("input","nan") || !setText("mix","inf") ||
       !setText("output","-inf") || !setText("macro1","not-a-number") ||
       !setText("macro2","0.371") ||
       !setText("s1_wet","999") || !setText("s1_p1","-999") ||
       !setText("s1_p2","0.371") || !setText("s1_eqQ1","nan") ||
       !setText("s1_eqQ2","0.371") || !setText("mode","2.4") ||
       !setText("s1_type","999") || !setText("s2_type","not-a-number"))
        return false;

    auto corruptedXml=tree.createXml();
    if(!corruptedXml)return false;
    juce::MemoryBlock corrupted;
    juce::AudioProcessor::copyXmlToBinary(*corruptedXml,corrupted);

    auto restored=std::make_unique<SkellPresetAudioProcessor>();
    restored->setStateInformation(corrupted.getData(),static_cast<int>(corrupted.getSize()));

    const auto matchesDefault=[](SkellPresetAudioProcessor& processor,
                                 const juce::String& id)->bool {
        auto* parameter=processor.state.getParameter(id);
        auto* raw=processor.state.getRawParameterValue(id);
        if(!parameter||!raw)return false;
        const auto& range=parameter->getNormalisableRange();
        float expected=range.convertFrom0to1(parameter->getDefaultValue());
        if(!std::isfinite(expected))expected=range.start;
        expected=juce::jlimit(range.start,range.end,expected);
        if(parameter->isDiscrete())expected=range.snapToLegalValue(expected);
        const float actual=raw->load();
        const bool ok=std::isfinite(actual)&&std::abs(actual-expected)<1.e-4f;
        if(!ok)
            std::cerr << "DIAG: default id=" << id << " actual=" << actual
                      << " expected=" << expected << " range=[" << range.start
                      << "," << range.end << "] discrete=" << parameter->isDiscrete() << '\\n';
        return ok;
    };
    const auto matchesLegal=[](SkellPresetAudioProcessor& processor,
                               const juce::String& id,float requested)->bool {
        auto* parameter=processor.state.getParameter(id);
        auto* raw=processor.state.getRawParameterValue(id);
        if(!parameter||!raw)return false;
        const auto& range=parameter->getNormalisableRange();
        const float bounded=juce::jlimit(range.start,range.end,requested);
        const float expected=parameter->isDiscrete()?range.snapToLegalValue(bounded):bounded;
        const float actual=raw->load();
        const bool ok=std::isfinite(actual)&&std::abs(actual-expected)<1.e-4f;
        if(!ok)
            std::cerr << "DIAG: legal id=" << id << " requested=" << requested
                      << " actual=" << actual << " expected=" << expected
                      << " range=[" << range.start << "," << range.end
                      << "] discrete=" << parameter->isDiscrete() << '\\n';
        return ok;
    };
    const auto matchesValue=[](SkellPresetAudioProcessor& processor,
                               const juce::String& id,float expected)->bool {
        const auto* raw=processor.state.getRawParameterValue(id);
        const float actual=raw?raw->load():std::numeric_limits<float>::quiet_NaN();
        const bool ok=raw&&std::isfinite(actual)&&std::abs(actual-expected)<1.e-5f;
        if(!ok)
            std::cerr << "DIAG: preserved value id=" << id << " actual=" << actual
                      << " expected=" << expected << '\\n';
        return ok;
    };
    const auto allFiniteAndInRange=[](SkellPresetAudioProcessor& processor)->bool {
        for(int i=0;i<processor.state.state.getNumChildren();++i) {
            const auto child=processor.state.state.getChild(i);
            const auto id=child.getProperty("id").toString();
            auto* parameter=processor.state.getParameter(id);
            auto* raw=processor.state.getRawParameterValue(id);
            if(!parameter||!raw)continue;
            const auto& range=parameter->getNormalisableRange();
            const float value=raw->load();
            if(!std::isfinite(value)||value<range.start-1.e-4f||value>range.end+1.e-4f) {
                std::cerr << "DIAG: out-of-range id=" << id << " value=" << value
                          << " range=[" << range.start << "," << range.end << "]\\n";
                return false;
            }
        }
        return true;
    };

    if(!matchesDefault(*restored,"input") ||
       !matchesDefault(*restored,"mix") ||
       !matchesDefault(*restored,"output") ||
       !matchesDefault(*restored,"macro1") ||
       !matchesDefault(*restored,"s1_eqQ1") ||
       !matchesDefault(*restored,"s2_type") ||
       !matchesLegal(*restored,"s1_wet",999.0f) ||
       !matchesLegal(*restored,"s1_p1",-999.0f) ||
       !matchesLegal(*restored,"mode",2.4f) ||
       !matchesLegal(*restored,"s1_type",999.0f) ||
       !matchesValue(*restored,"macro2",0.371f) ||
       !matchesValue(*restored,"s1_p2",0.371f) ||
       !matchesValue(*restored,"s1_eqQ2",0.371f) ||
       !allFiniteAndInRange(*restored))
        return false;

    // A second host save/reload must preserve the repaired values, not reintroduce
    // the malformed XML or quantize valid continuous automation.
    juce::MemoryBlock repaired;
    restored->getStateInformation(repaired);
    auto roundtrip=std::make_unique<SkellPresetAudioProcessor>();
    roundtrip->setStateInformation(repaired.getData(),static_cast<int>(repaired.getSize()));
    if(!allFiniteAndInRange(*roundtrip) ||
       !matchesDefault(*roundtrip,"input") ||
       !matchesDefault(*roundtrip,"macro1") ||
       !matchesValue(*roundtrip,"macro2",0.371f) ||
       !matchesValue(*roundtrip,"s1_p2",0.371f) ||
       !matchesValue(*roundtrip,"s1_eqQ2",0.371f))
        return false;

    // Exercise a real audio block after corrupt-state recovery.
    if(auto* mode=roundtrip->state.getParameter("mode"))
        mode->setValueNotifyingHost(mode->getDefaultValue());
    roundtrip->prepareToPlay(44100.0,256);
    juce::AudioBuffer<float> audio(2,256);
    for(int i=0;i<audio.getNumSamples();++i) {
        const float sample=0.25f*std::sin(2.0f*juce::MathConstants<double>::pi
                                          *997.0*i/44100.0);
        audio.setSample(0,i,sample);
        audio.setSample(1,i,sample*0.73f);
    }
    juce::MidiBuffer midi;
    roundtrip->processBlock(audio,midi);
    for(int ch=0;ch<audio.getNumChannels();++ch)
        for(int i=0;i<audio.getNumSamples();++i)
            if(!std::isfinite(audio.getSample(ch,i))) {
                std::cerr << "DIAG: non-finite DSP sample channel=" << ch
                          << " index=" << i << " value=" << audio.getSample(ch,i) << '\\n';
                return false;
            }
    return true;
}

bool macrosMatchDirty(SkellPresetAudioProcessor& processor) {
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
    if(!emptyRackDefaultAndLegacyRestore()) {
        std::cerr << "FAIL: empty new-instance default or legacy Clean Lead host-state restore\\n";
        return 13;
    }
    std::cerr << "STEP 1: empty rack default, zero-latency identity and legacy session restore verified\\n" << std::flush;
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

    if(!hostStateSanitizerRoundtrip()) {
        std::cerr << "FAIL: corrupt host-state NaN/Inf recovery, range clamp, automation preservation, or real DSP block\\n";
        return 12;
    }
    std::cerr << "STEP 3A: corrupt host-state sanitizer and repaired reload verified\\n" << std::flush;

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
