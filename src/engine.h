// Reverie's sound: chords from the pads, voiced (chords.h), each note a voice of two morphing wavetable oscillators
// (waves.h) with unison, drift and air, a stereo filter and two envelopes, an LFO, and Motion: an X / Y path that
// moves four destinations, each note of a chord at its own place on it. The voices sum into the effects (fx.h)
// and a soft limiter.
//
// Timing: everything that changes over time (envelopes, glides, LFOs, Motion, the smoothed knobs, the effects'
// controls) moves on a grid of 16-sample control ticks counted from activation, and a note starts on its own sample;
// so the output is the same whatever the host's block size. Strum and Humanize delay notes by whole ticks.
#pragma once

#include "chords.h"
#include "fx.h"
#include "params.h"
#include "waves.h"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstring>
#if defined(__aarch64__)
#include <arm_neon.h>
#endif

namespace reverie
{
constexpr int maxVoices = 24;
constexpr int maxUnison = 4;
constexpr int maxStreams = 2 * maxUnison;
constexpr float outputGain = 2.15f; // a note's level before the chord's 1 / sqrt (notes): a full chord near -19 dBFS RMS

// What the page shows, written by the audio thread (relaxed atomics: a picture may mix two blocks).
struct Telemetry
{
    std::atomic<uint32_t> sounding { 0 };   // a bit a voice
    std::atomic<uint32_t> padsHeld { 0 };   // a bit a pad slot whose chord is held (or latched)
    std::atomic<int> lastSlot { -1 };       // the pad slot played last
    std::atomic<uint32_t> strikes { 0 };    // chords played (the page repaints when it moves)
    std::atomic<int> notes[maxVoices];
    std::atomic<float> levels[maxVoices], x[maxVoices], y[maxVoices];
    std::atomic<float> pathX { 0 }, pathY { 0 }; // where the path's own point is (index 0, no spread)
    std::atomic<float> morph[2];             // the newest voice's wave positions after modulation
    std::atomic<float> cutoff { 1000 };      // and its cutoff (Hz)
    std::atomic<float> ampEnv { 0 }, fltEnv { 0 };
    std::atomic<float> peak { 0 }, reverb { 0 };
    Telemetry()
    {
        for (int i = 0; i < maxVoices; ++i)
        {
            notes[i].store (-1, std::memory_order_relaxed);
            levels[i].store (0.0f, std::memory_order_relaxed);
            x[i].store (0.0f, std::memory_order_relaxed);
            y[i].store (0.0f, std::memory_order_relaxed);
        }
        morph[0].store (0.0f), morph[1].store (0.0f);
    }
};

struct Rng
{
    uint32_t s = 0x2545f491u;
    uint32_t next() { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return s; }
    float unit() { return (float) (next() >> 8) * (1.0f / 16777216.0f); }
    float bipolar() { return unit() * 2.0f - 1.0f; }
};

// The path a Motion shape draws, at phase t (cycles), in -1..1 each way.
inline void motionPoint (int shape, float t, float& px, float& py)
{
    constexpr float tau = 6.2831853f;
    t -= std::floor (t);
    const float a = tau * t;
    switch (shape)
    {
        case Motion::orbit: px = fast::cos (a), py = fast::sin (a); break;
        case Motion::figure8: px = fast::sin (a), py = 0.75f * fast::sin (2.0f * a); break;
        case Motion::pendulum: px = fast::sin (a), py = 1.25f * px * px - 0.65f; break;
        case Motion::spiral:
        {
            const float r = 0.15f + 0.85f * (1.0f - std::fabs (2.0f * t - 1.0f));
            px = r * fast::cos (4.0f * a), py = r * fast::sin (4.0f * a);
            break;
        }
        case Motion::wander:
            px = 0.55f * fast::sin (a) + 0.3f * fast::sin (3.0f * a + 1.3f) + 0.15f * fast::sin (7.0f * a + 2.1f);
            py = 0.5f * fast::sin (2.0f * a + 0.7f) + 0.35f * fast::sin (5.0f * a + 2.9f) + 0.15f * fast::sin (9.0f * a + 0.4f);
            break;
        case Motion::tide: px = 0.35f * fast::sin (3.0f * a), py = fast::sin (a); break;
        case Motion::star:
        {
            // a pentagram's five strokes, each eased
            const float s = t * 5.0f;
            const int k = (int) s;
            float f = s - (float) k;
            f = f * f * (3.0f - 2.0f * f);
            const float a0 = 1.5707963f + tau * 0.4f * (float) k, a1 = a0 + tau * 0.4f;
            px = fast::cos (a0) + (fast::cos (a1) - fast::cos (a0)) * f;
            py = fast::sin (a0) + (fast::sin (a1) - fast::sin (a0)) * f;
            break;
        }
        case Motion::lissajous: px = fast::sin (3.0f * a), py = fast::sin (2.0f * a); break;
        default: px = py = 0.0f; break;
    }
}

struct Envelope
{
    enum Stage : uint8_t { idle, attack, decay, release };
    Stage stage = idle;
    float level = 0.0f;
    struct Coefs { float attack, decay, sustain, release; };

    void trigger() { stage = attack; }
    void off() { if (stage != idle) stage = release; }
    // One control tick.
    float step (const Coefs& c)
    {
        switch (stage)
        {
            case attack:
                level += (1.25f - level) * c.attack;
                if (level >= 1.0f) level = 1.0f, stage = decay;
                break;
            case decay: level += (c.sustain - level) * c.decay; break;
            case release:
                level -= level * c.release;
                if (level < 1e-4f) level = 0.0f, stage = idle;
                break;
            default: break;
        }
        return level;
    }
};

// A stereo state-variable filter (Zavalishin's TPT form).
struct Svf
{
    float s1 = 0.0f, s2 = 0.0f;
    void clear() { s1 = s2 = 0.0f; }
};

class Engine
{
public:
    explicit Engine (Telemetry* t) : tel (t)
    {
        for (int i = 0; i < paramCount; ++i) target[i] = smooth[i] = paramTable[i].def;
        rng.s = 0x9e3779b9u;
    }

    void set (uint32_t index, float v)
    {
        if (index >= (uint32_t) paramCount) return;
        target[index] = v;
        if (paramTable[index].kind != Kind::continuous || ! running) smooth[index] = v;
        dirty = true;
    }

    void prepare (double sampleRate)
    {
        rate = (float) sampleRate;
        tickRate = rate / (float) tickSize;
        smoothCoef = 1.0f - std::exp (-1.0f / (0.02f * tickRate)); // about 20 ms
        (void) WaveBank::get();
        ensemble.prepare (rate);
        delay.prepare (rate);
        reverb.prepare (rate);
        reset();
    }

    void reset()
    {
        for (auto& v : voices) v = Voice {};
        for (int i = 0; i < paramCount; ++i) smooth[i] = target[i];
        ensemble.reset(), delay.reset(), reverb.reset();
        std::fill (std::begin (keyDown), std::end (keyDown), false);
        tickPos = 0;
        now = 0;
        motionPhase = 0.0;
        groups = 0;
        sustain = false;
        fxAwake = false;
        noise = 0x1234567u;
        airLp[0] = airLp[1] = airHp[0] = airHp[1] = 0.0f;
        silentTicks = 0;
        bend = 0.0f;
        wheel = 0.0f;
        voicedProgression = voicedVoicing = voicedKey = -1;
        dirty = true, smoothing = false;
        derive();
        running = true;
        tel->sounding.store (0, std::memory_order_relaxed);
        tel->padsHeld.store (0, std::memory_order_relaxed);
    }

    void deactivate() { running = false; }

    // The host's transport, at the start of a block.
    void transport (bool playing, double beats, double bpm)
    {
        tempo = bpm;
        if (playing) songBeats = beats;
        songPlaying = playing;
    }

    // --- Notes ------------------------------------------------------------------------------------------------------
    void noteOn (int key, float velocity)
    {
        if (key < 0 || key > 127) return;
        velocity = std::clamp (velocity, 0.0f, 1.0f);
        ensureVoicings();
        if (stale) derive(), stale = false; // asleep, the ticks only kept the knobs
        // Two strikes of one key on one sample: the louder plays once.
        if (lastStrikeKey == key && lastStrikeAt == now)
        {
            if (velocity > lastStrikeVelocity)
            {
                // as if struck once at the louder velocity (the same jitter, the same sums)
                for (auto& v : voices)
                    if (v.active && v.group == lastStrikeGroup)
                    {
                        v.velocity = std::clamp (velocity * v.jitter, 0.02f, 1.0f);
                        v.gain = velocityGain (v.velocity, target[ParamId::velocity]) * v.level;
                    }
                lastStrikeVelocity = velocity;
            }
            return;
        }
        const PadKey pad = padOf (key);
        const ChordNotes& chord = voiced.slot[pad.slot];
        if (chord.count == 0) return;
        const float* chordSet = target; // the chord section as set: a value sent with the note applies to it
        const bool latching = chordSet[ParamId::latch] >= 0.5f;
        const float glideTime = glideMs (chordSet[ParamId::glide]);

        // A key struck again while it holds a chord: that chord lets go first (a fresh strike, never a slide).
        releaseKey (key, true);
        keyDown[key] = true;

        int notes[maxChordNotes], count = 0;
        const int shift = 12 * (int) chordSet[ParamId::octave] + pad.shift;
        for (int i = 0; i < chord.count; ++i)
        {
            if (i == 0 && chord.hasBass && chordSet[ParamId::bass] <= 0.001f) continue; // no bass wanted: not played
            int n = chord.note[i] + shift;
            while (n > 115) n -= 12;
            while (n < 12) n += 12;
            notes[count++] = n;
        }
        if (count == 0) return;
        const bool bassFirst = chord.hasBass && chordSet[ParamId::bass] > 0.001f;
        const uint32_t group = ++groups;

        // Chord Glide: the chord held before (or latched) slides into this one, note by note, without striking again.
        int reused = 0;
        Voice* glideFrom[maxChordNotes] {};
        if (glideTime > 0.0f)
        {
            uint32_t held = 0;
            for (auto& v : voices)
                if (v.active && v.gated && v.waitTicks == 0 && v.group > held) held = v.group;
            if (held != 0)
            {
                int n = 0;
                for (auto& v : voices)
                    if (v.active && v.gated && v.group == held && n < maxChordNotes) glideFrom[n++] = &v;
                for (int i = 1; i < n; ++i) // by pitch (insertion: at most eight)
                    for (int j = i; j > 0 && glideFrom[j - 1]->target > glideFrom[j]->target; --j) std::swap (glideFrom[j - 1], glideFrom[j]);
                reused = std::min (n, count);
                for (int i = reused; i < n; ++i) glideFrom[i]->releaseNow();
            }
        }
        else if (latching)
            releaseLatched();

        const float strumTicks = strumMs (chordSet[ParamId::strum]) * 0.001f * tickRate;
        const float human = chordSet[ParamId::humanize];
        const float norm = 1.0f / std::sqrt ((float) std::max (count, 1));
        for (int i = 0; i < count; ++i)
        {
            const bool isBass = bassFirst && i == 0;
            const float jitter = 1.0f + human * 0.18f * rng.bipolar();
            const float vel = std::clamp (velocity * jitter, 0.02f, 1.0f);
            const float level = norm * outputGain * (isBass ? chordSet[ParamId::bass] : 1.0f);
            const float gain = velocityGain (vel, chordSet[ParamId::velocity]) * level;
            if (i < reused)
            {
                Voice& v = *glideFrom[i];
                v.key = key, v.group = group, v.index = i, v.size = count, v.isBass = isBass;
                v.target = (float) notes[i];
                v.velocity = vel, v.gain = gain, v.jitter = jitter, v.level = level;
                v.latched = latching, v.sustained = false;
                v.panAt = -9.0f; // its place in the chord moved
                continue;
            }
            Voice& v = allocate();
            int wait = (int) std::lround (strumTicks * (float) i);
            if (human > 0.0f) wait += (int) (human * human * 0.02f * tickRate * rng.unit()); // up to 20 ms late
            start (v, key, group, notes[i], vel, gain, i, count, isBass, wait, latching);
            v.jitter = jitter, v.level = level;
        }
        lastStrikeKey = key, lastStrikeAt = now, lastStrikeGroup = group, lastStrikeVelocity = velocity;
        tel->lastSlot.store (pad.slot, std::memory_order_relaxed);
        tel->strikes.fetch_add (1, std::memory_order_relaxed);
        fxAwake = true;
        silentTicks = 0;
    }

    void noteOff (int key)
    {
        if (key < 0)
        {
            for (int k = 0; k < 128; ++k) keyDown[k] = false;
            for (auto& v : voices)
                if (v.active && v.gated && ! v.latched)
                {
                    if (sustain) v.sustained = true;
                    else v.releaseNow();
                }
            return;
        }
        if (key > 127) return;
        keyDown[key] = false;
        releaseKey (key, false);
    }

    // A choke (or All Sound Off): the voices stop within a few milliseconds.
    void choke (int key)
    {
        for (auto& v : voices)
            if (v.active && (key < 0 || v.key == key)) v.chokeNow();
        if (key < 0) std::fill (std::begin (keyDown), std::end (keyDown), false);
        else if (key < 128) keyDown[key] = false;
    }

    // All Sound Off (CC 120): every voice choked and the effects' tails gone.
    void silence()
    {
        choke (-1);
        ensemble.reset(), delay.reset(), reverb.reset();
    }

    void allNotesOff() // CC 123 and the like: every chord lets go, latched ones too (notes yet to strum never start)
    {
        for (auto& v : voices)
            if (v.active && v.waitTicks > 0) v.active = false, v.gated = false, v.waitTicks = 0;
            else if (v.active && v.gated) v.releaseNow();
        std::fill (std::begin (keyDown), std::end (keyDown), false);
        sustain = false;
    }

    void controller (int cc, int value)
    {
        switch (cc)
        {
            case 1: wheel = (float) value / 127.0f, dirty = true; break;
            case 64:
                sustain = value >= 64;
                if (! sustain)
                    for (auto& v : voices)
                        if (v.active && v.sustained) v.releaseNow();
                break;
            case 120: if (value == 0) silence(); break;
            case 121: if (value == 0) { bend = 0.0f, wheel = 0.0f, dirty = true; } break;
            case 123: if (value == 0) allNotesOff(); break;
            default: break;
        }
    }
    void pitchBend (float amount) { bend = std::clamp (amount, -1.0f, 1.0f) * 2.0f; } // two semitones

    // --- Rendering -------------------------------------------------------------------------------------------------
    void render (float* left, float* right, uint32_t frames)
    {
        while (frames > 0)
        {
            if (tickPos == 0) tick();
            const int n = (int) std::min<uint32_t> (frames, (uint32_t) (tickSize - tickPos));
            std::fill (left, left + n, 0.0f);
            std::fill (right, right + n, 0.0f);
            float air[tickSize] {};
            bool breathing = false;
            for (auto& v : voices)
                if (v.active && v.waitTicks == 0)
                {
                    breathing |= v.airNow > 0.0f || v.airStep > 0.0f;
                    renderVoice (v, left, right, air, n);
                }
            // Air: one stereo noise in its band, under the sum of the voices' air envelopes (the same sound as a noise
            // a voice, for a twentieth of the work).
            if (breathing)
            {
                const float c = sh.airCoef, hc = sh.airHigh;
                uint32_t x = noise;
                for (int j = 0; j < n; ++j)
                {
                    x ^= x << 13; x ^= x >> 17; x ^= x << 5;
                    const float nl = (float) (int16_t) (x & 0xffff) * (1.0f / 32768.0f);
                    const float nr = (float) (int16_t) (x >> 16) * (1.0f / 32768.0f);
                    airLp[0] += (nl - airLp[0]) * c;
                    airLp[1] += (nr - airLp[1]) * c;
                    airHp[0] += (airLp[0] - airHp[0]) * hc;
                    airHp[1] += (airLp[1] - airHp[1]) * hc;
                    left[j] += (airLp[0] - airHp[0]) * air[j];
                    right[j] += (airLp[1] - airHp[1]) * air[j];
                }
                noise = x;
            }
            if (fxAwake)
            {
                ensemble.process (left, right, n);
                delay.process (left, right, n);
                reverb.process (left, right, n);
                for (int i = 0; i < n; ++i)
                {
                    left[i] = limit (left[i] * volumeNow);
                    right[i] = limit (right[i] * volumeNow);
                    volumeNow += volumeStep;
                    const float a = std::max (std::fabs (left[i]), std::fabs (right[i]));
                    tickPeak = std::max (tickPeak, a);
                }
            }
            else
                for (int i = 0; i < n; ++i) volumeNow += volumeStep;
            left += n, right += n, frames -= (uint32_t) n;
            tickPos = (tickPos + n) & (tickSize - 1);
            now += (uint64_t) n;
        }
        publish();
    }

    bool quiet() const { return ! fxAwake && activeCount == 0; }

    // The chords the pads play now (for the page and the tests).
    const VoicingSet& voicings() { ensureVoicings(); return voiced; }

private:
    struct Voice
    {
        bool active = false, gated = false, latched = false, sustained = false, isBass = false, choking = false;
        int key = -1;
        uint32_t group = 0;
        int index = 0, size = 1;
        int waitTicks = 0;
        float velocity = 1.0f, gain = 1.0f, jitter = 1.0f, level = 1.0f;
        float pitch = 60.0f, target = 60.0f;
        uint64_t born = 0;
        uint32_t phase[maxStreams] {}, inc[maxStreams] {};
        const float* seg[2] {}; // each oscillator's segment (waves.h): its unison share it
        uint32_t shift[2] {};
        float wobble[maxStreams] {}, panL[maxStreams] {}, panR[maxStreams] {}, oscGain[2] {};
        float panAt = -9.0f;
        float morphNow[2] {}, morphStep[2] {};
        int frame[2] { -1, -1 };
        Envelope amp, flt;
        float ampNow = 0.0f, ampStep = 0.0f, airNow = 0.0f, airStep = 0.0f;
        Svf fl[2], fr[2]; // the left and right states of each stage
        float g = 0.5f, k = 2.0f, a1 = 0.0f, a2 = 0.0f, a3 = 0.0f;
        int filterMode = 0;
        float lfoPhase = 0.0f, lfoRandom = 0.0f, lfoRandomTarget = 0.0f;
        uint32_t lfoTicks = 0, ticks = 0;
        float drift[2] {};
        double startBeat = 0.0;

        int releaseIn = -1; // a note let go before its strum turn: the ticks it plays from its turn, then lets go

        void releaseNow()
        {
            latched = false, sustained = false;
            if (waitTicks > 0)
            {
                if (releaseIn < 0) releaseIn = std::max (1, (int) (ticks)); // as long as the pad was held
                return;
            }
            gated = false, releaseIn = -1;
            amp.off(), flt.off();
        }
        void chokeNow()
        {
            if (waitTicks > 0) { active = false, gated = false, waitTicks = 0; return; } // never sounded
            releaseNow();
            choking = true;
        }
    };

    Telemetry* tel;
    Voice voices[maxVoices];
    Ensemble ensemble;
    PingPong delay;
    Reverb reverb;
    Rng rng;
    float target[paramCount] {}, smooth[paramCount] {};
    float rate = 48000.0f, tickRate = 3000.0f, smoothCoef = 0.016f;
    bool running = false, dirty = true, smoothing = false, stale = true;
    int tickPos = 0;
    uint64_t now = 0;
    double motionPhase = 0.0, tempo = 120.0, songBeats = 0.0;
    bool songPlaying = false;
    uint32_t groups = 0;
    bool keyDown[128] {};
    bool sustain = false;
    float bend = 0.0f, wheel = 0.0f;
    int lastStrikeKey = -1;
    uint64_t lastStrikeAt = ~0ull;
    uint32_t lastStrikeGroup = 0;
    float lastStrikeVelocity = 0.0f;
    VoicingSet voiced {};
    int voicedProgression = -1, voicedVoicing = -1, voicedKey = -1;
    bool fxAwake = false;
    uint32_t silentTicks = 0;
    float tickPeak = 0.0f;
    int activeCount = 0;
    float volumeNow = 0.0f, volumeStep = 0.0f;
    bool volumePrimed = false;
    const Voice* newestVoice = nullptr;
    uint32_t noise = 0x1234567u;
    float airLp[2] {}, airHp[2] {};

    // What every voice reads each tick (from the smoothed knobs).
    struct Shared
    {
        int wave[2] {}, unison[2] { 1, 1 };
        float pos[2] {}, offset[2] {}, detune[2] {}, level[2] {};
        float width = 0.7f, air = 0.0f, airCoef = 0.5f, airHigh = 0.1f, drift = 0.0f;
        int filter = 0;
        float cutoffOct = 10.0f, reso = 0.0f, envAmt = 0.0f, keyTrack = 0.0f, velCut = 0.0f;
        Envelope::Coefs amp {}, flt {};
        int lfoShape = 0;
        float lfoInc = 0.0f, lfoFadeTicks = 1.0f, lfoPitch = 0.0f, lfoCutoff = 0.0f, lfoWave = 0.0f;
        int motion = 0;
        bool retrig = false, sync = false;
        float depth = 0.0f, spreadPath = 0.0f, mx = 0.0f, my = 0.0f, motionRate = 0.0f;
        double divBeats = 16.0;
        int dest[4] {};
        float amt[4] {};
        float unisonGain[2] { 1.0f, 1.0f };
        float at[maxUnison][maxUnison] {}; // each unison voice's place, -1 .. 1, for 1 to 4 of them
        float volume = 0.7f, ensHz = 0.35f, decay = 5.0f, damp = 5500.0f, predelay = 20.0f;
    } sh;

    void ensureVoicings()
    {
        const int p = (int) target[ParamId::progression], v = (int) target[ParamId::voicing], k = (int) target[ParamId::key];
        if (p == voicedProgression && v == voicedVoicing && k == voicedKey) return;
        voiceProgression (p, v, k, voiced);
        voicedProgression = p, voicedVoicing = v, voicedKey = k;
    }

    void releaseKey (int key, bool restrike)
    {
        for (auto& v : voices)
            if (v.active && v.gated && v.key == key)
            {
                if (v.latched && ! restrike) continue;
                if (sustain && ! restrike) { v.sustained = true; continue; }
                v.releaseNow();
            }
    }

    void releaseLatched()
    {
        for (auto& v : voices)
            if (v.active && v.gated && v.latched) v.releaseNow();
    }

    Voice& allocate()
    {
        for (auto& v : voices)
            if (! v.active) return v;
        // The quietest voice letting go, else the oldest.
        Voice* best = nullptr;
        for (auto& v : voices)
            if (! v.gated && (best == nullptr || v.amp.level < best->amp.level)) best = &v;
        if (best != nullptr) return *best;
        for (auto& v : voices)
            if (best == nullptr || v.born < best->born) best = &v;
        return *best;
    }

    void start (Voice& v, int key, uint32_t group, int note, float vel, float gain, int index, int size, bool isBass, int wait, bool latching)
    {
        const bool reuse = v.active; // a stolen voice keeps its filter's state, so its tail does not click
        v.active = true, v.gated = true, v.latched = latching, v.sustained = false, v.choking = false;
        v.key = key, v.group = group, v.index = index, v.size = size, v.isBass = isBass;
        v.waitTicks = wait;
        v.releaseIn = -1;
        v.velocity = vel, v.gain = gain;
        v.pitch = v.target = (float) note;
        v.born = now;
        v.amp = Envelope {}, v.flt = Envelope {};
        if (! reuse)
        {
            v.ampNow = 0.0f, v.airNow = 0.0f;
            for (auto& f : v.fl) f.clear();
            for (auto& f : v.fr) f.clear();
        }
        v.ampStep = 0.0f;
        v.amp.level = 0.0f;
        v.frame[0] = v.frame[1] = -1;
        v.panAt = -9.0f;
        v.lfoPhase = rng.unit();
        v.lfoRandom = v.lfoRandomTarget = rng.bipolar();
        v.lfoTicks = 0, v.ticks = 0;
        v.drift[0] = rng.bipolar() * 0.5f, v.drift[1] = rng.bipolar() * 0.5f;
        v.startBeat = songBeats;
        for (int s = 0; s < maxStreams; ++s)
        {
            const uint32_t r = rng.next();
            if (! reuse) v.phase[s] = r; // a stolen voice runs on from its phase: no step in its wave
            v.wobble[s] = rng.bipolar();
        }
        if (wait == 0)
        {
            begin (v);
            if (tickPos != 0) control (v, true); // on a tick's first sample the tick works it out
        }
    }

    // The voice sounds from now: its envelopes start.
    static void begin (Voice& v)
    {
        v.amp.trigger();
        v.flt.trigger();
    }

    // --- The control tick ------------------------------------------------------------------------------------------
    void tick()
    {
        // Asleep (nothing sounds, the tails are gone): the knobs go straight to their values and the clocks run on.
        if (! fxAwake && activeCount == 0)
        {
            if (dirty || smoothing)
            {
                std::memcpy (smooth, target, sizeof (smooth));
                dirty = smoothing = false;
                stale = true;
            }
            advanceClocks();
            return;
        }
        // The knobs, smoothed (about 20 ms), and what they mean worked out again while any moves.
        if (dirty || smoothing)
        {
            bool moving = false;
            for (int i = 0; i < paramCount; ++i)
            {
                if (smooth[i] == target[i]) continue;
                if (paramTable[i].kind != Kind::continuous) { smooth[i] = target[i]; continue; }
                smooth[i] += (target[i] - smooth[i]) * smoothCoef;
                if (std::fabs (target[i] - smooth[i]) <= 1e-6f * (1.0f + std::fabs (target[i]))) smooth[i] = target[i];
                else moving = true;
            }
            dirty = false, smoothing = moving;
            ensureVoicings();
            derive();
            stale = false;
        }
        else if (stale)
            derive(), stale = false;
        advanceClocks();
        {
            float px = 0.0f, py = 0.0f; // the path's own point, for the page
            motionPoint (sh.motion, (float) (motionPhase - std::floor (motionPhase)), px, py);
            tel->pathX.store (std::clamp (sh.mx + sh.depth * px, -1.0f, 1.0f), std::memory_order_relaxed);
            tel->pathY.store (std::clamp (sh.my + sh.depth * py, -1.0f, 1.0f), std::memory_order_relaxed);
        }

        // Latch turned off: latched chords whose pads are up let go.
        if (target[ParamId::latch] < 0.5f)
            for (auto& v : voices)
                if (v.active && v.latched)
                {
                    v.latched = false;
                    if (! (v.key >= 0 && v.key < 128 && keyDown[v.key])) v.releaseNow();
                }
        newestVoice = nullptr;
        for (const auto& v : voices)
            if (v.active && (v.waitTicks == 0 || v.waitTicks == 1)
                && (newestVoice == nullptr || v.born > newestVoice->born || (v.born == newestVoice->born && v.index < newestVoice->index)))
                newestVoice = &v;
        int active = 0;
        for (auto& v : voices)
        {
            if (! v.active) continue;
            if (v.waitTicks > 0)
            {
                ++active;
                ++v.ticks;
                if (--v.waitTicks != 0) continue;
                v.ticks = 0;
                begin (v);
                control (v, true);
                continue;
            }
            if (v.releaseIn > 0 && --v.releaseIn == 0) v.releaseNow();
            control (v, false);
            if (v.active) ++active;
        }
        activeCount = active;

        // Effects
        const float vol = sh.volume;
        if (! volumePrimed) volumeNow = vol, volumePrimed = true;
        volumeStep = (vol - volumeNow) / (float) tickSize;
        ensemble.control (smooth[ParamId::ensMix], smooth[ParamId::ensDepth], sh.ensHz);
        const double beat = 60.0 / std::clamp (tempo, 40.0, 300.0);
        delay.control (smooth[ParamId::dlyMix], (float) (delayBeats[(int) smooth[ParamId::dlyTime]] * beat), smooth[ParamId::dlyFeedback]);
        reverb.control (smooth[ParamId::revMix], smooth[ParamId::revSize], sh.decay, sh.damp, sh.predelay, smooth[ParamId::shimmer]);
        // Asleep once nothing sounds and the tails have been quiet for four seconds (the longest delay fits in it).
        if (fxAwake)
        {
            if (active == 0 && tickPeak < 2e-6f)
            {
                if (++silentTicks > (uint32_t) (4.0f * tickRate))
                {
                    fxAwake = false;
                    ensemble.reset(), delay.reset(), reverb.reset();
                }
            }
            else
                silentTicks = 0;
        }
        tel->reverb.store (tickPeak, std::memory_order_relaxed);
        tickPeak = 0.0f;
    }

    // Motion's own clock (free, or the song's beats when synced and playing) and the song's.
    void advanceClocks()
    {
        const bool sync = target[ParamId::motionSync] >= 0.5f;
        const double div = motionDivBeats[std::clamp ((int) target[ParamId::motionDiv], 0, 6)];
        if (sync)
        {
            if (songPlaying) motionPhase = songBeats / div;
            else motionPhase += tempo / 60.0 / (double) tickRate / div;
        }
        else
            motionPhase += (double) (motionHz (smooth[ParamId::motionRate]) / tickRate);
        if (motionPhase > 1e6) motionPhase -= std::floor (motionPhase);
        if (songPlaying) songBeats += tempo / 60.0 * (double) tickSize / (double) rate;
    }

    void derive()
    {
        using namespace ParamId;
        const float* p = smooth;
        for (int o = 0; o < 2; ++o)
        {
            const uint32_t b = ParamId::osc (o, 0);
            sh.wave[o] = std::clamp ((int) p[b], 0, waveCount - 1);
            sh.pos[o] = p[b + 1];
            sh.offset[o] = 12.0f * p[b + 2] + p[b + 3] + p[b + 4] * 0.01f;
            sh.unison[o] = std::clamp ((int) p[b + 5], 1, maxUnison);
            sh.detune[o] = detuneCents (p[b + 6]) * 0.01f;
            sh.level[o] = p[b + 7];
        }
        sh.width = p[width];
        sh.air = p[air] * p[air] * 0.35f;
        sh.airCoef = onePoleCoef (900.0f * std::pow (18.0f, p[airTone]), rate);
        sh.airHigh = onePoleCoef (350.0f, rate);
        sh.drift = driftCents (p[drift]) * 0.01f;
        sh.filter = (int) p[filterType];
        sh.cutoffOct = std::log2 (cutoffHz (p[cutoff]));
        sh.reso = p[resonance];
        sh.envAmt = p[filterEnv] * 6.0f;
        sh.keyTrack = p[keyTrack];
        sh.velCut = p[velCutoff];
        auto coefs = [&] (float a, float d, float s, float r) {
            const float tick = 1000.0f / tickRate; // ms
            auto rc = [&] (float ms, float shape) { return ms <= tick * 0.5f ? 1.0f : 1.0f - std::exp (-tick * shape / ms); };
            return Envelope::Coefs { rc (envMs (a), 1.609f), rc (envMs (d), 4.6f), s, rc (envMs (r), 6.9f) };
        };
        sh.amp = coefs (p[ampAttack], p[ampDecay], p[ampSustain], p[ampRelease]);
        sh.flt = coefs (p[fltAttack], p[fltDecay], p[fltSustain], p[fltRelease]);
        sh.lfoShape = (int) p[lfoShape];
        sh.lfoInc = lfoHz (p[lfoRate]) / tickRate;
        sh.lfoFadeTicks = std::max (1.0f, lfoDelayMs (p[lfoDelay]) * 0.001f * tickRate);
        sh.lfoPitch = p[lfoPitch] * 0.5f; // up to 50 cents
        sh.lfoCutoff = p[lfoCutoff] * 2.0f;
        sh.lfoWave = p[lfoWave] * 0.5f;
        sh.motion = (int) p[motionShape];
        sh.motionRate = motionHz (p[motionRate]);
        sh.sync = p[motionSync] >= 0.5f;
        sh.divBeats = motionDivBeats[std::clamp ((int) p[motionDiv], 0, 6)];
        sh.depth = sh.motion == Motion::off ? 0.0f : p[motionDepth];
        sh.spreadPath = p[motionSpread];
        sh.retrig = p[motionRetrig] >= 0.5f;
        sh.mx = p[motionX];
        sh.my = std::clamp (p[motionY] + wheel, -1.0f, 1.0f);
        const uint32_t dests[4] = { xDest1, xDest2, yDest1, yDest2 };
        for (int i = 0; i < 4; ++i) sh.dest[i] = (int) p[dests[i]], sh.amt[i] = p[dests[i] + 1];
        for (int o = 0; o < 2; ++o) sh.unisonGain[o] = 1.0f / std::sqrt ((float) sh.unison[o]);
        for (int n = 1; n <= maxUnison; ++n)
            for (int s = 0; s < n; ++s) sh.at[n - 1][s] = n == 1 ? 0.0f : -1.0f + 2.0f * (float) s / (float) (n - 1);
        sh.volume = p[ParamId::volume];
        sh.ensHz = ensHz (p[ParamId::ensRate]);
        sh.decay = decaySeconds (p[ParamId::revDecay]);
        sh.damp = dampHz (p[ParamId::revDamp]);
        sh.predelay = predelayMs (p[ParamId::revPredelay]);
    }

    // A voice's controls for the coming tick (or, `starting`, the rest of this one).
    void control (Voice& v, bool starting)
    {
        const int left = starting ? tickSize - tickPos : tickSize; // samples until the next tick
        const float amp = v.amp.step (sh.amp);
        const float flt = v.flt.step (sh.flt);
        if (v.choking) v.amp.level *= 0.6f; // gone within a few milliseconds
        ++v.ticks;

        // Glide (Chord Glide: about 95 % of the way in the glide time)
        if (v.pitch != v.target)
        {
            const float gms = glideMs (target[ParamId::glide]);
            const float coef = gms <= 0.0f ? 1.0f : 1.0f - std::exp (-3.0f * 1000.0f / (gms * tickRate));
            v.pitch += (v.target - v.pitch) * coef;
            if (std::fabs (v.target - v.pitch) < 1e-4f) v.pitch = v.target;
        }

        // LFO (its own phase a voice, faded in after the note starts)
        v.lfoPhase += sh.lfoInc;
        if (v.lfoPhase >= 1.0f)
        {
            v.lfoPhase -= std::floor (v.lfoPhase);
            v.lfoRandomTarget = rng.bipolar();
        }
        float lfo;
        switch (sh.lfoShape)
        {
            case LfoShape::triangle: lfo = 1.0f - 4.0f * std::fabs (v.lfoPhase - 0.5f); break;
            case LfoShape::drift:
                v.lfoRandom += (v.lfoRandomTarget - v.lfoRandom) * std::min (1.0f, sh.lfoInc * 4.0f);
                lfo = v.lfoRandom;
                break;
            default: lfo = fast::sin (6.2831853f * v.lfoPhase); break;
        }
        ++v.lfoTicks;
        lfo *= std::min (1.0f, (float) v.lfoTicks / sh.lfoFadeTicks);

        // Drift: each oscillator's own slow wander (an Ornstein-Uhlenbeck walk, about half a hertz)
        for (int o = 0; o < 2; ++o) v.drift[o] += 0.0775f * rng.bipolar() - 0.001f * v.drift[o];

        // Motion
        float mx = sh.mx, my = sh.my;
        if (sh.depth > 0.0f)
        {
            double t;
            if (sh.retrig)
                t = sh.sync ? (songBeats - v.startBeat) / sh.divBeats : (double) v.ticks * (double) sh.motionRate / (double) tickRate;
            else
                t = motionPhase;
            const float place = v.size > 1 ? sh.spreadPath * (float) v.index / (float) v.size : 0.0f;
            float px, py;
            motionPoint (sh.motion, (float) (t - std::floor (t)) + place, px, py);
            mx = std::clamp (mx + sh.depth * px, -1.0f, 1.0f);
            my = std::clamp (my + sh.depth * py, -1.0f, 1.0f);
        }
        const int slot = (int) (&v - voices);
        tel->x[slot].store (mx, std::memory_order_relaxed);
        tel->y[slot].store (my, std::memory_order_relaxed);
        float mod[Dest::count] {};
        for (int i = 0; i < 4; ++i) mod[sh.dest[i]] += sh.amt[i] * (i < 2 ? mx : my);
        const float lfoDepth = std::clamp (1.0f + mod[Dest::lfoDepth], 0.0f, 2.0f);
        lfo *= lfoDepth;

        // Oscillators
        const float detuneScale = std::clamp (1.0f + mod[Dest::detune], 0.0f, 3.0f) * (v.isBass ? 0.4f : 1.0f);
        const float mix = std::clamp (mod[Dest::oscMix], -1.0f, 1.0f);
        const float oscGain[2] = { sh.level[0] * (1.0f - std::max (0.0f, mix)), sh.level[1] * (1.0f + std::min (0.0f, mix)) };
        const float pitchMod = v.pitch + bend + lfo * sh.lfoPitch + mod[Dest::pitch] * 2.0f;
        float width = std::clamp (sh.width + mod[Dest::width], 0.0f, 1.0f) * (v.isBass ? 0.2f : 1.0f);
        if (std::fabs (width - v.panAt) > 1e-4f)
        {
            v.panAt = width;
            // the voice's own place (upper notes alternate sides), then its unison across the width
            const float home = v.isBass ? 0.0f : 0.3f * width * ((v.index & 1) ? 1.0f : -1.0f);
            for (int o = 0; o < 2; ++o)
                for (int s = 0; s < maxUnison; ++s)
                {
                    const float at = sh.at[sh.unison[o] - 1][std::min (s, sh.unison[o] - 1)];
                    const float pan = std::clamp (home + width * at * (o == 0 ? 0.8f : -0.8f), -1.0f, 1.0f);
                    const float angle = (pan + 1.0f) * 0.78539816f;
                    v.panL[o * maxUnison + s] = fast::cos (angle);
                    v.panR[o * maxUnison + s] = fast::sin (angle);
                }
        }
        for (int o = 0; o < 2; ++o)
        {
            const int n = sh.unison[o];
            float pos = sh.pos[o] + lfo * sh.lfoWave + mod[Dest::waves] + mod[o == 0 ? Dest::wave1 : Dest::wave2];
            pos = std::clamp (pos, 0.0f, 1.0f);
            const float x = pos * (float) (waveFrames - 1);
            const int frame = std::min ((int) x, waveFrames - 2);
            const float frac = x - (float) frame;
            if (frame != v.frame[o] || starting)
            {
                v.frame[o] = frame;
                v.morphNow[o] = frac, v.morphStep[o] = 0.0f;
            }
            else
                v.morphStep[o] = (frac - v.morphNow[o]) / (float) left;
            const float noteHz = 440.0f * fast::exp2 ((pitchMod + sh.offset[o] + v.drift[o] * sh.drift - 69.0f) * (1.0f / 12.0f));
            const float base = noteHz / rate;
            const float spread = sh.detune[o] * detuneScale;
            float top = base;
            for (int s = 0; s < n; ++s)
            {
                const float at = sh.at[n - 1][s];
                const float semis = spread * (at + 0.08f * v.wobble[o * maxUnison + s]);
                const float cycles = std::min (base * fast::exp2Small (semis * (1.0f / 12.0f)), 0.49f);
                v.inc[o * maxUnison + s] = (uint32_t) (cycles * 4294967296.0f);
                top = std::max (top, cycles);
            }
            // one band limit for the unison: the highest of them
            const int level = WaveBank::levelFor (top);
            v.seg[o] = WaveBank::get().segment (sh.wave[o], frame, level);
            v.shift[o] = WaveBank::get().table (sh.wave[o], frame, level).shift;
            v.oscGain[o] = oscGain[o] * sh.unisonGain[o];
        }
        if (&v == newestVoice)
        {
            tel->morph[0].store (std::clamp (sh.pos[0] + lfo * sh.lfoWave + mod[Dest::waves] + mod[Dest::wave1], 0.0f, 1.0f), std::memory_order_relaxed);
            tel->morph[1].store (std::clamp (sh.pos[1] + lfo * sh.lfoWave + mod[Dest::waves] + mod[Dest::wave2], 0.0f, 1.0f), std::memory_order_relaxed);
        }

        // Filter
        float oct = sh.cutoffOct + sh.envAmt * flt + sh.keyTrack * (v.pitch - 60.0f) / 12.0f + sh.velCut * (v.velocity - 1.0f) * 3.0f
                    + lfo * sh.lfoCutoff + mod[Dest::cutoff] * 4.0f;
        const float hz = std::clamp (fast::exp2 (oct), 16.0f, std::min (20000.0f, 0.45f * rate));
        const float reso = std::clamp (sh.reso + mod[Dest::resonance], 0.0f, 1.0f);
        v.filterMode = sh.filter;
        v.g = fast::tan (3.14159265f * hz / rate);
        v.k = 2.0f - 1.94f * reso;
        v.a1 = 1.0f / (1.0f + v.g * (v.g + v.k));
        v.a2 = v.g * v.a1;
        v.a3 = v.g * v.a2;
        if (&v == newestVoice)
        {
            tel->cutoff.store (hz, std::memory_order_relaxed);
            tel->ampEnv.store (amp, std::memory_order_relaxed);
            tel->fltEnv.store (flt, std::memory_order_relaxed);
        }

        // Amplitude and air
        const float level = std::clamp (1.0f + mod[Dest::level] * 0.5f, 0.0f, 1.5f);
        const float goal = amp * v.gain * level;
        if (starting) v.ampStep = (goal - v.ampNow) / (float) left;
        else v.ampStep = (goal - v.ampNow) / (float) tickSize;
        const float airGoal = std::clamp (sh.air + mod[Dest::air] * 0.15f, 0.0f, 0.5f) * goal;
        v.airStep = (airGoal - v.airNow) / (float) left;

        if (v.amp.stage == Envelope::idle && v.ampNow <= 1e-6f && ! v.gated)
        {
            v.active = false;
            v.ampNow = 0.0f;
        }
    }

    // --- A voice's samples -----------------------------------------------------------------------------------------
    // The oscillators into a stereo buffer (pairs), the filter over it, then the amplitude into the output; the voice's
    // air envelope into `air` (one noise for every voice: Engine::render).
    void renderVoice (Voice& v, float* outL, float* outR, float* air, int n)
    {
        alignas (16) float lr[2 * tickSize] {};
        for (int o = 0; o < 2; ++o)
        {
            const int u = sh.unison[o];
            float morph[tickSize];
            {
                float m = v.morphNow[o];
                for (int j = 0; j < n; ++j) morph[j] = m, m += v.morphStep[o];
                v.morphNow[o] = m;
            }
            if (v.oscGain[o] <= 0.0f) continue;
            const float* seg = v.seg[o];
            const uint32_t shift = v.shift[o], fracShift = 32u - shift;
            for (int s = 0; s < u; ++s)
            {
                const int i = o * maxUnison + s;
                const uint32_t inc = v.inc[i];
                const float gl = v.panL[i] * v.oscGain[o], gr = v.panR[i] * v.oscGain[o];
                uint32_t ph = v.phase[i];
#if defined(__aarch64__)
                const float32x2_t pan = { gl, gr };
                for (int j = 0; j < n; ++j)
                {
                    const uint32_t idx = ph >> shift;
                    const float fr = (float) (ph << fracShift) * 2.3283064e-10f;
                    const float32x4_t q = vld1q_f32 (seg + 2 * idx); // value, difference at idx and idx + 1
                    const float32x2_t lo = vget_low_f32 (q);
                    const float32x2_t vd = vfma_n_f32 (lo, vsub_f32 (vget_high_f32 (q), lo), fr);
                    const float x = vget_lane_f32 (vd, 0) + morph[j] * vget_lane_f32 (vd, 1);
                    vst1_f32 (lr + 2 * j, vfma_n_f32 (vld1_f32 (lr + 2 * j), pan, x));
                    ph += inc;
                }
#else
                for (int j = 0; j < n; ++j)
                {
                    const uint32_t idx = ph >> shift;
                    const float fr = (float) (ph << fracShift) * 2.3283064e-10f;
                    const float* q = seg + 2 * idx;
                    const float value = q[0] + (q[2] - q[0]) * fr, diff = q[1] + (q[3] - q[1]) * fr;
                    const float x = value + morph[j] * diff;
                    lr[2 * j] += x * gl;
                    lr[2 * j + 1] += x * gr;
                    ph += inc;
                }
#endif
                v.phase[i] = ph;
            }
        }
        // the filter (two in series for LP 24: the first without resonance)
        if (v.filterMode == Filter::lp24)
        {
            filter (v, 0, lr, n, Filter::lp12, true);
            filter (v, 1, lr, n, Filter::lp12, false);
        }
        else
            filter (v, 0, lr, n, v.filterMode, false);
        float a = v.ampNow;
        const float da = v.ampStep;
        for (int j = 0; j < n; ++j)
        {
            outL[j] += lr[2 * j] * a;
            outR[j] += lr[2 * j + 1] * a;
            a += da;
        }
        v.ampNow = std::max (0.0f, a);
        if (v.airNow > 0.0f || v.airStep > 0.0f)
        {
            float w = v.airNow;
            for (int j = 0; j < n; ++j) air[j] += w, w += v.airStep;
            v.airNow = std::max (0.0f, w);
        }
    }

    // The TPT state-variable filter over a stereo buffer (pairs): mode LP 12, BP or HP; `firstOfTwo` makes LP 24's
    // first stage (Butterworth's damping, no resonance).
    void filter (Voice& v, int stage, float* lr, int n, int mode, bool firstOfTwo)
    {
        float a1 = v.a1, a2 = v.a2, a3 = v.a3, k = v.k;
        if (firstOfTwo)
        {
            k = 1.4142136f;
            a1 = 1.0f / (1.0f + v.g * (v.g + k));
            a2 = v.g * a1;
            a3 = v.g * a2;
        }
        Svf& L = v.fl[stage];
        Svf& R = v.fr[stage];
#if defined(__aarch64__)
        float32x2_t s1 = { L.s1, R.s1 }, s2 = { L.s2, R.s2 };
        for (int j = 0; j < n; ++j)
        {
            const float32x2_t x = vld1_f32 (lr + 2 * j);
            const float32x2_t v3 = vsub_f32 (x, s2);
            const float32x2_t v1 = vfma_n_f32 (vmul_n_f32 (s1, a1), v3, a2);
            const float32x2_t v2 = vfma_n_f32 (vfma_n_f32 (s2, s1, a2), v3, a3);
            s1 = vsub_f32 (vadd_f32 (v1, v1), s1);
            s2 = vsub_f32 (vadd_f32 (v2, v2), s2);
            float32x2_t y;
            if (mode == Filter::bp) y = vmul_n_f32 (v1, k);
            else if (mode == Filter::hp) y = vsub_f32 (vsub_f32 (x, vmul_n_f32 (v1, k)), v2);
            else y = v2;
            vst1_f32 (lr + 2 * j, y);
        }
        L.s1 = vget_lane_f32 (s1, 0), R.s1 = vget_lane_f32 (s1, 1);
        L.s2 = vget_lane_f32 (s2, 0), R.s2 = vget_lane_f32 (s2, 1);
#else
        for (int c = 0; c < 2; ++c)
        {
            Svf& f = c == 0 ? L : R;
            float s1 = f.s1, s2 = f.s2;
            for (int j = 0; j < n; ++j)
            {
                const float x = lr[2 * j + c];
                const float v3 = x - s2;
                const float v1 = a1 * s1 + a2 * v3;
                const float v2 = s2 + a2 * s1 + a3 * v3;
                s1 = 2.0f * v1 - s1;
                s2 = 2.0f * v2 - s2;
                lr[2 * j + c] = mode == Filter::bp ? v1 * k : mode == Filter::hp ? x - k * v1 - v2 : v2;
            }
            f.s1 = s1, f.s2 = s2;
        }
#endif
    }

    static float limit (float x)
    {
        constexpr float t = 0.8f;
        const float a = std::fabs (x);
        if (a <= t) return x;
        const float y = t + (1.0f - t) * fast::tanh ((a - t) / (1.0f - t));
        return x < 0.0f ? -y : y;
    }

    void publish()
    {
        uint32_t sounding = 0, held = 0;
        for (int i = 0; i < maxVoices; ++i)
        {
            const Voice& v = voices[i];
            const bool on = v.active && v.waitTicks == 0;
            if (on) sounding |= 1u << i;
            if (v.active && v.gated)
            {
                const PadKey pad = padOf (v.key);
                held |= 1u << pad.slot;
            }
            tel->notes[i].store (on ? (int) std::lround (v.pitch) : -1, std::memory_order_relaxed);
            tel->levels[i].store (on ? v.ampNow : 0.0f, std::memory_order_relaxed);
        }
        tel->sounding.store (sounding, std::memory_order_relaxed);
        tel->padsHeld.store (held, std::memory_order_relaxed);
    }
};
} // namespace reverie
