#include <JuceHeader.h>
#include "../Source/host/HostedVST3.h"
#include "../Source/host/FLNativeDiscovery.h"
#include <cmath>
#include <cstdio>
#include <iostream>
#if JUCE_WINDOWS
 #include <io.h>
#endif

namespace {
void stage(const char* text) { std::printf("STAGE %s\n",text); std::fflush(stdout); }
int selfTest() {
    namespace fs=std::filesystem;
    const auto root=fs::temp_directory_path()/("skell-fl-discovery-"+juce::Uuid().toString().toStdString());
    struct Cleanup {fs::path root; ~Cleanup(){std::error_code ec;fs::remove_all(root,ec);}} cleanup{root};
    std::error_code ec;
    fs::create_directories(root/"Shared",ec);
    const auto first=root/"Plugins"/"Fruity"/"Effects"/"MockSat";
    fs::create_directories(first,ec);
    fs::create_directories(root/"Plugins"/"Fruity"/"Effects"/"OnlyEngine",ec);
    const bool ok=juce::File(juce::String((first/"MockSat_x64.dll").u8string())).create().wasOk() &&
        juce::File(juce::String((first/"engine_x64.dll").u8string())).create().wasOk() &&
        juce::File(juce::String((root/"Plugins"/"Fruity"/"Effects"/"OnlyEngine"/"engine_x64.dll").u8string())).create().wasOk();
    const auto result=skellpreset::host::flnative::discover(root);
    if (!ok || result.size()!=1 || result[0].name!="MockSat" ||
        result[0].identifier!="mocksat_x64" ||
        !skellpreset::host::flnative::discover(root/"missing").empty())return 31;
    stage("pass read-only FL native discovery fixture (no DLL loaded)");
    using Host=skellpreset::host::HostedVST3;
    const auto bridge=juce::File(juce::String((root/"FloopHole_x64.vst3").u8string()));
    if(!Host::needsIsolatedHost(bridge) || Host::blocksNestedBridge(bridge))return 32;
    // The path is intentionally missing. The standalone diagnostic must
    // reach file validation, not the nested-rack rejection.
    std::vector<Host::EffectChoice> choices;
    const auto scan=Host::inspectEffects(bridge,choices);
    if(scan.wasOk() || scan.getErrorMessage()!="Choose an installed .vst3 bundle")return 33;
    stage("pass diagnostic-only FloopHole policy (no DLL loaded)");
    return 0;
}
int probeVST3(const juce::File& path,juce::String requestedId) {
    using Host=skellpreset::host::HostedVST3;
    if(Host::blocksNestedBridge(path)){
        std::fprintf(stderr,"BLOCKED FloopHole needs an out-of-process host inside Skell Preset.\n");
        return 40;
    }
    if(Host::needsIsolatedHost(path))
        stage("standalone FloopHole probe; nested rack hosting remains blocked");
    stage("scan VST3 bundle");
    std::vector<Host::EffectChoice> choices;
    const auto inspected=Host::inspectEffects(path,choices);
    if(inspected.failed()){
        std::fprintf(stderr,"FAIL scan: %s\n",inspected.getErrorMessage().toRawUTF8());return 41;
    }
    for(const auto& choice:choices)
        std::printf("EFFECT %s | %s\n",choice.name.toRawUTF8(),choice.identifier.toRawUTF8());
    if(choices.empty())return 42;
    std::fflush(stdout);
    // Never silently choose one native FL effect from a multi-class bridge.
    if(requestedId.isEmpty()){
        if(choices.size()!=1){
            std::printf("SELECT: multiple audio effects found. Run again with the exact EFFECT name or class ID.\n");
            return 47;
        }
        requestedId=choices.front().identifier;
    }else{
        juce::String resolved;
        int matches=0;
        for(const auto& choice:choices){
            if(choice.identifier==requestedId || choice.name.equalsIgnoreCase(requestedId)){
                resolved=choice.identifier;++matches;
            }
        }
        if(matches!=1){
            std::fprintf(stderr,"FAIL effect selection: name/class ID missing or ambiguous. Use an exact class ID from the list.\n");
            return 48;
        }
        requestedId=resolved;
    }
    stage("instantiate selected VST3 class");
    Host host;
    const auto loaded=host.load(0,path,48000.,256,requestedId);
    if(loaded.failed()){
        std::fprintf(stderr,"FAIL instantiate: %s\n",loaded.getErrorMessage().toRawUTF8());return 43;
    }
    auto* instance=host.get(0);
    if(!instance){std::fprintf(stderr,"FAIL missing instance after load\n");return 44;}
    std::printf("INSTANCE name=%s latency=%d samples\n",host.getName(0).toRawUTF8(),host.getLatency(0));
    std::printf("BUSES input=%d output=%d main-in=%d main-out=%d\n",
        instance->getBusCount(true),instance->getBusCount(false),
        instance->getMainBusNumInputChannels(),instance->getMainBusNumOutputChannels());
    stage("render 4 stereo buffers at 48 kHz / 256");
    juce::AudioBuffer<float> audio(2,256);
    juce::MidiBuffer midi;
    double peak=0.;
    for(int block=0;block<4;++block){
        midi.clear();
        for(int ch=0;ch<2;++ch)
            for(int n=0;n<256;++n)
                audio.setSample(ch,n,.08f*std::sin(2.*juce::MathConstants<double>::pi*
                    (ch==0?440.:660.)*(block*256+n)/48000.));
        instance->processBlock(audio,midi);
        for(int ch=0;ch<2;++ch)
            for(int n=0;n<256;++n){
                const auto value=audio.getSample(ch,n);
                if(!std::isfinite(value)){std::fprintf(stderr,"FAIL nonfinite audio\n");return 45;}
                peak=std::max(peak,std::abs((double)value));
            }
    }
    std::printf("PASS scan / instantiate / stereo render; peak=%g\n",peak);
    return 0;
}
}
int main(int argc,char** argv) {
    // Explorer launches without arguments. Keep a visible interactive console
    // and run the actual VST3 probe as a bounded child, not in this launcher.
    if(argc==1){
        std::cout << "SKELL PLUGIN DIAGNOSTIC\n"
                     "Close your DAW first. Paste or drag the full path of a failing .vst3 bundle below.\n"
                     "Press Enter to test it (or Enter on an empty line to quit).\n> " << std::flush;
        std::string input;
        std::getline(std::cin,input);
        const auto path=juce::String::fromUTF8(input.c_str()).trim().unquoted();
        if(path.isNotEmpty()){
            const auto log=juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
                .getChildFile("Skell-Plugin-Diagnostic-"+juce::Uuid().toString()+".txt");
            const auto exe=juce::File::getSpecialLocation(juce::File::currentExecutableFile);
            juce::ChildProcess child;
            juce::StringArray args{exe.getFullPathName(),"--report",log.getFullPathName(),"--vst3",path};
            std::cout<<"Effect name (e.g. Fruity Reeverb 2), or Enter to list choices: "<<std::flush;
            std::string effect;
            std::getline(std::cin,effect);
            const auto selected=juce::String::fromUTF8(effect.c_str()).trim();
            if(selected.isNotEmpty())args.add(selected);
            std::cout<<"Testing in a separate process (25-second timeout)...\n"<<std::flush;
            if(!child.start(args,0)){
                log.appendText("FAIL: could not launch diagnostic worker.\n");
            }else if(!child.waitForProcessToFinish(25000)){
                const bool stopped=child.kill();
                child.waitForProcessToFinish(2000);
                log.appendText(stopped?"\nFAIL timeout: worker terminated after 25 seconds.\n":
                    "\nFAIL timeout: worker did not terminate cleanly.\n");
            }else{
                log.appendText("\nWorker exit code: "+juce::String((juce::int64)child.getExitCode())+"\n");
            }
            if(log.existsAsFile())std::cout<<log.loadFileAsString().toStdString()<<"\n";
            else std::cout<<"FAIL: unable to write the report file.\n";
            std::cout<<"Report: "<<log.getFullPathName().toStdString()<<"\n";
        }
        std::cout<<"Press Enter to close.\n"<<std::flush;
        std::getline(std::cin,input);
        return 0;
    }
    if((argc==5 || argc==6) && juce::String(argv[1])=="--report" && juce::String(argv[3])=="--vst3"){
        // Redirect inside worker; no shell quoting, no full pipe deadlock.
        #if JUCE_WINDOWS
        FILE* stream=nullptr;
        const auto file=juce::File(juce::String::fromUTF8(argv[2]));
        if(_wfreopen_s(&stream,file.getFullPathName().toWideCharPointer(),L"w",stdout)!=0)return 46;
        if(_dup2(_fileno(stdout),_fileno(stderr))!=0)return 46;
        std::setvbuf(stdout,nullptr,_IONBF,0);
        std::setvbuf(stderr,nullptr,_IONBF,0);
        juce::ScopedJuceInitialiser_GUI gui;
        stage("interactive worker started");
        const auto code=probeVST3(juce::File(juce::String::fromUTF8(argv[4])),
            argc==6?juce::String::fromUTF8(argv[5]):juce::String{});
        std::printf("EXIT %d\n",code);
        return code;
        #else
        return 46;
        #endif
    }
    if(argc==2 && juce::String(argv[1])=="--self-test")return selfTest();
    if(argc==3 && juce::String(argv[1])=="--fl-native"){
        const auto root=std::filesystem::u8path(argv[2]);
        if(!skellpreset::host::flnative::installedAt(root)){
            std::fprintf(stderr,"FAIL FL native inventory: supply an installed licensed FL Studio directory with Shared and Plugins/Fruity/Effects.\n");
            return 20;
        }
        stage("file-only FL native inventory (not loaded)");
        const auto results=skellpreset::host::flnative::discover(root);
        for(const auto& e:results)std::printf("FL_EFFECT %s | %s\n",e.name.c_str(),e.identifier.c_str());
        std::printf("COUNT %zu\n",results.size());return 0;
    }
    if(argc<3 || juce::String(argv[1])!="--vst3"){
        std::fprintf(stderr,"Usage: SkellPluginDiagnostic.exe --vst3 <bundle.vst3> [JUCE-class-ID] | --fl-native <FL-root> | --self-test\n");
        return 2;
    }
    juce::ScopedJuceInitialiser_GUI gui;
    return probeVST3(juce::File(juce::String::fromUTF8(argv[2])),
        argc>3?juce::String::fromUTF8(argv[3]):juce::String{});
}
