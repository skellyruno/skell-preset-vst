#include "../Source/chain/PresetPackage.h"
#include <iostream>
#include <limits>
#include <string>

namespace {
int failures=0;
void check(bool condition,const char* label){
    if(!condition){std::cerr<<"FAIL: "<<label<<'\n';++failures;}
    else std::cout<<"PASS: "<<label<<'\n';
}
juce::File testFile(const juce::File& folder, const juce::String& name){
    return folder.getChildFile(name+".skellpreset");
}
void createArchive(const juce::File& output,const juce::String& name,const std::string& content){
    juce::ZipFile::Builder builder;
    builder.addEntry(new juce::MemoryInputStream(content.data(),content.size(),true), 6,name,juce::Time::getCurrentTime());
    auto stream=output.createOutputStream();
    if(!stream||!builder.writeToStream(*stream,nullptr))++failures;
}
}
int main(){
    const auto dir=juce::File::getSpecialLocation(juce::File::tempDirectory)
                   .getNonexistentChildFile("skell-preset-tests","",false);
    if(dir.createDirectory().failed()){std::cerr<<"Unable to create test folder\n";return 1;}
    skellpreset::package::Document first;
    first.name="Wide Ghost Vocal";first.bank="Adlibs";
    for(int s=0;s<skellpreset::package::slotCount;++s){
        auto& slot=first.slots[(size_t)s];slot.slotId=(s+3)%skellpreset::package::slotCount;
        // Exercise the appended native module IDs while preserving the legacy
        // 0..11 values in the first ten slots of v1 test archives.
        slot.type=s<11?s%11:13+s-11;
        slot.enabled=s%2==0;slot.wet=.15f+(float)s*.04f;
        slot.params.fill(.5f);
    }
    first.activeSlotCount=14;
    first.macros.fill(.5f);
    first.macros[0]=.86f;
    auto path=testFile(dir,"roundtrip");
    check(skellpreset::package::write(path,first).wasOk(),"Export compressed .skellpreset");
    skellpreset::package::Document second;
    check(skellpreset::package::read(path,second).wasOk(),"Import compressed .skellpreset");
    check(second.name==first.name&&second.bank==first.bank&&second.slots[0].slotId==3
          &&second.slots[9].type==9%11&&second.macros[0]==first.macros[0]
          &&second.activeSlotCount==14&&second.slots[15].slotId==2
          &&second.slots[11].type==13&&second.slots[15].type==17,
          "Roundtrip sixteen slots, active rack count, macros and metadata");

    // Schema v2 supports all new VoxRack modules without modifying the
    // original automation indices in an older host project.
    {
        auto vox=first;
        vox.slots[0].type=18; // actual VoxRack Room
        vox.slots[1].type=19; // actual VoxRack Smooth Compressor
        const auto newFile=testFile(dir,"voxrack-roundtrip");
        check(skellpreset::package::write(newFile,vox).wasOk(),
              "Export VoxRack module IDs using existing schema v2");
        skellpreset::package::Document restored;
        check(skellpreset::package::read(newFile,restored).wasOk()
              && restored.slots[0].type==18&&restored.slots[1].type==19,
              "Roundtrip native VoxRack Room and Smooth Comp presets");
    }
    // Appended modulation IDs must roundtrip without shifting older IDs.
    {
        auto motion=first;
        motion.slots[0].type=20;
        motion.slots[1].type=21;
        motion.slots[2].type=22;
        const auto modFile=testFile(dir,"motion-roundtrip");
        check(skellpreset::package::write(modFile,motion).wasOk(),
              "Export three native modulation modules");
        skellpreset::package::Document restored;
        check(skellpreset::package::read(modFile,restored).wasOk()
              && restored.slots[0].type==20
              && restored.slots[1].type==21
              && restored.slots[2].type==22,
              "Roundtrip chorus/flanger/ring module IDs without remapping");
    }
    // Imported DSP modules extend the allowed type range without new schema.
    {
        auto imported=first;
        imported.slots[0].type=23; // Vocal Rider
        imported.slots[1].type=24; // Ducking Ping-Pong
        imported.slots[2].type=25; // Vocal Vibrato
        const auto newFile=testFile(dir,"imported-vocal-fx");
        check(skellpreset::package::write(newFile,imported).wasOk(),
              "Export Vocal Rider, Ducking Delay and Vibrato effects");
        skellpreset::package::Document restored;
        check(skellpreset::package::read(newFile,restored).wasOk()
              &&restored.slots[0].type==23
              &&restored.slots[1].type==24
              &&restored.slots[2].type==25,
              "Roundtrip new rack effect IDs and retain legacy values");
    }
    // Native v2 package supports advanced effects and independent Q without
    // changing original module IDs, old files or schema number.
    {
        auto advanced=first;
        advanced.slots[0].type=26;
        advanced.slots[1].type=27;
        advanced.slots[2].type=28;
        advanced.slots[3].type=29;
        advanced.slots[4].type=30;
        advanced.slots[0].eqQ[0]=.81f;
        advanced.slots[0].eqQ[7]=.12f;
        const auto file=testFile(dir,"advanced-vocal-suite");
        check(skellpreset::package::write(file,advanced).wasOk(),
              "Export 8-Band EQ, FET+Opto, Harshness and Noise");
        skellpreset::package::Document loaded;
        check(skellpreset::package::read(file,loaded).wasOk()
              &&loaded.slots[0].type==26
              &&loaded.slots[1].type==27
              &&loaded.slots[2].type==28
              &&loaded.slots[3].type==29
              &&loaded.slots[4].type==30
              &&loaded.slots[0].eqQ[0]==.81f
              &&loaded.slots[0].eqQ[7]==.12f,
              "Roundtrip new effect IDs and independent EQ bandwidths");
        // Older schema-v2 packages had no eqQ property; neutral values
        // must be provided automatically rather than failing import.
        juce::ZipFile archive(file);
        std::unique_ptr<juce::InputStream> input(archive.createStreamForEntry(0));
        if(input){
            auto json=juce::JSON::parse(input->readEntireStreamAsString());
            if(auto* chain=json.getProperty("chain",{}).getArray())
                for(auto& entry:*chain)
                    if(auto* item=entry.getDynamicObject())
                        item->removeProperty("eqQ");
            const auto noQ=testFile(dir,"legacy-v2-without-eq-q");
            createArchive(noQ,"preset.json",juce::JSON::toString(json).toStdString());
            skellpreset::package::Document restored;
            check(skellpreset::package::read(noQ,restored).wasOk()
                  &&restored.slots[0].eqQ[0]==.5f
                  &&restored.slots[0].eqQ[7]==.5f,
                  "Load old native v2 file with safe neutral EQ bandwidths");
        }
    }
    // A Stage 4/8 schema-v1 ten-slot ZIP must import with identity slots
    // 11..16 appended and no change to old slot IDs.
    {
        juce::ZipFile v2(path);
        std::unique_ptr<juce::InputStream> input(v2.createStreamForEntry(0));
        check(input!=nullptr,"Valid preset ZIP has JSON entry");
        if(!input){dir.deleteRecursively();return 1;}
        const auto jsonText=input->readEntireStreamAsString();
        auto legacyJson=juce::JSON::parse(jsonText);
        auto* object=legacyJson.getDynamicObject();
        check(object!=nullptr,"V2 JSON object");
        if(object){
            object->setProperty("schemaVersion",1);
            object->removeProperty("activeSlotCount");
            auto* chain=object->getProperty("chain").getArray();
            check(chain!=nullptr,"V2 chain array");
            if(chain){
                // Old preset IDs must be in 0..9. Make a deterministic ten
                // entry permutation (v1 format) rather than truncating 16 IDs.
                while(chain->size()>10)chain->removeLast();
                for(int i=0;i<10;++i)
                    (*chain)[i].getDynamicObject()->setProperty("slotId",i);
                const auto json=juce::JSON::toString(legacyJson);
                auto legacyFile=testFile(dir,"legacy-v1");
                createArchive(legacyFile,"preset.json",json.toStdString());
                skellpreset::package::Document restored;
                check(skellpreset::package::read(legacyFile,restored).wasOk(),
                      "Import original ten-slot schema v1 preset");
                check(restored.activeSlotCount==10&&restored.slots[10].slotId==10
                      &&restored.slots[15].slotId==15,
                      "Legacy rack defaults and slot identity preserved");
            }
        }
    }
    // EQ Q is a serialized preset parameter: reject invalid values on both
    // export and import, without changing the last successfully loaded preset.
    {
        auto invalidQ=first;
        invalidQ.slots[0].eqQ[0]=1.01f;
        check(skellpreset::package::write(testFile(dir,"invalid-eq-q"),invalidQ).failed(),
              "Reject out-of-range EQ Q on export");
        invalidQ.slots[0].eqQ[0]=std::numeric_limits<float>::quiet_NaN();
        check(skellpreset::package::write(testFile(dir,"nan-eq-q"),invalidQ).failed(),
              "Reject NaN EQ Q on export");

        juce::ZipFile source(path);
        std::unique_ptr<juce::InputStream> input(source.createStreamForEntry(0));
        check(input!=nullptr,"EQ Q import fixture opens");
        if(input){
            auto json=juce::JSON::parse(input->readEntireStreamAsString());
            if(auto* chain=json.getProperty("chain",{}).getArray()){
                if(auto* item=(*chain)[0].getDynamicObject()){
                    juce::Array<juce::var> badQ;
                    for(int k=0;k<8;++k)badQ.add(k==3?1.5:0.5);
                    item->setProperty("eqQ",juce::var(badQ));
                    const auto badFile=testFile(dir,"bad-import-eq-q");
                    createArchive(badFile,"preset.json",juce::JSON::toString(json).toStdString());
                    skellpreset::package::Document retained=second;
                    check(skellpreset::package::read(badFile,second).failed(),
                          "Reject out-of-range EQ Q on import");
                    check(second.name==retained.name && second.bank==retained.bank
                          && second.slots[0].eqQ==retained.slots[0].eqQ,
                          "Failed EQ Q import preserves previous preset state");
                }
            }
        }
    }
    first.activeSlotCount=17;
    check(skellpreset::package::write(testFile(dir,"bad-count"),first).failed(),
          "Reject rack expansion beyond sixteen slots");
    first.activeSlotCount=14;
    first.slots[1].slotId=first.slots[0].slotId;
    check(skellpreset::package::write(testFile(dir,"duplicate"),first).failed(),"Reject duplicate slot IDs");
    first.slots[1].slotId=4;
    first.slots[2].params[0]=1.2f;
    check(skellpreset::package::write(testFile(dir,"invalid-param"),first).failed(),"Reject out-of-range DSP value");
    first.slots[2].params[0]=.5f;
    first.slots[2].type=99;
    check(skellpreset::package::write(testFile(dir,"invalid-module"),first).failed(),"Reject undefined module type");
    auto malicious=testFile(dir,"path-traversal");
    createArchive(malicious,"../evil.json","{}");
    check(skellpreset::package::read(malicious,second).failed(),"Reject malicious ZIP path");
    auto oversize=testFile(dir,"oversize");
    createArchive(oversize,"preset.json",std::string(300000,'a'));
    check(skellpreset::package::read(oversize,second).failed(),"Reject compressed oversized entry");
    auto malformed=testFile(dir,"malformed");
    createArchive(malformed,"preset.json","not json");
    check(skellpreset::package::read(malformed,second).failed(),"Reject malformed JSON");
    check(second.name==first.name || second.name=="Wide Ghost Vocal", "Bad input keeps previous successful document");
    dir.deleteRecursively();
    if(failures)std::cerr<<failures<<" package tests failed\n";
    return failures?1:0;
}
