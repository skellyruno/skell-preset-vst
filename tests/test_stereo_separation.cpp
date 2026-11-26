// Native output-stage stereo separation: verification for
// skellpreset::dsp::MixerStereoSeparation.
//
// Covers the mixer-style unity-mid separation candidate, the -96..+96 raw range,
// 10 ms sample-count smoothing, neutral initialization and exact neutral
// bypass, mono compatibility, mid/side response, peak safety, block-size
// consistency and preservation of the documented matrix.
#include "../Source/chain/MixerStereoSeparation.h"
#include <array>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <random>
#include <vector>

using skellpreset::dsp::MixerStereoSeparation;

namespace {
constexpr double pi = 3.14159265358979323846;

struct Stereo {
    std::vector<float> left, right;
    size_t size() const noexcept { return left.size(); }
};

Stereo makeNoise(size_t frames, unsigned seed) {
    std::mt19937 rng(seed);
    std::uniform_real_distribution<float> dist(-.3f, .3f);
    Stereo s;
    s.left.resize(frames);
    s.right.resize(frames);
    for (size_t i = 0; i < frames; ++i) {
        s.left[i] = dist(rng);
        s.right[i] = dist(rng);
    }
    return s;
}

Stereo processRate(const Stereo& in, int raw, double rate, int block) {
    MixerStereoSeparation sep;
    sep.prepare(rate);
    sep.setRaw(raw);
    Stereo out = in;
    // Run the first block from neutral so the 10 ms ramp engages exactly as it
    // does when a host automates the control.
    for (size_t offset = 0; offset < out.size(); offset += (size_t)block) {
        const int n = (int)std::min((size_t)block, out.size() - offset);
        sep.process(out.left.data() + offset, out.right.data() + offset, n);
    }
    return out;
}

float peak(const Stereo& s) {
    float p = 0.f;
    for (size_t i = 0; i < s.size(); ++i)
        p = std::max(p, std::max(std::abs(s.left[i]), std::abs(s.right[i])));
    return p;
}

float correlation(const Stereo& s) {
    double ll = 0., rr = 0., lr = 0.;
    for (size_t i = 0; i < s.size(); ++i) {
        ll += (double)s.left[i] * s.left[i];
        rr += (double)s.right[i] * s.right[i];
        lr += (double)s.left[i] * s.right[i];
    }
    if (ll <= 0. || rr <= 0.) return 0.f;
    return (float)(lr / std::sqrt(ll * rr));
}

double rms(const std::vector<float>& v) {
    double sum = 0.;
    for (float x : v) sum += (double)x * x;
    return v.empty() ? 0. : std::sqrt(sum / (double)v.size());
}
} // namespace

int main() {
    constexpr std::array<double, 4> rates{44100., 48000., 96000., 192000.};

    // ---- The published law, sampled across the whole raw range ----------
    for (int raw = MixerStereoSeparation::minimumRaw;
         raw <= MixerStereoSeparation::maximumRaw; ++raw) {
        const float s = (float)raw / 96.f;
        const float a = 1.f - s * .5f;
        const float b = s * .5f;
        assert(std::abs(MixerStereoSeparation::matrixA(raw) - a) < 2.e-7f);
        assert(std::abs(MixerStereoSeparation::matrixB(raw) - b) < 2.e-7f);
        assert(std::abs(MixerStereoSeparation::matrixA(raw)+MixerStereoSeparation::matrixB(raw)-1.f)<2.e-7f);
    }
    // Neutral is the identity, and the ends of the range behave as documented.
    assert(MixerStereoSeparation::matrixA(0) == 1.f);
    assert(MixerStereoSeparation::matrixB(0) == 0.f);
    assert(MixerStereoSeparation::peakGainBound(0) == 1.f);
    // -96 is the widest, +96 the closest to mono. Both stay finite.
    assert(MixerStereoSeparation::matrixA(-96) == 1.5f);
    assert(MixerStereoSeparation::matrixA(96) == .5f);
    assert(std::abs(MixerStereoSeparation::matrixB(96) - .5f) < 1.e-7f);
    // Out-of-range raw values clamp rather than extrapolate.
    assert(MixerStereoSeparation::matrixA(-400) == MixerStereoSeparation::matrixA(-96));
    assert(MixerStereoSeparation::matrixA(400) == MixerStereoSeparation::matrixA(96));

    for (double rate : rates) {
        const auto noise = makeNoise(4096, 100u + (unsigned)rate);

        // ---- Neutral null: bit-exact identity, including both directions --
        {
            MixerStereoSeparation sep;
            sep.prepare(rate);
            assert(sep.neutral());
            Stereo out = noise;
            sep.process(out.left.data(), out.right.data(), (int)out.size());
            for (size_t i = 0; i < noise.size(); ++i) {
                assert(out.left[i] == noise.left[i]);
                assert(out.right[i] == noise.right[i]);
            }
            // Re-selecting neutral after leaving it returns to exact bypass.
            sep.setRaw(72);
            sep.process(out.left.data(), out.right.data(), 1024);
            sep.setRaw(0);
            sep.process(out.left.data(), out.right.data(), (int)out.size() - 1024);
            assert(sep.neutral());
        }

        // ---- Settled transfer matches the matrix exactly -------------------
        for (int raw = -96; raw <= 96; ++raw) {
            const auto out = processRate(noise, raw, rate, 127);
            const size_t settle = (size_t)(rate * .02) + 2;
            const float a = MixerStereoSeparation::matrixA(raw);
            const float b = MixerStereoSeparation::matrixB(raw);
            float error = 0.f;
            for (size_t i = settle; i < out.size(); ++i) {
                const float expectedL = a * noise.left[i] + b * noise.right[i];
                const float expectedR = a * noise.right[i] + b * noise.left[i];
                error = std::max(error, std::abs(out.left[i] - expectedL));
                error = std::max(error, std::abs(out.right[i] - expectedR));
            }
            assert(error < 2.e-6f);
            assert(std::isfinite(out.left[0]) && std::isfinite(out.right[0]));
        }

        // ---- Mono compatibility: a mono source never gains a side signal ---
        for (int raw = -96; raw <= 96; ++raw) {
            Stereo mono;
            mono.left = noise.left;
            mono.right = noise.left;
            const auto out = processRate(mono, raw, rate, 512);
            for (size_t i = 0; i < out.size(); ++i)
                assert(out.left[i] == mono.left[i] && out.right[i] == mono.right[i]);
            // And the mono sum is the documented (a+b) gain, not a phase flip.
            const float expected = MixerStereoSeparation::matrixA(raw)
                                 + MixerStereoSeparation::matrixB(raw);
            const size_t settle = (size_t)(rate * .02) + 2;
            for (size_t i = settle; i < out.size(); i += 97)
                assert(std::abs(out.left[i] - expected * mono.left[i]) < 2.e-6f);
        }

        // ---- Mid/side response: widening raises side, +96 lowers it --------
        {
            Stereo source;
            source.left.resize(8192);
            source.right.resize(8192);
            for (size_t i = 0; i < source.size(); ++i) {
                const float mid = .2f * (float)std::sin(2. * pi * 220. * (double)i / rate);
                const float side = .1f * (float)std::sin(2. * pi * 660. * (double)i / rate);
                source.left[i] = mid + side;
                source.right[i] = mid - side;
            }
            auto sideEnergy = [&](const Stereo& s) {
                std::vector<float> side(s.size());
                for (size_t i = 0; i < s.size(); ++i)
                    side[i] = (s.left[i] - s.right[i]) * .5f;
                return rms(side);
            };
            std::vector<float> midOfSource(source.size());
            for (size_t i = 0; i < source.size(); ++i)
                midOfSource[i] = (source.left[i] + source.right[i]) * .5f;

            const auto neutral = processRate(source, 0, rate, 512);
            const auto wider = processRate(source, -96, rate, 512);
            const auto merged = processRate(source, 96, rate, 512);
            const size_t settle = (size_t)(rate * .02) + 2;
            std::vector<float> wSide(wider.size() - settle), mSide(merged.size() - settle);
            for (size_t i = settle; i < wider.size(); ++i) {
                wSide[i - settle] = (wider.left[i] - wider.right[i]) * .5f;
                mSide[i - settle] = (merged.left[i] - merged.right[i]) * .5f;
            }
            const float baseSide = (float)sideEnergy(neutral);
            assert(baseSide > 0.f);
            assert(rms(wSide) > baseSide * 1.2);
            assert(rms(mSide) < baseSide * .8);
            // Mid content is scaled, never inverted: the waveform keeps sign.
            for (size_t i = settle; i < neutral.size(); i += 211) {
                const float mid = (merged.left[i] + merged.right[i]) * .5f;
                assert(std::abs(mid-midOfSource[i])<2.e-6f);
                assert(merged.left[i]==merged.right[i]);
                const float wideMid=(wider.left[i]+wider.right[i])*.5f;
                assert(std::abs(wideMid-midOfSource[i])<2.e-6f);
            }
            // Widening raises the side/mid ratio, merging lowers it. The
            // supplied law over-separates at the extreme (b turns negative),
            // so correlation is only required to *decrease* monotonically --
            // it may cross zero. Merging must always move toward +1.
            const float baseCorr = correlation(neutral);
            const float wideCorr = correlation(wider);
            assert(wideCorr < baseCorr);
            assert(correlation(merged) > baseCorr);
            assert(correlation(merged) <= 1.f + 1.e-6f);
            // Mid content is still never inverted for a mono source, which is
            // covered by the exact a+b check above.
        }

        // ---- Block-size consistency and no latency -------------------------
        for (int raw : {-96, -33, 0, 41, 96}) {
            const auto block127 = processRate(noise, raw, rate, 127);
            const auto block512 = processRate(noise, raw, rate, 512);
            for (size_t i = 0; i < noise.size(); ++i) {
                assert(block127.left[i] == block512.left[i]);
                assert(block127.right[i] == block512.right[i]);
            }
            // An impulse must emerge on the very first sample of its own block.
            MixerStereoSeparation sep;
            sep.prepare(rate);
            sep.setRaw(0);
            Stereo impulse;
            impulse.left.assign(8, 0.f);
            impulse.right.assign(8, 0.f);
            impulse.left[0] = 1.f;
            sep.process(impulse.left.data(), impulse.right.data(), 8);
            assert(impulse.left[0] == 1.f && impulse.right[0] == 0.f);
        }

        // ---- Smoothing is sample-count based and monotonic ------------------
        {
            MixerStereoSeparation sep;
            sep.prepare(rate);
            const int expectedRamp = std::max(1, (int)std::lround(rate * .01));
            assert(sep.rampLength() == expectedRamp);
            // Drive the left channel only and keep the right silent, so the
            // observed left output is exactly a*L and isolates coefficient a.
            sep.setRaw(96);
            std::vector<float> l(8192, .25f), r(8192, 0.f);
            sep.process(l.data(), r.data(), (int)l.size());
            const float target = MixerStereoSeparation::matrixA(96);
            const float settling = 1.5f; // widest a, bounds the ramp range
            assert(std::abs(l.back() - (target * .25f)) < 2.e-6f);
            assert(l.back() <= settling * .25f);
            // Exactly one ramp window after the change the value is monotonic
            // toward the target and has not overshot it.
            MixerStereoSeparation probe;
            probe.prepare(rate);
            probe.setRaw(96);
            assert(!probe.neutral());
            std::vector<float> pl((size_t)expectedRamp + 8, .25f), pr((size_t)expectedRamp + 8, 0.f);
            probe.process(pl.data(), pr.data(), (int)pl.size());
            for (size_t i = 1; i < pl.size(); ++i)
                assert(pl[i] <= pl[i - 1] + 1.e-7f);
            // The silent right channel still receives the b*L cross-feed, which
            // is the whole point of a separation matrix rather than a gain.
            const float crossFeed = MixerStereoSeparation::matrixB(96) * .25f;
            assert(std::abs(pr.back() - crossFeed) < 2.e-6f);
        }

        // ---- Peak safety: bounded by the documented matrix gain ------------
        for (int raw = -96; raw <= 96; ++raw) {
            const auto out = processRate(noise, raw, rate, 256);
            const float inPeak = peak(noise);
            const float bound = MixerStereoSeparation::peakGainBound(raw) * inPeak;
            // +1e-6 absorbs the 10 ms ramp crossing between two matrix states.
            assert(peak(out) <= bound + 1.e-6f);
        }
    }

    // ---- Repeated engage/disengage never drifts away from neutral ----------
    {
        MixerStereoSeparation sep;
        sep.prepare(48000.);
        // Each cycle gets a fresh buffer: the module transforms in place, so
        // reusing one buffer would measure its own accumulated output.
        for (int cycle = 0; cycle < 64; ++cycle) {
            sep.setRaw(cycle % 2 ? 96 : -96);
            std::vector<float> l(512, .3f), r(512, -.2f);
            sep.process(l.data(), r.data(), (int)l.size());
        }
        // Returning to neutral glides over the documented 10 ms window and then
        // restores bit-exact transparency, with no residual ramp or drift.
        sep.setRaw(0);
        const int ramp = (int)std::lround(48000. * .01);
        std::vector<float> warm(ramp + 16, 0.f), warmR(ramp + 16, 0.f);
        sep.process(warm.data(), warmR.data(), (int)warm.size());
        assert(sep.neutral());
        std::vector<float> l(4096), r(4096);
        for (size_t i = 0; i < l.size(); ++i) {
            l[i] = .3f + .01f * (float)(i % 7);
            r[i] = -.2f - .005f * (float)(i % 5);
        }
        const auto expectedL = l, expectedR = r;
        sep.process(l.data(), r.data(), (int)l.size());
        assert(sep.neutral());
        for (size_t i = 0; i < l.size(); ++i) {
            assert(l[i] == expectedL[i]);
            assert(r[i] == expectedR[i]);
        }
    }

    // A restored control must take effect on the first frame, including
    // the hard-panned impulse. Retargeting midway must never shift the mid.
    for (double rate : rates) {
        MixerStereoSeparation sep;
        sep.prepare(rate);
        sep.setRaw(-96);
        sep.reset();
        float l=.25f,r=0.f;
        sep.process(&l,&r,1);
        assert(l==.375f && r==-.125f);
        for(int raw : {96,-37,0,51,-96,96,0}) {
            sep.setRaw(raw);
            const int partial=std::max(1,sep.rampLength()/3);
            for(int n=0;n<partial;++n) {
                l=.375f;r=-.125f;
                sep.process(&l,&r,1);
                assert(std::abs((l+r)-.25f)<2.e-7f);
            }
        }
        sep.setRaw(96);sep.reset();
        l=.25f;r=0.f;sep.process(&l,&r,1);
        assert(l==.125f && r==.125f);
        sep.setRaw(0);sep.reset();
        l=.25f;r=0.f;sep.process(&l,&r,1);
        assert(l==.25f && r==0.f);
    }
    std::printf("stereo separation: all checks passed\n");
    return 0;
}
