#pragma once
#include "../chain/BasicDSP.h"
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <vector>

namespace skellpreset::match {
struct Result {
    int lagSamples=0;
    bool inverted=false;
    float gainDb=0, score=0, confidence=0;
    float rmsDry=0,rmsWet=0;
};
inline double dot(const std::vector<float>& a,const std::vector<float>& b,int lag,bool inverted) {
    double sum=0;
    for(size_t i=0;i<a.size();++i) {
        const long j=(long)i+lag;
        if(j>=0&&j<(long)b.size())sum+=(inverted?-1:1)*(double)a[i]*b[(size_t)j];
    }
    return sum;
}
inline Result analyse(const std::vector<float>& dry,const std::vector<float>& wet,int maxLag=2000) {
    Result o;
    if(dry.size()<128 || wet.size()<128 || maxLag<0) return o;
    // Silent or corrupt pairs cannot support a meaningful MATCH estimate.
    double dryEnergy=0, wetEnergy=0;
    for(const float v:dry) {
        if(!std::isfinite(v))return o;
        dryEnergy+=(double)v*v;
    }
    for(const float v:wet) {
        if(!std::isfinite(v))return o;
        wetEnergy+=(double)v*v;
    }
    if(dryEnergy<1e-12 || wetEnergy<1e-12)return o;
    // Downstream version will add envelope onset correlation and sub-sample alignment.
    double best=-1;
    maxLag=std::min(maxLag,(int)std::min(dry.size(),wet.size())/3);
    for(int lag=-maxLag;lag<=maxLag;++lag) {
        const auto d=dot(dry,wet,lag,false);
        const auto mag=std::abs(d);
        if(mag>best) {best=mag;o.lagSamples=lag;o.inverted=d<0;}
    }
    double a=0,b=0,err=0,den=0;
    for(size_t i=0;i<dry.size();++i) {
        const auto j=(long)i+o.lagSamples;
        if(j<0||j>=(long)wet.size())continue;
        const auto x=dry[i],y=wet[(size_t)j]*(o.inverted?-1.f:1.f);
        a+=x*y;b+=x*x;
    }
    const double gain=b>1e-15?a/b:1;
    for(size_t i=0;i<dry.size();++i) {
        const auto j=(long)i+o.lagSamples;
        if(j<0||j>=(long)wet.size())continue;
        const auto x=dry[i],y=wet[(size_t)j]*(o.inverted?-1.f:1.f);
        err+=(x*gain-y)*(x*gain-y);den+=y*y;
    }
    o.gainDb=dsp::gainToDb((float)std::abs(gain));
    o.score=std::clamp((float)(100.*(1-std::sqrt(err/std::max(den,1e-12)))),0.f,100.f);
    o.confidence=(float)std::clamp(best/(std::sqrt(std::max(dryEnergy*wetEnergy,1e-12))),0.,1.);
    o.rmsDry=std::sqrt((float)(b/std::max((size_t)1,dry.size())));
    o.rmsWet=std::sqrt((float)(den/std::max((size_t)1,dry.size())));
    return o;
}
}
