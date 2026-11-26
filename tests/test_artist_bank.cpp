#include "../Source/chain/ArtistBank.h"
#include "../Source/chain/PresetPackage.h"
#include <juce_cryptography/juce_cryptography.h>
#include <iostream>
#include <stdexcept>
using namespace skellpreset;
namespace {
void require(bool good,const char* message){if(!good)throw std::runtime_error(message);}
juce::String digest(const juce::File& file){return juce::SHA256(file).toHexString();}
juce::String bank(const juce::File& preset,int generation,const juce::String& previous={}) {
    juce::MemoryBlock bytes;require(preset.loadFileAsData(bytes),"fixture read");
    const auto h=juce::SHA256(bytes).toHexString();
    juce::Array<juce::var> history;history.add(h);if(previous.isNotEmpty())history.add(previous);
    juce::var row(new juce::DynamicObject());auto* r=row.getDynamicObject();
    r->setProperty("id","example_main");r->setProperty("artistId","qa");r->setProperty("artist","QA Artist");r->setProperty("title","Example Main");r->setProperty("type","main");r->setProperty("pair","example");r->setProperty("sha256",h);r->setProperty("history",history);r->setProperty("payload",juce::Base64::toBase64(bytes.getData(),bytes.getSize()));
    juce::var result(new juce::DynamicObject());result.getDynamicObject()->setProperty("schema",1);result.getDynamicObject()->setProperty("generation",generation);result.getDynamicObject()->setProperty("entries",juce::Array<juce::var>{row});return juce::JSON::toString(result,true);
}
}
int main(int argc,char** argv){
    auto root=juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("SkellArtistBankTest-"+juce::Uuid().toString());
    struct Cleanup{juce::File file;~Cleanup(){file.deleteRecursively();}} cleanup{root};
    try{
        require(root.createDirectory().wasOk(),"fixture directory");
        auto a=root.getChildFile("a.skellpreset"),b=root.getChildFile("b.skellpreset");package::Document doc;doc.name="QA Artist Main";doc.slots[0].type=4;doc.slots[0].enabled=true;
        require(package::write(a,doc).wasOk(),"fixture package A");doc.outputDb=-2;require(package::write(b,doc).wasOk(),"fixture package B");
        const auto one=bank(a,1),two=bank(b,2,digest(a));auto lib=root.getChildFile("library");
        auto first=artistbank::sync(lib,one);require(first.error.isEmpty()&&first.installed==1&&first.metadata.size()==1,"initial embedded install");
        auto dest=lib.getChildFile("Presets/qa--example_main [Trial].skellpreset");require(digest(dest)==digest(a),"exact bytes");
        require(artistbank::sync(lib,one).current==1,"idempotent install");
        require(artistbank::sync(lib,{}).metadata.size()==1,"artist metadata retained without embedded payload");
        auto update=artistbank::sync(lib,two);require(update.updated==1&&digest(dest)==digest(b),"revision update");
        require(digest(lib.getChildFile("SoundMatchHistory/example_main--trial/"+digest(a)+".skellpreset"))==digest(a),"immutable backup");
        require(artistbank::sync(lib,one).conflicts==1&&digest(dest)==digest(b),"older binary cannot roll back");
        doc.outputDb=-6;require(package::write(dest,doc).wasOk(),"local edit");const auto local=digest(dest);
        require(artistbank::sync(lib,two).conflicts==1&&digest(dest)==local,"local edits survive");
        auto unowned=root.getChildFile("unowned");auto localPath=unowned.getChildFile("Presets/qa--example_main [Trial].skellpreset");require(localPath.getParentDirectory().createDirectory().wasOk()&&a.copyFileTo(localPath),"unowned collision fixture");
        require(artistbank::sync(unowned,two).conflicts==1&&digest(localPath)==digest(a),"unmanaged collision survives");
        auto malformed=two.replace(digest(b),juce::String::repeatedString("0",64));auto invalid=root.getChildFile("invalid");require(artistbank::sync(invalid,malformed).error.isNotEmpty()&&!invalid.exists(),"validate before writes");
        if(argc>1){auto privateBank=juce::File(juce::String::fromUTF8(argv[1])).loadFileAsString();auto target=root.getChildFile("private");auto result=artistbank::sync(target,privateBank);require(result.error.isEmpty()&&result.conflicts==0&&result.installed>0&&result.metadata.size()==(size_t)result.installed,"private catalog native load");require(artistbank::sync(target,privateBank).current==result.installed,"private repeat");int mains=0,adlibs=0;for(const auto& item:result.metadata){mains+=item.second.type=="main";adlibs+=item.second.type=="adlib";}std::cout<<"Private native bank: "<<result.installed<<" packages, "<<mains<<" main, "<<adlibs<<" adlib\n";}
        std::cout<<"PASS: install, byte identity, idempotence, upgrade, history, downgrade protection, local edit, unmanaged collision, malformed bank\n";return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
