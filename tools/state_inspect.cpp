// Offline inspector/editor of extracted plugin payloads, not a complete FLP/FST writer.
#include "../Source/chain/FlRawState.h"
#include <iostream>
#include <fstream>
#include <filesystem>
#include <charconv>
#include <string_view>
namespace {
bool integer(std::string_view s,int& v){auto r=std::from_chars(s.data(),s.data()+s.size(),v);return r.ec==std::errc{}&&r.ptr==s.data()+s.size();}
}
int main(int argc,char** argv) {
    using namespace skell::state;
    const bool edit=argc==7&&std::string_view(argv[1])=="--set-control";
    if(argc!=3&&!edit){std::cerr<<"usage: state_inspect EFFECT PAYLOAD\n       state_inspect --set-control EFFECT PAYLOAD NEW_PAYLOAD INDEX RAW\n";return 2;}
    const int arg=edit?2:1;
    auto k=kind(argv[arg]);if(!k)return 2;
    std::ifstream f(argv[arg+1],std::ios::binary|std::ios::ate);
    if(!f||f.tellg()<4||f.tellg()>4096)return 2;
    const auto n=static_cast<std::streamsize>(f.tellg());f.seekg(0);Bytes b(static_cast<std::size_t>(n));
    if(!f.read(reinterpret_cast<char*>(b.data()),n))return 2;
    auto s=decode(*k,b);if(!s)return 3;
    auto out=encode(*s);if(!out||*out!=b)return 4;
    if(edit){
        int index=0,value=0;
        if((*k!=Kind::limiter&&*k!=Kind::compressor)||!integer(argv[5],index)||!integer(argv[6],value)||index<0||static_cast<std::size_t>(index)>=s->controls.size())return 5;
        s->controls[static_cast<std::size_t>(index)]=value;
        out=encode(*s);if(!out)return 5;
        std::error_code ec;
        // Do not replace an existing file or symlink. Offline, trusted local directory only.
        auto info=std::filesystem::symlink_status(argv[4],ec);
        if(std::filesystem::exists(info)||std::filesystem::is_symlink(info)||(ec&&ec!=std::errc::no_such_file_or_directory))return 6;
        std::ofstream dest(argv[4],std::ios::binary);
        if(!dest.write(reinterpret_cast<const char*>(out->data()),static_cast<std::streamsize>(out->size())))return 6;
        dest.close();return dest?0:6;
    }
    std::cout<<"{\"version\":"<<s->version<<",\"sidechain\":"<<s->sidechain<<",\"exact_ceiling\":"<<(s->exactCeiling?"true":"false")<<",\"report_latency\":"<<(s->reportLatency?"true":"false")<<",\"controls\":[";
    for(std::size_t i=0;i<s->controls.size();++i){if(i)std::cout<<',';std::cout<<s->controls[i];}
    std::cout<<"],\"editor_words\":["<<s->editorWords[0]<<','<<s->editorWords[1]<<"],\"eq_design\":"<<s->eqDesign<<",\"eq_quality\":"<<s->eqQuality<<",\"eq_fade\":"<<s->eqFade<<",\"eq_options\":"<<unsigned(s->eqOptions)<<",\"eq_mask\":"<<s->eqMask<<",\"byte_roundtrip\":true}\n";
}
