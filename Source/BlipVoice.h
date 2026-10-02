#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include "SphereModel.h"

namespace blip
{
// Værdier som processoren opdaterer hver blok, og som alle stemmer læser
struct VoiceShared
{
    std::array<float, kNumSounds> weights {};   // kuglens blanding lige nu
    float cutoffHz = 1700.0f;
    float reso = 0.2f;
    float glideSec = 0.08f;
    float lastNoteHz = 0.0f;                     // til glide mellem toner
};

struct BlipSound : public juce::SynthesiserSound
{
    bool appliesToNote (int) override { return true; }
    bool appliesToChannel (int) override { return true; }
};

class BlipVoice : public juce::SynthesiserVoice
{
public:
    explicit BlipVoice (VoiceShared& s) : shared (s) { ks.resize (4096, 0.0f); }

    bool canPlaySound (juce::SynthesiserSound* s) override { return dynamic_cast<BlipSound*> (s) != nullptr; }

    void setADSR (const juce::ADSR::Parameters& p) { adsr.setParameters (p); }

    void prepare (double sampleRate)
    {
        sr = (float) sampleRate;
        adsr.setSampleRate (sampleRate);
    }

    void startNote (int midiNote, float velocity, juce::SynthesiserSound*, int) override
    {
        targetHz = (float) juce::MidiMessage::getMidiNoteInHertz (midiNote);
        curHz = (shared.glideSec > 0.001f && shared.lastNoteHz > 0.0f) ? shared.lastNoteHz : targetHz;
        shared.lastNoteHz = targetHz;
        level = 0.3f + 0.7f * velocity;

        // nulstil lydmotorerne
        for (auto& row : ph) for (auto& p : row) p = rng.nextDouble();
        t = 0.0f;
        bpIc1 = bpIc2 = 0.0f;
        droneLp = 0.0f;
        lpIc1 = lpIc2 = 0.0f;
        smoothCutoff = shared.cutoffHz;
        grainClock = 1.0f; nextGrain = 0;
        for (auto& g : grains) g = {};
        arpClock = 0.0f; arpStep = 0;
        voiceW = shared.weights;

        // Karplus-Strong streng til PLUK
        ksLen = juce::jlimit (2, (int) ks.size() - 1, (int) std::round (sr / targetHz));
        for (int i = 0; i < ksLen; ++i) ks[(size_t) i] = rng.nextFloat() * 2.0f - 1.0f;
        ksPos = 0;

        adsr.noteOn();
    }

    void stopNote (float, bool allowTailOff) override
    {
        if (allowTailOff) adsr.noteOff();
        else { adsr.reset(); clearCurrentNote(); }
    }

    void pitchWheelMoved (int) override {}
    void controllerMoved (int, int) override {}

    void renderNextBlock (juce::AudioBuffer<float>& out, int start, int num) override
    {
        if (! isVoiceActive()) return;

        const float dtSec = 1.0f / sr;
        const float wCoef = 1.0f - std::exp (-1.0f / (0.03f * sr));
        const float glideCoef = shared.glideSec > 0.001f ? 1.0f - std::exp (-1.0f / (shared.glideSec * sr)) : 1.0f;
        const float cutCoef = 1.0f - std::exp (-1.0f / (0.01f * sr));
        const float k = 1.0f / (0.5f + shared.reso * 11.5f);

        for (int n = 0; n < num; ++n)
        {
            curHz += (targetHz - curHz) * glideCoef;

            float s = 0.0f;
            for (int e = 0; e < kNumSounds; ++e)
            {
                voiceW[(size_t) e] += (shared.weights[(size_t) e] - voiceW[(size_t) e]) * wCoef;
                const float w = voiceW[(size_t) e];
                if (w > 1.0e-4f)
                    s += w * engine (e, curHz);
            }

            // lavpas-filter (TPT state variable)
            smoothCutoff += (shared.cutoffHz - smoothCutoff) * cutCoef;
            const float fc = juce::jlimit (20.0f, sr * 0.45f, smoothCutoff);
            const float g = std::tan (juce::MathConstants<float>::pi * fc / sr);
            const float a1 = 1.0f / (1.0f + g * (g + k)), a2 = g * a1, a3 = g * a2;
            const float v3 = s - lpIc2;
            const float v1 = a1 * lpIc1 + a2 * v3;
            const float v2 = lpIc2 + a2 * lpIc1 + a3 * v3;
            lpIc1 = 2.0f * v1 - lpIc1;
            lpIc2 = 2.0f * v2 - lpIc2;

            const float y = v2 * adsr.getNextSample() * level * 0.25f;
            for (int c = 0; c < out.getNumChannels(); ++c)
                out.addSample (c, start + n, y);

            t += dtSec;

            if (! adsr.isActive())
            {
                clearCurrentNote();
                break;
            }
        }
    }

private:
    static float polyBlep (double p, double dt)
    {
        if (p < dt) { const double x = p / dt; return (float) (x + x - x * x - 1.0); }
        if (p > 1.0 - dt) { const double x = (p - 1.0) / dt; return (float) (x * x + x + x + 1.0); }
        return 0.0f;
    }

    static void adv (double& p, double dt) { p += dt; if (p >= 1.0) p -= std::floor (p); }

    float saw (double& p, float hz)
    {
        const double dt = juce::jlimit (0.0, 0.49, (double) hz / sr);
        const float v = (float) (2.0 * p - 1.0) - polyBlep (p, dt);
        adv (p, dt);
        return v;
    }

    float sine (double& p, float hz)
    {
        if (hz > sr * 0.45f) return 0.0f;
        const float v = std::sin (juce::MathConstants<float>::twoPi * (float) p);
        adv (p, hz / sr);
        return v;
    }

    float tri (double& p, float hz)
    {
        const float v = 4.0f * std::abs ((float) p - 0.5f) - 1.0f;
        adv (p, hz / sr);
        return v;
    }

    // De 14 lydmotorer. Hver giver ca. -1..1.
    float engine (int e, float f)
    {
        constexpr float twoPi = juce::MathConstants<float>::twoPi;
        auto& P = ph[(size_t) e];

        switch (e)
        {
            case 0: // VARM PAD: to svævende trekanter + sinus under
                return 0.4f * (tri (P[0], f * 0.997f) + tri (P[1], f * 1.003f)) + 0.3f * sine (P[2], f * 0.5f);

            case 1: // GLAS KLOKKE: uharmoniske deltoner der klinger ud
            {
                const float d = std::exp (-t / 1.6f);
                return 0.5f * sine (P[0], f * 2.0f) + 0.35f * d * sine (P[1], f * 5.52f) + 0.15f * d * sine (P[2], f * 10.8f);
            }

            case 2: // SAV BAS: to savtakker en oktav nede
                return 0.5f * (saw (P[0], f * 0.5f) + saw (P[1], f * 0.503f));

            case 3: // FM BJÆLDE: FM med faldende indeks
            {
                const float idx = 0.3f + 3.0f * std::exp (-t / 0.35f);
                const float m = std::sin (twoPi * (float) P[1]);
                adv (P[1], f * 3.5f / sr);
                const float v = std::sin (twoPi * (float) P[0] + idx * m);
                adv (P[0], f / sr);
                return v;
            }

            case 4: // VIND STØJ: støj gennem et smalt båndpas på tonen
            {
                const float x = rng.nextFloat() * 2.0f - 1.0f;
                const float fc = juce::jlimit (40.0f, sr * 0.4f, f * 2.0f);
                const float g = std::tan (juce::MathConstants<float>::pi * fc / sr);
                const float kk = 1.0f / 6.0f;
                const float a1 = 1.0f / (1.0f + g * (g + kk)), a2 = g * a1, a3 = g * a2;
                const float v3 = x - bpIc2;
                const float v1 = a1 * bpIc1 + a2 * v3;
                const float v2 = bpIc2 + a2 * bpIc1 + a3 * v3;
                bpIc1 = 2.0f * v1 - bpIc1;
                bpIc2 = 2.0f * v2 - bpIc2;
                return v1 * 2.5f * (0.7f + 0.3f * std::sin (twoPi * 0.3f * t));
            }

            case 5: // PLUK: Karplus-Strong streng
            {
                const float v = ks[(size_t) ksPos];
                const int nx = (ksPos + 1) % ksLen;
                ks[(size_t) ksPos] = 0.5f * (v + ks[(size_t) nx]) * 0.996f;
                ksPos = nx;
                return v * 1.4f;
            }

            case 6: // SUB BAS: ren sinus en oktav nede
                return sine (P[0], f * 0.5f);

            case 7: // KOR: tre svævende sinusser med vibrato
            {
                const float vib = 1.0f + 0.004f * std::sin (twoPi * 5.2f * t);
                const float a = sine (P[0], f * vib * 0.993f), b = sine (P[1], f * vib), c = sine (P[2], f * vib * 1.007f);
                return (a + b + c) * 0.33f + 0.3f * std::sin (2.0f * twoPi * (float) P[1]);
            }

            case 8: // ORGEL: trækstænger 1, 2, 3, 4, 8
            {
                const float p = twoPi * (float) P[0];
                float v = std::sin (p);
                if (f * 2.0f < sr * 0.45f) v += 0.8f * std::sin (2.0f * p);
                if (f * 3.0f < sr * 0.45f) v += 0.5f * std::sin (3.0f * p);
                if (f * 4.0f < sr * 0.45f) v += 0.6f * std::sin (4.0f * p);
                if (f * 8.0f < sr * 0.45f) v += 0.3f * std::sin (8.0f * p);
                adv (P[0], f / sr);
                return v * 0.4f;
            }

            case 9: // PWM LEAD: pulsbølge med bevægelig bredde
            {
                const float width = 0.5f + 0.35f * std::sin (twoPi * 0.7f * t);
                const double dt = juce::jlimit (0.0, 0.49, (double) f / sr);
                double p2 = P[0] + width; p2 -= std::floor (p2);
                const float s1 = (float) (2.0 * P[0] - 1.0) - polyBlep (P[0], dt);
                const float s2 = (float) (2.0 * p2 - 1.0) - polyBlep (p2, dt);
                adv (P[0], dt);
                return (s1 - s2) * 0.6f;
            }

            case 10: // GRANULÆR: korte korn med tilfældig oktav
            {
                grainClock += 1.0f / sr;
                if (grainClock > 0.035f)
                {
                    grainClock = 0.0f;
                    static const float ratios[] = { 0.5f, 1.0f, 1.0f, 2.0f, 1.5f };
                    auto& gr = grains[(size_t) nextGrain];
                    gr.ratio = ratios[rng.nextInt (5)];
                    gr.pos = 0.0f;
                    gr.ph = rng.nextDouble();
                    nextGrain = (nextGrain + 1) % 2;
                }
                float v = 0.0f;
                for (auto& gr : grains)
                {
                    if (gr.pos >= 1.0f) continue;
                    const float env = std::sin (juce::MathConstants<float>::pi * gr.pos);
                    v += env * env * sine (gr.ph, f * gr.ratio);
                    gr.pos += 1.0f / (0.07f * sr);
                }
                return v;
            }

            case 11: // CHIP ARP: firkant der hopper grundtone, kvint, oktav
            {
                arpClock += 1.0f / sr;
                if (arpClock > 0.125f) { arpClock = 0.0f; arpStep = (arpStep + 1) % 4; }
                static const float steps[] = { 1.0f, 1.5f, 2.0f, 1.5f };
                const float v = P[0] < 0.5 ? 0.6f : -0.6f;
                adv (P[0], f * steps[arpStep] / sr);
                return v;
            }

            case 12: // MØRK DRONE: dybe savtakker gennem et mørkt filter
            {
                const float x = saw (P[0], f * 0.4985f) + saw (P[1], f * 0.5015f) + 0.5f * saw (P[2], f * 0.25f);
                droneLp += (x - droneLp) * (1.0f - std::exp (-twoPi * 350.0f / sr));
                return droneLp * 0.7f;
            }

            case 13: // STRENGE: tre savtakker der svulmer op
            {
                const float swell = 1.0f - std::exp (-t / 0.35f);
                return swell * (saw (P[0], f * 0.994f) + saw (P[1], f) + saw (P[2], f * 1.006f)) * 0.33f;
            }

            default:
                return 0.0f;
        }
    }

    VoiceShared& shared;
    juce::ADSR adsr;
    juce::Random rng;
    float sr = 44100.0f;
    float targetHz = 440.0f, curHz = 440.0f, level = 1.0f, t = 0.0f;
    std::array<float, kNumSounds> voiceW {};
    std::array<std::array<double, 3>, kNumSounds> ph {};

    float lpIc1 = 0.0f, lpIc2 = 0.0f, smoothCutoff = 1700.0f;
    float bpIc1 = 0.0f, bpIc2 = 0.0f;
    float droneLp = 0.0f;
    std::vector<float> ks;
    int ksLen = 2, ksPos = 0;

    struct Grain { double ph = 0.0; float ratio = 1.0f; float pos = 1.0f; };
    std::array<Grain, 2> grains {};
    float grainClock = 1.0f;
    int nextGrain = 0;

    float arpClock = 0.0f;
    int arpStep = 0;
};
} // namespace blip
