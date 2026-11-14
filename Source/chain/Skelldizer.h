#pragma once
// Original Skelly dynamics/saturation processor. NOT a clone of Soundgoodizer.
// No allocation, locks or transcendental work in the audio callback except tanh.
#include <algorithm>
#include <array>
#include <cmath>
namespace skellpreset::dsp {
struct Skelldizer {
    double sampleRate=48000.0, envelope=0.0, lowL=0.0, lowR=0.0;
    float amount=.45f, character=.5f, tone=.5f, output=.5f;
    float smoothAmount=.45f, smoothCharacter=.5f, smoothTone=.5f, smoothOutput=.5f;
    double controlPole=.9996, detectorAttack=.99, detectorRelease=.999;
    void prepare(double sr) noexcept {
        sampleRate=std::clamp(sr,8000.0,192000.0);
        controlPole=std::exp(-1.0/(sampleRate*.012));
        detectorAttack=std::exp(-1.0/(sampleRate*.004));
        detectorRelease=std::exp(-1.0/(sampleRate*.110));
        envelope=lowL=lowR=0.0;
        smoothAmount=amount;smoothCharacter=character;smoothTone=tone;smoothOutput=output;
    }
    void set(float a,float c,float t,float o) noexcept {
        amount=std::clamp(std::isfinite(a)?a:0.f,0.f,1.f);
        character=std::clamp(std::isfinite(c)?c:.5f,0.f,1.f);
        tone=std::clamp(std::isfinite(t)?t:.5f,0.f,1.f);
        output=std::clamp(std::isfinite(o)?o:.5f,0.f,1.f);
    }
    std::array<float,2> process(float left,float right) noexcept {
        auto smooth=[this](float& current,float target) noexcept {
            current=(float)(controlPole*current+(1.0-controlPole)*target);
        };
        smooth(smoothAmount,amount);smooth(smoothCharacter,character);
        smooth(smoothTone,tone);smooth(smoothOutput,output);
        const float inL=std::isfinite(left)?std::clamp(left,-8.f,8.f):0.f;
        const float inR=std::isfinite(right)?std::clamp(right,-8.f,8.f):0.f;
        const double peak=std::max(std::abs(inL),std::abs(inR));
        const double pole=peak>envelope?detectorAttack:detectorRelease;
        envelope=pole*envelope+(1.0-pole)*peak;
        if(envelope<1.e-20)envelope=0.0;
        // Stereo-linked upward density, then smooth saturation and makeup trim.
        const float threshold=.20f-.15f*smoothAmount;
        const float ratio=1.f+3.5f*smoothAmount;
        const float level=(float)std::max(envelope,1.e-7);
        const float compression=level>threshold
            ?std::pow(threshold/level,1.f-1.f/ratio):1.f;
        const float drive=1.f+5.0f*smoothAmount*(.65f+.7f*smoothCharacter);
        const float saturation=1.f+2.5f*smoothAmount*smoothCharacter;
        const float tilt=2.f*smoothTone-1.f;
        const float lowPole=(float)std::exp(-2.0*3.141592653589793*850.0/sampleRate);
        const float input[2]{inL,inR};
        const float makeup=std::pow(10.f,(smoothOutput-.5f)*18.f/20.f);
        std::array<float,2> out{};
        for(int ch=0;ch<2;++ch) {
            double& low=ch==0?lowL:lowR;
            low=lowPole*low+(1.f-lowPole)*input[ch];
            if(std::abs(low)<1.e-20)low=0.0;
            const float split=(float)low;
            const float shaped=(split*(1.f-.20f*tilt)+(input[ch]-split)*(1.f+.28f*tilt))
                 *drive*compression;
            const float wet=std::tanh(shaped*saturation)/saturation;
            // Keep modest signal gain as amount grows; output is explicitly trimmed.
            const float matched=wet*(1.f+.6f*smoothAmount)*makeup;
            out[(size_t)ch]=std::isfinite(matched)?std::clamp(matched,-4.f,4.f):0.f;
        }
        return out;
    }
};
} // namespace skellpreset::dsp
