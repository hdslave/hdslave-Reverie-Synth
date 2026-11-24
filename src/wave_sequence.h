// A single additional band-limited layer per note. The two main oscillators keep their own waves and unison.
#pragma once

#include "fx.h"
#include "waves.h"

namespace reverie
{
struct WaveSequenceSettings
{
    int mode = Sequence::off;
    int waves[4] { 5, 2, 9, 7 };
    float level = 0.6f, fade = 0.7f, octave = 0.0f;
    double beats = 1.0;
};

class WaveSequenceVoice
{
public:
    void start (uint32_t initialPhase)
    {
        *this = {};
        phase = initialPhase;
    }

    // Advance from the note's actual start, including a partial first control tick. A tempo
    // change alters the remaining duration without moving the note to a different step.
    void control (const WaveSequenceSettings& s, float pitch, float rate, double stepsPerSample,
                  int frames, float pan)
    {
        if (mode != s.mode)
        {
            if (gain > 0.00001f)
            {
                gainStep = -gain / (float) frames;
                rendering = true;
                return;
            }
            mode = s.mode;
            position = 0.0;
            lastStep = -1;
            gain = 0.0f;
        }
        if (mode == Sequence::off || (mode == Sequence::strike && position >= 4.0))
        {
            gain = gainStep = 0.0f;
            rendering = false;
            return;
        }

        rendering = gain > 0.0f || s.level > 0.0f;
        const int step = std::min ((int) position, 3);
        if (step != lastStep)
        {
            previousWave = lastStep < 0 ? s.waves[step] : currentWave;
            currentWave = s.waves[step];
            mix = previousWave == currentWave ? 1.0f : 0.0f;
            lastStep = step;
        }
        const double next = position + frames * stepsPerSample;
        // Reserve at least 4 ms to join unrelated waves, even at the shortest fade setting.
        const double overlap = std::max ((double) s.fade, stepsPerSample * rate * 0.004);
        const float t = (float) std::clamp ((next - step) / overlap, 0.0, 1.0);
        const float targetMix = t * t * (3.0f - 2.0f * t);
        mixStep = previousWave == currentWave ? 0.0f : (targetMix - mix) / (float) frames;
        const float contour = mode == Sequence::strike ? std::max (0.0f, fast::exp2 (-2.5f * (float) next) - 0.001f) : 1.0f;
        gainStep = (s.level * contour - gain) / (float) frames;
        panLeft = 0.7071f * (1.0f - pan);
        panRight = 0.7071f * (1.0f + pan);

        const float cycles = std::min (0.46f, 440.0f / rate * fast::exp2 ((pitch + s.octave * 12.0f - 69.0f) / 12.0f));
        increment = (uint32_t) (cycles * 4294967296.0f);
        const int level = WaveBank::levelFor (cycles);
        const auto& bank = WaveBank::get();
        // Each wave's second stored frame has an independent, fixed spectrum.
        from = bank.segment (previousWave, 1, level);
        to = bank.segment (currentWave, 1, level);
        shift = bank.table (currentWave, 1, level).shift;
        position = mode == Sequence::loop && next >= 4.0 ? next - 4.0 : std::min (next, 4.0);
    }

    bool audible() const { return rendering; }
    bool enabled() const { return mode != Sequence::off; }

    void render (float* stereo, int n)
    {
        const bool crossfade = from != to;
        for (int i = 0; i < n; ++i)
        {
            const uint32_t index = (phase >> shift) * 2u;
            const float fraction = (float) (phase << (32u - shift)) * 2.3283064e-10f;
            float value = to[index] + fraction * (to[index + 2] - to[index]);
            if (crossfade && mix < 1.0f)
            {
                const float old = from[index] + fraction * (from[index + 2] - from[index]);
                value = old + std::clamp (mix, 0.0f, 1.0f) * (value - old);
            }
            // In Strike the short layer speaks first while the sustained oscillators rise behind it.
            const float body = mode == Sequence::strike ? 1.0f - 0.72f * gain : 1.0f;
            stereo[2 * i] = stereo[2 * i] * body + value * gain * panLeft;
            stereo[2 * i + 1] = stereo[2 * i + 1] * body + value * gain * panRight;
            phase += increment;
            mix = std::clamp (mix + mixStep, 0.0f, 1.0f);
            gain = std::max (0.0f, gain + gainStep);
        }
    }

private:
    int mode = Sequence::off, lastStep = -1, previousWave = 0, currentWave = 0;
    double position = 0.0;
    uint32_t phase = 0, increment = 0, shift = 0;
    const float* from = nullptr;
    const float* to = nullptr;
    float mix = 1.0f, mixStep = 0.0f, gain = 0.0f, gainStep = 0.0f;
    float panLeft = 0.7071f, panRight = 0.7071f;
    bool rendering = false;
};
} // namespace reverie
