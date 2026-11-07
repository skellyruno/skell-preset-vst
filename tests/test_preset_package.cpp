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
        auto& slot=first.slots[(size_t)s];slot.slotId=(s+3)%10;
        slot.type=s%9;slot.enabled=s%2==0;slot.wet=.1f+(float)s*.08f;
        slot.params.fill(.5f);
    }
    first.macros.fill(.5f);
    first.macros[0]=.86f;
    auto path=testFile(dir,"roundtrip");
    check(skellpreset::package::write(path,first).wasOk(),"Export compressed .skellpreset");
    skellpreset::package::Document second;
    check(skellpreset::package::read(path,second).wasOk(),"Import compressed .skellpreset");
    check(second.name==first.name&&second.bank==first.bank&&second.slots[0].slotId==3
          &&second.slots[9].type==9%9&&second.macros[0]==first.macros[0],"Roundtrip chain/macro/metadata");

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
