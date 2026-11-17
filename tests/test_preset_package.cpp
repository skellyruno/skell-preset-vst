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
    // Fresh default racks must export/import with stable slot identities and no active effects.
    {
        skellpreset::package::Document empty;
        bool valid = true;
        for (int s = 0; s < skellpreset::package::slotCount; ++s)
            valid = valid && empty.slots[(size_t)s].slotId == s
                          && !empty.slots[(size_t)s].enabled;
        check(valid, "Default Alchemist-compatible rack has unique inactive slots");
        bool neutralMacros=true;
        for(float value:empty.macros)neutralMacros=neutralMacros && value==.5f;
        check(neutralMacros,"Default preset macros match neutral host defaults");
        bool neutralSlotParameters=true;
        for(const auto& slot:empty.slots)
            for(float value:slot.params)
                neutralSlotParameters=neutralSlotParameters && value==.5f;
        check(neutralSlotParameters,
              "Default empty rack effect parameters match neutral APVTS defaults");
        const auto emptyPath = testFile(dir, "default-empty-rack");
        check(skellpreset::package::write(emptyPath, empty).wasOk(),
              "Default rack exports without duplicate IDs");
        skellpreset::package::Document loaded;
        check(skellpreset::package::read(emptyPath, loaded).wasOk()
              && loaded.slots[0].slotId == 0
              && loaded.slots[15].slotId == 15
              && !loaded.slots[0].enabled
              && !loaded.slots[15].enabled
              && !loaded.micPreOn,
              "Default rack imports without activating effects or mic preamp");
        bool importedNeutralParameters=true;
        for(const auto& slot:loaded.slots)
            for(float value:slot.params)
                importedNeutralParameters=importedNeutralParameters && value==.5f;
        check(importedNeutralParameters,
              "Default empty rack roundtrip preserves neutral parameters in all 16 slots");
    }
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
    first.micPreOn=true;
    first.micPreInput=1;  // generic condenser input
    first.micPreTarget=3; // generic air output
    first.micPreAmount=.72f;first.micPreTrim=.63f;
    first.macros.fill(.5f);
    first.macros[0]=.86f;
    auto path=testFile(dir,"roundtrip");
    check(skellpreset::package::write(path,first).wasOk(),"Export compressed .skellpreset");
    skellpreset::package::Document second;
    check(skellpreset::package::read(path,second).wasOk(),"Import compressed .skellpreset");
    check(second.name==first.name&&second.bank==first.bank&&second.slots[0].slotId==3
          &&second.slots[9].type==9%11&&second.macros[0]==first.macros[0]
          &&second.activeSlotCount==14&&second.slots[15].slotId==2
          &&second.slots[11].type==13&&second.slots[15].type==17
          &&second.micPreOn && second.micPreInput==1
          &&second.micPreTarget==3 && second.micPreAmount==.72f
          &&second.micPreTrim==.63f,
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
        advanced.slots[5].type=31; // Alchemist causal finish, append-only native module
        advanced.slots[6].type=32; // Mic Modeling must roundtrip in schema v2
        // Factory 52's exact FET/Opto normalized controls must survive .skellpreset export/import.
        advanced.slots[1].wet=.70f;
        advanced.slots[1].params[0]=.428571f;
        advanced.slots[1].params[1]=.50f;
        advanced.slots[1].params[2]=.35f;
        advanced.slots[1].params[3]=.15f;
        advanced.slots[0].eqQ[0]=.81f;
        advanced.slots[0].eqQ[7]=.12f;
        advanced.slots[0].eqLowCut=.63f;
        advanced.slots[0].eqHighCut=.39f;
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
              &&loaded.slots[5].type==31
              &&loaded.slots[6].type==32
              &&loaded.slots[1].wet==.70f
              &&std::abs(loaded.slots[1].params[0]-.428571f)<1.e-6f
              &&std::abs(loaded.slots[1].params[1]-.50f)<1.e-6f
              &&std::abs(loaded.slots[1].params[2]-.35f)<1.e-6f
              &&std::abs(loaded.slots[1].params[3]-.15f)<1.e-6f
              &&loaded.slots[0].eqQ[0]==.81f
              &&loaded.slots[0].eqQ[7]==.12f
              &&loaded.slots[0].eqLowCut==.63f
              &&loaded.slots[0].eqHighCut==.39f,
              "Roundtrip new effect IDs, eight Q bands and high/low cuts");
        // Older schema-v2 packages had no eqQ property; neutral values
        // must be provided automatically rather than failing import.
        juce::ZipFile archive(file);
        std::unique_ptr<juce::InputStream> input(archive.createStreamForEntry(0));
        if(input){
            auto json=juce::JSON::parse(input->readEntireStreamAsString());
            if(auto* chain=json.getProperty("chain",{}).getArray())
                for(auto& entry:*chain)
                    if(auto* item=entry.getDynamicObject())
                        {
                            item->removeProperty("eqQ");
                            item->removeProperty("eqLowCut");
                            item->removeProperty("eqHighCut");
                        }
            const auto noQ=testFile(dir,"legacy-v2-without-eq-q");
            createArchive(noQ,"preset.json",juce::JSON::toString(json).toStdString());
            skellpreset::package::Document restored;
            check(skellpreset::package::read(noQ,restored).wasOk()
                  &&restored.slots[0].eqQ[0]==.5f
                  &&restored.slots[0].eqQ[7]==.5f
                  &&restored.slots[0].eqLowCut==0.f
                  &&restored.slots[0].eqHighCut==0.f,
                  "Load old native v2 file with flat EQ cuts and neutral Q");
        }
    }
    // Modules 33..37 are append-only. ID 38 is deliberately unknown.
    {
        auto newest=first;
        newest.slots[0].type=33;
        const auto file=testFile(dir,"skelldizer-id33-roundtrip");
        check(skellpreset::package::write(file,newest).wasOk(),
              "Export Skelldizer module 33");
        skellpreset::package::Document recalled;
        check(skellpreset::package::read(file,recalled).wasOk()
              &&recalled.slots[0].type==33,
              "Recall Skelldizer module 33 without remapping");
        for(int studio=34;studio<=37;++studio) {
            newest.slots[0].type=studio;
            const auto target=testFile(dir,"studio-module-"+juce::String(studio));
            check(skellpreset::package::write(target,newest).wasOk(),
                  "Write appended Studio module without changing schema");
            skellpreset::package::Document loaded;
            check(skellpreset::package::read(target,loaded).wasOk()
                  && loaded.slots[0].type==studio,
                  "Recall appended Studio module without renumbering");
        }
        newest.slots[0].type=38;
        check(skellpreset::package::write(
                 testFile(dir,"unknown-id38"),newest).failed(),
              "Reject unknown module 38");
    }
    // Schema-v2 macros are optional too: absent means neutral, malformed means reject.
    {
        juce::ZipFile v2(path);
        std::unique_ptr<juce::InputStream> input(v2.createStreamForEntry(0));
        check(input!=nullptr,"Schema-v2 archive has JSON entry for optional-macro test");
        if(input) {
            const auto jsonText=input->readEntireStreamAsString();
            auto noMacros=juce::JSON::parse(jsonText);
            auto* object=noMacros.getDynamicObject();
            auto* globals=object?object->getProperty("globals").getDynamicObject():nullptr;
            check(globals!=nullptr,"Schema-v2 globals object exists");
            if(globals) {
                globals->removeProperty("macros");
                const auto json=juce::JSON::toString(noMacros);
                auto file=testFile(dir,"schema-v2-no-macros");
                createArchive(file,"preset.json",json.toStdString());
                skellpreset::package::Document restored;
                const auto result=skellpreset::package::read(file,restored);
                check(result.wasOk(),"Import schema-v2 archive without optional macros");
                if(result.wasOk())
                    check(restored.macros[0]==.5f&&restored.macros[7]==.5f,
                          "Missing schema-v2 macros default to neutral 50%");
            }
            auto malformed=juce::JSON::parse(jsonText);
            auto* malformedObject=malformed.getDynamicObject();
            auto* malformedGlobals=malformedObject
                ?malformedObject->getProperty("globals").getDynamicObject():nullptr;
            if(malformedGlobals) {
                juce::Array<juce::var> shortMacros;
                for(int i=0;i<7;++i)shortMacros.add(.5f);
                malformedGlobals->setProperty("macros",juce::var(shortMacros));
                auto file=testFile(dir,"schema-v2-short-macros");
                createArchive(file,"preset.json",juce::JSON::toString(malformed).toStdString());
                skellpreset::package::Document rejected;
                rejected.name="Preserve Existing Preset";
                rejected.macros[0]=.23f;
                const auto malformedResult=skellpreset::package::read(file,rejected);
                check(malformedResult.failed(),
                      "Reject malformed optional macro array when present");
                check(rejected.name=="Preserve Existing Preset"
                      && rejected.macros[0]==.23f,
                      "Malformed optional macro import leaves destination unchanged");
            } else check(false,"Malformed schema-v2 globals object exists");
            struct NullCase {
                bool global;
                const char* property;
                const char* suffix;
                const char* label;
            };
            const NullCase nullCases[] = {
                {true,"micPreOn","mic-pre-on","Reject explicit null mic-pre enable"},
                {true,"micPreInput","mic-pre-input","Reject explicit null mic-pre input"},
                {true,"micPreTarget","mic-pre-target","Reject explicit null mic-pre target"},
                {true,"micPreAmount","mic-pre-amount","Reject explicit null mic-pre amount"},
                {true,"micPreTrim","mic-pre-trim","Reject explicit null mic-pre trim"},
                {true,"macros","macros","Reject explicit null macro array"},
                {false,"eqQ","eq-q","Reject explicit null EQ Q array"},
                {false,"studioEqKind","studio-eq-kind","Reject explicit null Studio EQ type array"},
                {false,"studioEqOn","studio-eq-on","Reject explicit null Studio EQ enable array"},
                {false,"eqLowCut","eq-low-cut","Reject explicit null EQ low-cut value"},
                {false,"eqHighCut","eq-high-cut","Reject explicit null EQ high-cut value"}
            };
            for(const auto& test:nullCases) {
                auto nullJson=juce::JSON::parse(jsonText);
                auto* nullRoot=nullJson.getDynamicObject();
                auto* nullGlobals=nullRoot
                    ?nullRoot->getProperty("globals").getDynamicObject():nullptr;
                auto* nullChain=nullRoot
                    ?nullRoot->getProperty("chain").getArray():nullptr;
                juce::DynamicObject* target=test.global
                    ?nullGlobals
                    :(nullChain && nullChain->size()>0
                        ?(*nullChain)[0].getDynamicObject():nullptr);
                if(!target) {
                    check(false,"Build explicit-null optional field fixture");
                    break;
                }
                target->setProperty(juce::Identifier(test.property),juce::var());
                const auto nullFile=testFile(
                    dir,juce::String("schema-v2-null-")+test.suffix);
                createArchive(nullFile,"preset.json",
                    juce::JSON::toString(nullJson).toStdString());
                skellpreset::package::Document preserved;
                preserved.name="Preserve Existing Preset";
                preserved.macros[0]=.23f;
                const auto nullResult=skellpreset::package::read(nullFile,preserved);
                check(nullResult.failed(),test.label);
                check(preserved.name=="Preserve Existing Preset"
                      &&preserved.macros[0]==.23f,
                      "Explicit-null optional import leaves destination unchanged");
            }
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
            auto* globals=object->getProperty("globals").getDynamicObject();
            check(globals!=nullptr,"Legacy globals object exists");
            if(globals) {
                globals->removeProperty("macros");
                globals->removeProperty("micPreOn");
                globals->removeProperty("micPreInput");
                globals->removeProperty("micPreTarget");
                globals->removeProperty("micPreAmount");
                globals->removeProperty("micPreTrim");
            }
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
                check(restored.macros[0]==.5f&&restored.macros[7]==.5f
                      &&!restored.micPreOn&&restored.micPreAmount==1.f
                      &&restored.micPreTrim==.5f,
                      "Legacy optional controls restore to neutral/off defaults");
            }
        }
    }
    // New Studio EQ HP/LP/shelf and eighth-node controls are optional
    // v2 extensions. The old 16 parameter automation and slot IDs stay
    // identical, but native ZIPs must roundtrip these new choices.
    {
        auto studio=first;
        studio.slots[0].type=34;
        studio.slots[0].studioEqKind[0]=1;  // high-pass
        studio.slots[0].studioEqKind[6]=4;  // high shelf
        studio.slots[0].studioEqKind[7]=2;  // low-pass
        studio.slots[0].studioEqOn[7]=true; // added eighth node
        const auto zip=testFile(dir,"studio-eq-eight-band");
        check(skellpreset::package::write(zip,studio).wasOk(),
              "Write Studio EQ eight-band filter type and enabled flags");
        skellpreset::package::Document decoded;
        check(skellpreset::package::read(zip,decoded).wasOk(),
              "Read Studio EQ filter types and enabled flags");
        check(decoded.slots[0].type==34 &&
              decoded.slots[0].studioEqKind[0]==1 &&
              decoded.slots[0].studioEqKind[6]==4 &&
              decoded.slots[0].studioEqKind[7]==2 &&
              decoded.slots[0].studioEqOn[7],
              "Studio EQ HP/LP/shelf and enabled B8 survive ZIP roundtrip");
        studio.slots[0].studioEqKind[7]=5;
        check(skellpreset::package::write(
                testFile(dir,"bad-studio-eq-shape"),studio).failed(),
              "Reject unsupported Studio EQ band type 5");
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
    first.slots[2].eqHighCut=1.1f;
    check(skellpreset::package::write(testFile(dir,"invalid-cut"),first).failed(),
          "Reject out-of-range EQ cut data before writing ZIP");
    first.slots[2].eqHighCut=0.f;
    first.slots[2].type=99;
    check(skellpreset::package::write(testFile(dir,"invalid-module"),first).failed(),"Reject undefined module type");
    // PR #21 safety regression: invalid imports must never mutate a
    // previously accepted preset, even if the new archive carries a
    // malformed path, oversized payload or malformed JSON.
    const auto previous=second;
    const auto unchanged=[&](){
        if(second.name!=previous.name || second.author!=previous.author ||
           second.bank!=previous.bank || second.mode!=previous.mode ||
           second.activeSlotCount!=previous.activeSlotCount ||
           second.inputDb!=previous.inputDb || second.mix!=previous.mix ||
           second.outputDb!=previous.outputDb ||
           second.micPreOn!=previous.micPreOn ||
           second.micPreInput!=previous.micPreInput ||
           second.micPreTarget!=previous.micPreTarget ||
           second.micPreAmount!=previous.micPreAmount ||
           second.micPreTrim!=previous.micPreTrim ||
           second.macros!=previous.macros)return false;
        for(size_t i=0;i<second.slots.size();++i){
            const auto& a=second.slots[i];
            const auto& b=previous.slots[i];
            if(a.slotId!=b.slotId || a.type!=b.type ||
               a.enabled!=b.enabled || a.wet!=b.wet ||
               a.params!=b.params || a.eqQ!=b.eqQ ||
               a.studioEqKind!=b.studioEqKind ||
               a.studioEqOn!=b.studioEqOn ||
               a.eqLowCut!=b.eqLowCut || a.eqHighCut!=b.eqHighCut)return false;
        }
        return true;
    };
    auto malicious=testFile(dir,"path-traversal");
    createArchive(malicious,"../evil.json","{}");
    check(skellpreset::package::read(malicious,second).failed(),"Reject malicious ZIP path");
    check(unchanged(),"Path traversal rejection keeps entire prior preset");
    auto oversize=testFile(dir,"oversize");
    createArchive(oversize,"preset.json",std::string(300000,'a'));
    check(skellpreset::package::read(oversize,second).failed(),"Reject compressed oversized entry");
    check(unchanged(),"Oversized ZIP rejection keeps entire prior preset");
    auto malformed=testFile(dir,"malformed");
    createArchive(malformed,"preset.json","not json");
    check(skellpreset::package::read(malformed,second).failed(),"Reject malformed JSON");
    check(unchanged(),"Malformed JSON rejection keeps entire prior preset");
    check(second.name==first.name || second.name=="Wide Ghost Vocal", "Bad input keeps previous successful document");
    dir.deleteRecursively();
    if(failures)std::cerr<<failures<<" package tests failed\n";
    return failures?1:0;
}
