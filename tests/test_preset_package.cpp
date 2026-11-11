#include "../Source/chain/PresetPackage.h"
#include <iostream>
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
