#include "../Source/chain/BasicDSP.h"
#include <cstdlib>
#include <cstdio>
using skellpreset::dsp::DetectorHistory;
int main(){
    DetectorHistory h;h.prepare(100,31);
    int cursor=0;
    for(int frames:{1,31,7,30,2,17,31,5,31,31,31,31}){
        for(int n=0;n<frames;++n){float x=static_cast<float>(cursor+n+1);h.push(x,-x);}
        for(int delay:{0,1,17,100})for(int n=0;n<frames;++n){
            float expected=static_cast<float>(std::max(0,cursor+n+1-delay));
            auto got=h.at(frames,n,delay);
            if(got[0]!=expected || got[1]!=-expected)return EXIT_FAILURE;
        }
        cursor+=frames;
    }
    std::puts("PASS shared stereo detector history, mixed block sizes and delay taps");
}
