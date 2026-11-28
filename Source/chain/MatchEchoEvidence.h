#pragma once
#include "LiveVocalMatch.h"

namespace skellpreset::liveMatch {
// Bounded worker-only evidence from a paired earlier phrase. Adapted from
// Skell Sound Match's isolated_echo_period / pairing_audit research, without
// the offline Python dependencies or any target-dependent replay filter.
struct EchoEvidence { bool paired=false; float seconds=0.f, strength=0.f; };
inline EchoEvidence estimateEchoEvidence(const Audio& dry,const Audio& ref,size_t trainEnd,
                                       const std::atomic<bool>* cancel=nullptr) {
    EchoEvidence result;
    if(!std::isfinite(dry.rate)||dry.rate<8000.||dry.rate>192000.||dry.rate!=ref.rate||
       dry.stereo.size()%2||ref.stereo.size()!=dry.stereo.size()||trainEnd>dry.frames()/2||trainEnd<dry.rate*2.)return result;
    const auto stopped=[&]{return cancel&&cancel->load(std::memory_order_relaxed);};
    // Reduce to approximately 2 kHz via block averaging. Remove DC with a
    // first difference; compare both channel directions independently.
    const size_t stride=(size_t)std::max(1.,std::round(dry.rate/2000.));
    const size_t n=std::min(trainEnd,(size_t)(dry.rate*8.))/stride;
    if(n<3000)return result;
    std::vector<double> x(n),l(n),r(n);double previousX=0.,previousL=0.,previousR=0.;
    for(size_t i=0;i<n;++i){if((i&255u)==0&&stopped())return {};
        double a=0.,b=0.,c=0.;for(size_t k=0;k<stride;++k){const size_t j=(i*stride+k)*2;
            const float dl=dry.stereo[j],dr=dry.stereo[j+1],rl=ref.stereo[j],rr=ref.stereo[j+1];
            if(!std::isfinite(dl)||!std::isfinite(dr)||!std::isfinite(rl)||!std::isfinite(rr))return {};
            a+=.5*((double)dl+dr);b+=rl;c+=rr;}
        a/=stride;b/=stride;c/=stride;x[i]=a-previousX;l[i]=b-previousL;r[i]=c-previousR;previousX=a;previousL=b;previousR=c;
    }
    // Three separate direct-signal checks. Unrelated takes, large offsets,
    // sustained silence and fully wet prints do not license paired inference.
    unsigned agree=0;for(size_t k=0;k<3;++k){double xx=0.,yy=0.,xy=0.;
        for(size_t i=n*k/3;i<n*(k+1)/3;++i){const double y=.5*(l[i]+r[i]);xx+=x[i]*x[i];yy+=y*y;xy+=x[i]*y;}
        if(xx>1.e-9&&yy>1.e-9&&xy/std::sqrt(xx*yy)>.55)++agree;}
    if(agree!=3||stopped())return result;
    result.paired=true;
    double xx=0.,xl=0.,xr=0.;for(size_t i=0;i<n;++i){xx+=x[i]*x[i];xl+=x[i]*l[i];xr+=x[i]*r[i];}
    if(xx<1.e-9)return result;
    for(size_t i=0;i<n;++i){l[i]-=x[i]*xl/xx;r[i]-=x[i]*xr/xx;}
    const double rate=dry.rate/stride;
    const size_t low=(size_t)std::ceil(.055*rate),high=std::min((size_t)std::floor(.905*rate),n/3);
    if(high<=low+2)return result;
    std::vector<double> scores(high+1);double totalResidual=0.;for(size_t i=0;i<n;++i)totalResidual+=l[i]*l[i]+r[i]*r[i];
    if(totalResidual<.004*xx)return result;
    for(size_t lag=low;lag<=high;++lag){if((lag&15u)==0&&stopped())return {};
        double u=0.,v=0.,e=0.,a=0.,b=0.;for(size_t i=lag;i<n;++i){const double s=x[i-lag];e+=s*s;u+=s*l[i];v+=s*r[i];a+=l[i]*l[i];b+=r[i]*r[i];}
        if(e>1.e-9&&a+b>1.e-9)scores[lag]=std::sqrt((u*u+v*v)/(e*(a+b)));
    }
    size_t best=low;for(size_t i=low+1;i<=high;++i)if(scores[i]>scores[best])best=i;
    // Reject broad or ambiguous peaks (e.g. periodic sustained notes).
    const size_t exclusion=(size_t)std::ceil(.018*rate);double rival=0.;
    for(size_t i=low;i<=high;++i)if(i+exclusion<best||i>best+exclusion)rival=std::max(rival,scores[i]);
    if(scores[best]>=.32&&scores[best]>=rival*1.18&&scores[best]-rival>=.045){result.seconds=(float)(best/rate);result.strength=(float)scores[best];}
    return result;
}
// Gain-invariant two-tap identification on the actual rendered earlier
// phrase. Fit direct and lagged dry simultaneously to avoid mistaking a
// pitched vocal's autocorrelation for a return. This is a diagnostic only.
inline float pairedEchoRatio(const Audio& dry,const float* target,size_t begin,size_t end,float seconds) noexcept {
    if(!target||!std::isfinite(dry.rate)||dry.rate<8000.||dry.rate>192000.||!std::isfinite(seconds)||seconds<.055f||seconds>.906f||begin>=end||end>dry.frames())return -1.f;
    const size_t stride=(size_t)std::max(1.,std::round(dry.rate/2000.));
    const size_t lag=(size_t)std::round(seconds*dry.rate/stride)*stride;
    if(end-begin<lag+stride*4)return -1.f;
    const size_t first=std::max(begin,lag)+stride;
    double xx=0.,zz=0.,xz=0.;std::array<double,2> xy{},zy{},yy{};
    auto mid=[&](size_t start){double sum=0.;for(size_t k=0;k<stride;++k)sum+=.5*((double)dry.stereo[2*(start+k)]+dry.stereo[2*(start+k)+1]);return sum;};
    for(size_t i=first;i+stride<=end;i+=stride){const double x=mid(i)-mid(i-stride),z=mid(i-lag)-mid(i-lag-stride);xx+=x*x;zz+=z*z;xz+=x*z;
        for(size_t ch=0;ch<2;++ch){double y=0.;for(size_t k=0;k<stride;++k){const size_t j=i-begin+k;y+=(double)target[2*j+ch]-target[2*(j-stride)+ch];}xy[ch]+=x*y;zy[ch]+=z*y;yy[ch]+=y*y;}}
    const double det=xx*zz-xz*xz;if(xx<1.e-8||zz<1.e-8||det<.02*xx*zz)return -1.f;
    double direct=0.,echo=0.;for(size_t ch=0;ch<2;++ch){const double a=(xy[ch]*zz-zy[ch]*xz)/det,b=(zy[ch]*xx-xy[ch]*xz)/det;direct+=a*a;echo+=b*b;}
    if(direct<1.e-6||xx*direct<.09*(yy[0]+yy[1])||!std::isfinite(direct)||!std::isfinite(echo))return -1.f;
    return (float)std::sqrt(echo/direct);
}
inline float echoRatioError(float actual,float target) noexcept {
    if(actual<0.f||target<0.f||!std::isfinite(actual)||!std::isfinite(target))return 1.e6f;
    return (float)std::abs(std::log((actual+.01f)/(target+.01f)));
}
}
