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
    double fractionalLagSamples=0.0; // parabolic refinement; not applied to audio
    float driftSamples=0.f;          // estimated lag difference across the performance
    bool driftSuspected=false;       // advisory only, not a correction pass
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
    // Interpolate the local cross-correlation summit. Sub-sample alignment
    // is a confidence/inspection cue in this phase, not hidden delay compensation.
    const double aPrev=std::abs(dot(dry,wet,o.lagSamples-1,false));
    const double aMid=std::abs(dot(dry,wet,o.lagSamples,false));
    const double aNext=std::abs(dot(dry,wet,o.lagSamples+1,false));
    const double curvature=aPrev-2*aMid+aNext;
    const double fine=std::abs(curvature)>1e-15
        ? std::clamp(.5*(aPrev-aNext)/curvature,-.5,.5):0.;
    o.fractionalLagSamples=(double)o.lagSamples+fine;
    // Compare early/late correlation peaks over short, distinct windows.
    // Deliberately only probe close to the global alignment; time-stretching
    // and nonlinear processing can make broader lag estimates ambiguous.
    if (dry.size()>=4096 && wet.size()>=4096) {
        const auto window=std::min(dry.size()/5,wet.size()/5);
        auto localLag=[&](size_t start) {
            int optimum=o.lagSamples;
            double highest=-1;
            for(int lag=o.lagSamples-24;lag<=o.lagSamples+24;lag++) {
                double d=0;
                for(size_t i=start;i<start+window && i<dry.size();++i) {
                    const long j=(long)i+lag;
                    if(j>=0&&j<(long)wet.size()) d+=(double)dry[i]*wet[(size_t)j];
                }
                if(std::abs(d)>highest){highest=std::abs(d);optimum=lag;}
            }
            return optimum;
        };
        const int early=localLag(dry.size()/10);
        const int late=localLag(dry.size()*7/10);
        o.driftSamples=(float)(late-early);
        o.driftSuspected=std::abs(o.driftSamples)>=2.f;
    }
    o.gainDb=dsp::gainToDb((float)std::abs(gain));
    o.score=std::clamp((float)(100.*(1-std::sqrt(err/std::max(den,1e-12)))),0.f,100.f);
    o.confidence=(float)std::clamp(best/(std::sqrt(std::max(dryEnergy*wetEnergy,1e-12))),0.,1.);
    o.rmsDry=std::sqrt((float)(b/std::max((size_t)1,dry.size())));
    o.rmsWet=std::sqrt((float)(den/std::max((size_t)1,dry.size())));
    return o;
}
}
