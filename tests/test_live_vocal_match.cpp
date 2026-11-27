#include "../Source/chain/LiveVocalMatch.h"
#include <iostream>
#include <limits>
#include <vector>
using namespace skellpreset::liveMatch;
int main(){
    for(double rate:{44100.,48000.,96000.,192000.}){
        Capture c;Profile p;bool done=false;c.start();
        std::vector<float> b(257,0.f);
        for(int n=0;n<rate*20/257+1;++n)c.push(b.data(),nullptr,257,rate);
        if(!c.read(p,done)||!done||p.valid())return 1;
        c.cancel();if(c.read(p,done))return 2;
        c.start();if(c.read(p,done))return 3;
        int frame=0;
        for(int n=0;n<rate*7/257+1;++n){for(auto& x:b)x=.12f*std::sin(6.283185307179586*220.*frame++/rate);c.push(b.data(),nullptr,257,rate);}
        if(!c.read(p,done)||!done||!p.valid()||p.lowDb<-4.f)return 4;
        const auto r=fit(p);if(r.low>=0||r.ratio<1.5f||r.ratio>4.f)return 5;
        if(comparable(p,p))return 6;
        Profile ref=p;ref.highDb+=8;if(!comparable(p,ref)||fit(p,&ref).high<=0)return 7;
        c.cancel();c.start();std::fill(b.begin(),b.end(),1.f);
        for(int n=0;n<rate*7/257+1;++n)c.push(b.data(),nullptr,257,rate);
        if(!c.read(p,done)||!done||p.valid())return 8;
        c.cancel();c.start();std::fill(b.begin(),b.end(),std::numeric_limits<float>::quiet_NaN());
        for(int n=0;n<rate*20/257+1;++n)c.push(b.data(),nullptr,257,rate);
        if(!c.read(p,done)||!done||p.valid())return 9;
    }
    std::cout<<"Live vocal match capture and bounded profile decisions passed at four sample rates\n";
}
