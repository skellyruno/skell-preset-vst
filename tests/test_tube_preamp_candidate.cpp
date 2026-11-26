#include "../Source/chain/TubePreampCandidate.h"
#include <cstdio>
#include <limits>
using namespace skellpreset::research;
int main(){
    constexpr double tau=6.283185307179586;
    TubeCore direct,aa;direct.set(5.,.12);aa.set(5.,.12);
    double dFund=0.,aFund=0.,dAlias=0.,aAlias=0.;double dfc=0.,afc=0.,dac=0.,aac=0.;
    for(int i=0;i<52800;++i){const double p=tau*i/48000.,x=.7*std::sin(9000.*p);const double d=direct.curve(x),a=aa.process(x);
        if(i>=4800){dFund+=d*std::sin(9000.*p);dfc+=d*std::cos(9000.*p);aFund+=a*std::sin(9000.*p);afc+=a*std::cos(9000.*p);
        dAlias+=d*std::sin(21000.*p);dac+=d*std::cos(21000.*p);aAlias+=a*std::sin(21000.*p);aac+=a*std::cos(21000.*p);}}
    const double d=20.*std::log10(std::hypot(dAlias,dac)/std::hypot(dFund,dfc));
    const double a=20.*std::log10(std::hypot(aAlias,aac)/std::hypot(aFund,afc));
    std::printf("9 kHz tone, 48 kHz, folded third 21 kHz: direct %.3f dBc, ADAA %.3f dBc, improvement %.3f dB\n",d,a,d-a);
    if(!std::isfinite(a)||d-a<3.)return 1;
    for(double sr:{44100.,48000.,96000.,192000.}){
        TubePreampCandidate p;p.prepare(sr);p.set(0.,0.,0.);
        for(int n=0;n<8192;++n){const float x=(float)(.65*std::sin(tau*230.*n/sr));if(p.process(x,0)!=x||p.process(0.f,1)!=0.f)return 2;}
        p.set(3.,.6,3.);double left=0.,right=0.;
        for(int n=0;n<8192;++n){const float x=(float)(.65*std::sin(tau*230.*n/sr));const float l=p.process(x,0),r=p.process(0.f,1);if(!std::isfinite(l)||!std::isfinite(r)||std::abs(l)>8.f)return 3;left+=l*l;right+=r*r;}
        if(left<.01||right!=0.)return 4;
        p.reset();if(!std::isfinite(p.process(std::numeric_limits<float>::quiet_NaN(),0)))return 5;
    }
    for(double sr:{44100.,48000.,96000.,192000.}){
        MicTubeStage stage;stage.prepare(sr,false,12.,1.,6.,12.);
        for(int i=0;i<4096;++i){float l=.3f,r=0.f;stage.process(l,r);if(l!=.3f||r!=0.f)return 6;}
        stage.set(true,0.,.35,1.,-2.);
        float prior=.3f;double difference=0.;
        for(int i=0;i<(int)(sr*.05);++i){float l=.3f,r=0.f;stage.process(l,r);
            if(!std::isfinite(l)||r!=0.f||std::abs(l-prior)>.04f)return 7;
            difference+=std::abs(l-.3f);prior=l;}
        if(difference<.001)return 8;
        stage.set(false,12.,1.,6.,12.);
        for(int i=0;i<(int)(sr*.03);++i){float l=.3f,r=0.f;stage.process(l,r);if(!std::isfinite(l)||r!=0.f)return 9;}
        float l=.3f,r=0.f;stage.process(l,r);if(l!=.3f||r!=0.f)return 10;
        stage.prepare(sr,true,0.,0.,0.,0.);l=.4f;r=-.2f;stage.process(l,r);if(l!=.4f||r!=-.2f)return 11;
    }
    std::puts("PASS: four-rate finite processing, exact neutral bypass, independent stereo, reset, 20 ms parameter smoothing and off transition");
}
