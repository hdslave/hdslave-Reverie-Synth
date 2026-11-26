// Loads Reverie.clap as a host does and checks it: description and ports, parameters and their texts, state, the
// sounds (and that they leave the chord section alone), the progressions (every one parses, every voicing in every
// key is a sound chord, the table the plug-in reads is the search's), the pad map Melodee reads, notes and chords (the
// pads, the banks, Melodee's same-sample and on-on-off strikes, latch, sustain, chokes, note ids, the channel modes,
// stolen voices), Chord Glide, the effects cleared, the same output whatever the block size, other rates and tuning,
// bounded output at extreme settings, sleeping when silent, and the screen page.
// Writes demo renders (WAV) and the page's pictures (PNG) into the output folder.
//   host Reverie.clap OUT_FOLDER
#include "wav.h"
#include "png.h"
#include "../src/engine.h"
#include "mood_notes.h"
#include "../src/melodee_screen.h"
#include "../src/params.h"
#include "../src/presets.h"
#include "../src/ui/page.h"

#include <clap/clap.h>

#include <algorithm>
#include <cmath>
#include <complex>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dlfcn.h>
#include <fstream>
#include <iterator>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace
{
int failures = 0, checks = 0;
#define CHECK(c) do { ++checks; if (! (c)) { ++failures; std::printf ("FAIL %s:%d  %s\n", __FILE__, __LINE__, #c); } } while (0)
#define CHECK_NEAR(a, b, tol) do { ++checks; const double va = (a), vb = (b); if (! (std::fabs (va - vb) <= (tol))) { ++failures; \
    std::printf ("FAIL %s:%d  %s = %g, %s = %g (tolerance %g)\n", __FILE__, __LINE__, #a, va, #b, vb, (double) (tol)); } } while (0)

using namespace reverie;
const clap_plugin_entry* entry = nullptr;
const clap_plugin_factory* factory = nullptr;
std::string outDir;

int remoteChanges = 0;
int rescans = 0, textRescans = 0, callbacks = 0, paints = 0, processRequests = 0, presetsLoaded = 0, presetErrors = 0, flushes = 0;
const void* hostExtension (const clap_host*, const char* id)
{
    static const clap_host_params params { [] (const clap_host*, clap_param_rescan_flags f) {
                                              rescans += (f & CLAP_PARAM_RESCAN_VALUES) != 0;
                                              textRescans += (f & CLAP_PARAM_RESCAN_TEXT) != 0; },
                                           [] (const clap_host*, clap_id, clap_param_clear_flags) {}, [] (const clap_host*) { ++flushes; } };
    static const clap_host_remote_controls remote { [] (const clap_host*) { ++remoteChanges; }, nullptr };
    if (std::strcmp (id, CLAP_EXT_REMOTE_CONTROLS) == 0) return &remote;
    static const melodee_screen_host_t screen { [] (const void*, uint32_t) { ++paints; } };
    static const clap_host_preset_load presetLoad {
        [] (const clap_host*, uint32_t, const char*, const char*, int32_t, const char*) { ++presetErrors; },
        [] (const clap_host*, uint32_t, const char*, const char*) { ++presetsLoaded; },
    };
    if (std::strcmp (id, MELODEE_SCREEN_CLAP_HOST) == 0) return &screen;
    if (std::strcmp (id, CLAP_EXT_PARAMS) == 0) return &params;
    if (std::strcmp (id, CLAP_EXT_PRESET_LOAD) == 0) return &presetLoad;
    return nullptr;
}

clap_host host {
    CLAP_VERSION, nullptr, "Reverie test host", "tests", "", "1",
    hostExtension, [] (const clap_host*) {}, [] (const clap_host*) { ++processRequests; }, [] (const clap_host*) { ++callbacks; },
};

struct Events
{
    struct Any { union { clap_event_header header; clap_event_param_value param; clap_event_note note; clap_event_midi midi; }; };
    std::vector<Any> list;
    clap_input_events in { this, [] (const clap_input_events* e) { return (uint32_t) static_cast<const Events*> (e->ctx)->list.size(); },
                           [] (const clap_input_events* e, uint32_t i) { return &static_cast<const Events*> (e->ctx)->list[i].header; } };
    void param (uint32_t time, clap_id id, double value)
    {
        Any a {};
        a.param = { { sizeof (clap_event_param_value), time, CLAP_CORE_EVENT_SPACE_ID, CLAP_EVENT_PARAM_VALUE, 0 }, id, nullptr, -1, -1, -1, -1, value };
        list.push_back (a);
    }
    void note (uint32_t time, uint16_t type, int16_t key, double velocity, int32_t id = -1, int16_t channel = 0)
    {
        Any a {};
        a.note = { { sizeof (clap_event_note), time, CLAP_CORE_EVENT_SPACE_ID, type, 0 }, id, 0, channel, key, velocity };
        list.push_back (a);
    }
    void midi (uint32_t time, uint8_t status, uint8_t d1, uint8_t d2)
    {
        Any a {};
        a.midi.header = { sizeof (clap_event_midi), time, CLAP_CORE_EVENT_SPACE_ID, CLAP_EVENT_MIDI, 0 };
        a.midi.port_index = 0, a.midi.data[0] = status, a.midi.data[1] = d1, a.midi.data[2] = d2;
        list.push_back (a);
    }
    void sort() { std::stable_sort (list.begin(), list.end(), [] (const Any& a, const Any& b) { return a.header.time < b.header.time; }); }
};

// What a test plays: at a frame, a parameter (plain value), a note, a choke or MIDI bytes.
struct Cue
{
    uint64_t frame;
    enum Kind { param, on, off, midi, choke } kind;
    uint32_t a = 0;   // parameter id, key, or MIDI status
    double b = 0.0;   // value, velocity, or MIDI data 1
    int c = 0;        // MIDI data 2, or a note's CLAP note id (-1: none)
    int16_t channel = 0;
};
Cue P (uint64_t f, uint32_t id, double v) { return { f, Cue::param, id, v, 0 }; }
Cue On (uint64_t f, int key, double vel = 0.8, int noteId = -1, int16_t channel = 0) { return { f, Cue::on, (uint32_t) key, vel, noteId, channel }; }
Cue Off (uint64_t f, int key, int noteId = -1, int16_t channel = 0) { return { f, Cue::off, (uint32_t) key, 0.0, noteId, channel }; }
Cue Choke (uint64_t f, int key, int noteId = -1, int16_t channel = 0) { return { f, Cue::choke, (uint32_t) key, 0.0, noteId, channel }; }
Cue Midi (uint64_t f, int status, int d1, int d2) { return { f, Cue::midi, (uint32_t) status, (double) d1, d2 }; }

struct Stereo { std::vector<float> l, r; };

struct Instance
{
    const clap_plugin* plugin = nullptr;
    const clap_plugin_params* params = nullptr;
    const clap_plugin_state* state = nullptr;
    const clap_plugin_preset_load* presets = nullptr;
    const melodee_screen_t* screen = nullptr;
    double rate;
    std::vector<clap_process_status> statuses;
    std::vector<std::pair<clap_id, double>> told; // the output param events

    explicit Instance (double sampleRate = 48000.0, uint32_t maxFrames = 4096, bool activate = true) : rate (sampleRate)
    {
        const auto* d = factory->get_plugin_descriptor (factory, 0);
        plugin = factory->create_plugin (factory, &host, d->id);
        CHECK (plugin != nullptr && plugin->init (plugin));
        params = static_cast<const clap_plugin_params*> (plugin->get_extension (plugin, CLAP_EXT_PARAMS));
        state = static_cast<const clap_plugin_state*> (plugin->get_extension (plugin, CLAP_EXT_STATE));
        presets = static_cast<const clap_plugin_preset_load*> (plugin->get_extension (plugin, CLAP_EXT_PRESET_LOAD));
        screen = static_cast<const melodee_screen_t*> (plugin->get_extension (plugin, MELODEE_SCREEN_CLAP));
        if (activate)
        {
            CHECK (plugin->activate (plugin, rate, 1, maxFrames));
            CHECK (plugin->start_processing (plugin));
        }
    }
    ~Instance()
    {
        plugin->stop_processing (plugin);
        plugin->deactivate (plugin);
        plugin->destroy (plugin);
    }
    double value (uint32_t id)
    {
        double v = 0.0;
        params->get_value (plugin, id, &v);
        return v;
    }
    bool preset (const char* key) { return presets->from_location (plugin, CLAP_PRESET_DISCOVERY_LOCATION_PLUGIN, nullptr, key); }
    void flush (std::vector<std::pair<uint32_t, double>> values)
    {
        Events e;
        for (auto& [id, v] : values) e.param (0, id, v);
        clap_output_events out { this, [] (const clap_output_events*, const clap_event_header*) { return true; } };
        params->flush (plugin, &e.in, &out);
    }

    Stereo render (uint64_t frames, const std::vector<Cue>& cues = {}, uint32_t block = 128, bool playing = false, double bpm = 120.0)
    {
        Stereo s;
        s.l.resize (frames), s.r.resize (frames);
        statuses.clear();
        clap_event_transport t {};
        t.header = { sizeof (t), 0, CLAP_CORE_EVENT_SPACE_ID, CLAP_EVENT_TRANSPORT, 0 };
        clap_output_events out { this, [] (const clap_output_events* o, const clap_event_header* h) {
                                     if (h->type == CLAP_EVENT_PARAM_VALUE)
                                     {
                                         const auto* v = reinterpret_cast<const clap_event_param_value*> (h);
                                         static_cast<Instance*> (o->ctx)->told.push_back ({ v->param_id, v->value });
                                     }
                                     return true; } };
        for (uint64_t at = 0; at < frames; at += block)
        {
            const uint32_t n = (uint32_t) std::min<uint64_t> (block, frames - at);
            Events e;
            for (const auto& c : cues)
                if (c.frame >= at && c.frame < at + n)
                {
                    const uint32_t time = (uint32_t) (c.frame - at);
                    switch (c.kind)
                    {
                        case Cue::param: e.param (time, c.a, c.b); break;
                        case Cue::on: e.note (time, CLAP_EVENT_NOTE_ON, (int16_t) c.a, c.b, c.c, c.channel); break;
                        case Cue::off: e.note (time, CLAP_EVENT_NOTE_OFF, (int16_t) (int) c.a, 0.0, c.c, c.channel); break;
                        case Cue::choke: e.note (time, CLAP_EVENT_NOTE_CHOKE, (int16_t) (int) c.a, 0.0, c.c, c.channel); break;
                        case Cue::midi: e.midi (time, (uint8_t) c.a, (uint8_t) c.b, (uint8_t) c.c); break;
                    }
                }
            e.sort();
            float* ch[2] = { s.l.data() + at, s.r.data() + at };
            clap_audio_buffer buffer { ch, nullptr, 2, 0, 0 };
            t.flags = CLAP_TRANSPORT_HAS_TEMPO | CLAP_TRANSPORT_HAS_BEATS_TIMELINE | (playing ? CLAP_TRANSPORT_IS_PLAYING : 0);
            t.tempo = bpm;
            t.song_pos_beats = (clap_beattime) std::llround ((double) at / rate * bpm / 60.0 * (double) CLAP_BEATTIME_FACTOR);
            clap_process p {};
            p.frames_count = n;
            p.audio_outputs = &buffer;
            p.audio_outputs_count = 1;
            p.in_events = &e.in;
            p.out_events = &out;
            p.transport = &t;
            statuses.push_back (plugin->process (plugin, &p));
        }
        return s;
    }
};

double rms (const std::vector<float>& x, size_t from = 0, size_t to = SIZE_MAX)
{
    to = std::min (to, x.size());
    double s = 0.0;
    for (size_t i = from; i < to; ++i) s += (double) x[i] * x[i];
    return to > from ? std::sqrt (s / (double) (to - from)) : 0.0;
}
double peak (const Stereo& s, size_t from = 0, size_t to = SIZE_MAX)
{
    double p = 0.0;
    to = std::min (to, s.l.size());
    for (size_t i = from; i < to; ++i) p = std::max (p, (double) std::max (std::fabs (s.l[i]), std::fabs (s.r[i])));
    return p;
}
bool finite (const Stereo& s)
{
    for (size_t i = 0; i < s.l.size(); ++i)
        if (! std::isfinite (s.l[i]) || ! std::isfinite (s.r[i])) return false;
    return true;
}
bool same (const Stereo& a, const Stereo& b)
{
    return a.l.size() == b.l.size() && std::memcmp (a.l.data(), b.l.data(), a.l.size() * 4) == 0 && std::memcmp (a.r.data(), b.r.data(), a.r.size() * 4) == 0;
}

// The strongest frequency between lo and hi Hz in x (a Hann-windowed FFT, interpolated between bins).
double strongest (const std::vector<float>& x, size_t from, size_t n, double rate, double lo, double hi)
{
    size_t size = 1;
    while (size < n) size <<= 1;
    std::vector<std::complex<double>> a (size);
    for (size_t i = 0; i < n && from + i < x.size(); ++i) a[i] = x[from + i] * (0.5 - 0.5 * std::cos (2.0 * M_PI * (double) i / (double) n));
    for (size_t i = 1, j = 0; i < size; ++i)
    {
        size_t bit = size >> 1;
        for (; j & bit; bit >>= 1) j ^= bit;
        j ^= bit;
        if (i < j) std::swap (a[i], a[j]);
    }
    for (size_t len = 2; len <= size; len <<= 1)
    {
        const std::complex<double> step = std::polar (1.0, -2.0 * M_PI / (double) len);
        for (size_t i = 0; i < size; i += len)
        {
            std::complex<double> w (1.0);
            for (size_t k = 0; k < len / 2; ++k, w *= step)
            {
                const auto u = a[i + k], v = a[i + k + len / 2] * w;
                a[i + k] = u + v, a[i + k + len / 2] = u - v;
            }
        }
    }
    const double bin = rate / (double) size;
    size_t best = 1;
    for (size_t k = (size_t) (lo / bin); k < std::min (size / 2 - 1, (size_t) (hi / bin)); ++k)
        if (std::abs (a[k]) > std::abs (a[best])) best = k;
    const double l = std::abs (a[best - 1]), c = std::abs (a[best]), r = std::abs (a[best + 1]);
    const double off = 0.5 * (l - r) / (l - 2.0 * c + r);
    return ((double) best + off) * bin;
}

// The dry sound: one oscillator, no unison, drift, motion, LFO or effects, the filter open (for pitch and voicing).
std::vector<Cue> dry()
{
    using namespace ParamId;
    std::vector<Cue> c;
    for (auto [id, v] : std::vector<std::pair<uint32_t, double>> {
             { osc1Wave, 11 }, { osc1Pos, 0 }, { osc1Unison, 1 }, { osc1Detune, 0 }, { osc2Level, 0 }, { air, 0 }, { drift, 0 },
             { cutoff, 1 }, { filterEnv, 0 }, { keyTrack, 0 }, { velCutoff, 0 }, { lfoPitch, 0 }, { lfoCutoff, 0 }, { lfoWave, 0 },
             { motionShape, 0 }, { ensMix, 0 }, { dlyMix, 0 }, { revMix, 0 }, { strum, 0 }, { humanize, 0 }, { ampAttack, 0 },
             { ampSustain, 1 }, { width, 0 } })
        c.push_back (P (0, id, v));
    return c;
}

std::vector<Cue> operator+ (std::vector<Cue> a, const std::vector<Cue>& b)
{
    a.insert (a.end(), b.begin(), b.end());
    return a;
}

// --- Tests -----------------------------------------------------------------------------------------------------------

void testDescriptor()
{
    CHECK (factory->get_plugin_count (factory) == 1);
    const auto* d = factory->get_plugin_descriptor (factory, 0);
    CHECK (std::strcmp (d->id, "com.hdslave.reverie") == 0);
    CHECK (std::strcmp (d->name, "hdslave Reverie") == 0);
    bool instrument = false, stereo = false;
    for (auto f = d->features; *f; ++f) instrument |= std::strcmp (*f, CLAP_PLUGIN_FEATURE_INSTRUMENT) == 0, stereo |= std::strcmp (*f, CLAP_PLUGIN_FEATURE_STEREO) == 0;
    CHECK (instrument && stereo);
    Instance inst;
    const auto* audio = static_cast<const clap_plugin_audio_ports*> (inst.plugin->get_extension (inst.plugin, CLAP_EXT_AUDIO_PORTS));
    const auto* notes = static_cast<const clap_plugin_note_ports*> (inst.plugin->get_extension (inst.plugin, CLAP_EXT_NOTE_PORTS));
    CHECK (audio->count (inst.plugin, false) == 1 && audio->count (inst.plugin, true) == 0);
    clap_audio_port_info ai {};
    CHECK (audio->get (inst.plugin, 0, false, &ai) && ai.channel_count == 2);
    clap_note_port_info ni {};
    CHECK (notes->count (inst.plugin, true) == 1 && notes->get (inst.plugin, 0, true, &ni));
    CHECK ((ni.supported_dialects & CLAP_NOTE_DIALECT_MIDI) && (ni.supported_dialects & CLAP_NOTE_DIALECT_CLAP));
    CHECK (inst.screen != nullptr);
}

void testParams()
{
    Instance inst;
    CHECK (inst.params->count (inst.plugin) == (uint32_t) paramCount);
    std::set<clap_id> ids;
    for (uint32_t i = 0; i < (uint32_t) paramCount; ++i)
    {
        clap_param_info info {};
        CHECK (inst.params->get_info (inst.plugin, i, &info));
        CHECK (info.id == paramTable[i].id);
        CHECK (paramIndex (info.id) == (int) i);
        if (i < ParamId::mood) CHECK (info.id == i);
        ids.insert (info.id);
        CHECK (info.min_value < info.max_value);
        CHECK (info.default_value >= info.min_value && info.default_value <= info.max_value);
        CHECK (std::strlen (paramTable[i].label) <= 6);
        CHECK_NEAR (inst.value (info.id), info.default_value, 0.0);
        if (paramTable[i].kind == Kind::choice)
            for (int c = 0; c <= (int) (paramTable[i].max - paramTable[i].min); ++c) CHECK (paramTable[i].choices[c] != nullptr && paramTable[i].choices[c][0]);
        // text round trips (to the value as shown)
        for (int s = 0; s <= 8; ++s)
        {
            const double v = paramTable[i].kind == Kind::continuous ? info.min_value + (info.max_value - info.min_value) * s / 8.0
                                                                   : std::round (info.min_value + (info.max_value - info.min_value) * s / 8.0);
            char text[64];
            CHECK (inst.params->value_to_text (inst.plugin, info.id, v, text, sizeof (text)));
            double back = 0.0;
            CHECK (inst.params->text_to_value (inst.plugin, info.id, text, &back));
            char again[64];
            inst.params->value_to_text (inst.plugin, info.id, back, again, sizeof (again));
            if (std::strcmp (text, again) != 0) std::printf ("  text %s: %s -> %g -> %s\n", info.name, text, back, again);
            CHECK (std::strcmp (text, again) == 0);
        }
        double nope = 0.0;
        CHECK (! inst.params->text_to_value (inst.plugin, info.id, "nan", &nope));
    }
    CHECK (ids.size() == (size_t) paramCount);
    for (int i = 0; i < progressionCount; ++i)
    {
        char name[64];
        CHECK (inst.params->value_to_text (inst.plugin, 99, i, name, sizeof (name)));
        CHECK (std::strcmp (name, progressionTable[i].name) == 0);
    }
    CHECK (std::strcmp (progressionTable[0].name, "Velvet Night") == 0);
    CHECK (std::strcmp (progressionTable[45].name, "Moonstone") == 0);
    for (clap_id archived = 84; archived <= 96; ++archived)
    {
        double unused;
        CHECK (! inst.params->get_value (inst.plugin, archived, &unused));
    }
    {   // the Key reads as the page shows it: in the progression's mode (Db and Ab in a major one, C# and G# in a minor
        // one), either spelling reading back; moving to the other mode has the host read the text again
        auto keyText = [&] (double key) {
            static char text[16];
            CHECK (inst.params->value_to_text (inst.plugin, ParamId::key, key, text, sizeof (text)));
            return std::string (text);
        };
        auto keyValue = [&] (const char* text) {
            double v = -1.0;
            CHECK (inst.params->text_to_value (inst.plugin, ParamId::key, text, &v));
            return v;
        };
        int minorProgression = -1;
        for (int p = 0; p < progressionCount && minorProgression < 0; ++p)
            if (progressionTable[p].minor) minorProgression = p;
        CHECK (! progressionTable[0].minor && minorProgression > 0);
        CHECK (keyText (1) == "Db" && keyText (8) == "Ab" && keyText (6) == "F#");
        const int callbacksBefore = callbacks, textBefore = textRescans;
        inst.flush ({ { ParamId::publicId (ParamId::originalProgression), (double) minorProgression } });
        CHECK (callbacks > callbacksBefore);
        inst.plugin->on_main_thread (inst.plugin);
        CHECK (textRescans == textBefore + 1);
        CHECK (keyText (1) == "C#" && keyText (8) == "G#" && keyText (3) == "Eb");
        CHECK_NEAR (keyValue ("C#"), 1, 0);
        CHECK_NEAR (keyValue ("Db"), 1, 0);
        CHECK_NEAR (keyValue ("g#"), 8, 0);
        CHECK_NEAR (keyValue ("Ab"), 8, 0);
        inst.flush ({ { ParamId::publicId (ParamId::originalProgression), (double) minorProgression } }); // the same mode: nothing to read again
        inst.plugin->on_main_thread (inst.plugin);
        CHECK (textRescans == textBefore + 1);
        inst.flush ({ { ParamId::publicId (ParamId::originalProgression), 0.0 } });
        inst.plugin->on_main_thread (inst.plugin);
        CHECK (textRescans == textBefore + 2);
        CHECK (keyText (1) == "Db");
    }
    clap_param_info pastEnd {};
    CHECK (! inst.params->get_info (inst.plugin, (uint32_t) paramCount, &pastEnd));
    double v = 0.0;
    CHECK (! inst.params->get_value (inst.plugin, 9999, &v));
    // a flush while active sets values (and is told)
    inst.flush ({ { ParamId::cutoff, 0.3 }, { ParamId::progression, 7 } });
    CHECK_NEAR (inst.value (ParamId::cutoff), 0.3, 1e-6);
    CHECK_NEAR (inst.value (ParamId::progression), 7, 0);
    // out of range and non-finite values are clamped, steps rounded
    inst.flush ({ { ParamId::resonance, 7.0 }, { ParamId::osc1Unison, 2.6 }, { ParamId::volume, NAN } });
    CHECK_NEAR (inst.value (ParamId::resonance), 1.0, 0);
    CHECK_NEAR (inst.value (ParamId::osc1Unison), 3.0, 0);
    CHECK_NEAR (inst.value (ParamId::volume), paramTable[ParamId::volume].def, 1e-6);
}

void testProgressions()
{
    for (int p = 0; p < allProgressionCount; ++p)
    {
        CHECK (parsedProgressions()[p].ok);
        if (! parsedProgressions()[p].ok) std::printf ("  progression %s does not parse\n", progressionAt (p).name);
        CHECK (std::strlen (progressionAt (p).name) <= 18);
    }
    // names: no doubled accidentals or the odd ones in any key
    for (int p = 0; p < allProgressionCount; ++p)
        for (int k = 0; k < 12; ++k)
            for (int c = 0; c < chordsPerProgression; ++c)
            {
                char root[16];
                chordRootText (parsedProgressions()[p].chords[c], k, progressionAt (p).minor, root, sizeof (root));
                CHECK (std::strstr (root, "bb") == nullptr && std::strstr (root, "##") == nullptr);
                CHECK (std::strstr (root, "Cb") == nullptr && std::strstr (root, "Fb") == nullptr && std::strstr (root, "E#") == nullptr && std::strstr (root, "B#") == nullptr);
                CHECK (std::strlen (root) <= 6);
            }
    // a few spellings
    char s[24];
    chordText (parsedProgressions()[0].chords[0], 0, false, s, sizeof (s));
    CHECK (std::strcmp (s, "C") == 0);
    chordText (parsedProgressions()[0].chords[0], 3, false, s, sizeof (s));
    CHECK (std::strcmp (s, "Eb") == 0);
    chordText (parsedProgressions()[4].chords[6], 7, false, s, sizeof (s)); // Frost Dawn's D7/F# in G: A7/C#
    CHECK (std::strcmp (s, "A7/C#") == 0);
    chordText (parsedProgressions()[2].chords[2], 0, false, s, sizeof (s));
    CHECK (std::strcmp (s, "Gm7(11)") == 0);
    // every quality a progression names fits its pad on the page (40 pixels less the 4 in front, at size 10)
    for (int p = 0; p < allProgressionCount; ++p)
        for (const ChordSymbol& c : parsedProgressions()[p].chords)
            CHECK (hdui::Raster::measure (chordQualityText (c), 10) <= 36);
    // ... and so does every label the page draws beside it, at the size and place the page draws it: each pad's
    // root line in every key (size 12, size 10 once wider than 25, as Screen::padGrid picks), the skyline's
    // progression and voicing (from x 8, a rule and 12 pixels between them, across 320), and its key-and-mode
    // pill (from x 141). The sounds' names are the host's list to draw; they are held to the same full width.
    for (int p = 0; p < allProgressionCount; ++p)
    {
        for (int k = 0; k < 12; ++k)
            for (int c = 0; c < chordsPerProgression; ++c)
            {
                char root[16];
                chordRootText (parsedProgressions()[p].chords[c], k, progressionAt (p).minor, root, sizeof (root));
                const int rootSize = hdui::Raster::measure (root, 12) > 25 ? 10 : 12;
                CHECK (hdui::Raster::measure (root, rootSize) <= 36);
            }
        for (int v = 0; v < Voicing::count; ++v)
            CHECK (8 + hdui::Raster::measure (progressionAt (p).name, 12) + 12 + hdui::Raster::measure (voicingChoices[v], 12) <= 320);
        for (int k = 0; k < 12; ++k)
        {
            char mode[20];
            std::snprintf (mode, sizeof (mode), "%s %s", keyTonic (k, progressionAt (p).minor), progressionAt (p).minor ? "minor" : "major");
            CHECK (141 + hdui::Raster::measure (mode, 10) + 12 <= 320);
        }
    }
    for (const auto& preset : presetTable)
        CHECK (hdui::Raster::measure (preset.name, 12) <= 312);
    // every voicing of every progression in every key is a sound chord, and the table the plug-in reads (voiced when
    // it was built, src/voicings.h) holds exactly the chords the search gives
    int chords = 0, flat9 = 0, differ = 0, lifted = 0, withColour = 0;
    for (int p = 0; p < factoryBank::progressionCount; ++p)
        for (int v = 0; v < Voicing::count; ++v)
            for (int k = 0; k < 12; ++k)
            {
                VoicingSet set, searched;
                padChords (p, v, k, set);
                voiceProgression (p, v, k, searched);
                const bool same = std::memcmp (&set, &searched, sizeof (set)) == 0;
                differ += ! same;
                CHECK (same);
                for (int slot = 0; slot < padSlots; ++slot)
                {
                    const ChordNotes& c = set.slot[slot];
                    const ChordSymbol& sym = parsedProgressions()[p].chords[slot % 8];
                    const int colour = slot >= 8 ? liftTone (sym, parsedProgressions()[p].pitches) : -1;
                    ++chords;
                    CHECK (c.count >= 3 && c.count <= maxChordNotes);
                    CHECK (c.hasBass == (v != Voicing::rootless));
                    for (int i = 0; i + 1 < c.count; ++i) CHECK (c.note[i] < c.note[i + 1]);
                    for (int i = 0; i < c.count; ++i) CHECK (c.note[i] >= 30 && c.note[i] <= 100);
                    if (c.hasBass)
                    {
                        const int bass = ((sym.bassPc >= 0 ? sym.bassPc : sym.rootPc) + k) % 12;
                        CHECK (c.note[0] % 12 == bass);
                        CHECK (c.note[0] >= bassLow && c.note[0] <= bassHigh);
                        CHECK (c.note[1] - c.note[0] >= 3);
                    }
                    // every tone is the chord's (a lifted pad's colour tone aside); Rootless never plays the root of a
                    // chord of four tones or more
                    const Quality& q = qualities[sym.quality];
                    bool coloured = false;
                    for (int i = c.hasBass ? 1 : 0; i < c.count; ++i)
                    {
                        const int rel = ((c.note[i] - sym.rootPc - k) % 12 + 12) % 12;
                        bool in = false;
                        for (int t = 0; t < q.count; ++t) in |= rel == q.tones[t];
                        if (sym.bassPc >= 0) in |= rel == 0 || rel == (sym.bassPc - sym.rootPc + 12) % 12;
                        coloured |= colour >= 0 && rel == colour;
                        CHECK (in || (colour >= 0 && rel == colour));
                        if (v == Voicing::rootless && sym.bassPc < 0 && q.count > 3) CHECK (rel != 0);
                    }
                    if (colour >= 0)
                    {
                        ++withColour;
                        CHECK (coloured);
                    }
                    if (slot >= 8) ++lifted;
                    // the third (or the sus) is there
                    bool third = false;
                    for (int i = c.hasBass ? 1 : 0; i < c.count; ++i)
                    {
                        const int rel = ((c.note[i] - sym.rootPc - k) % 12 + 12) % 12;
                        third |= rel == q.tones[0];
                    }
                    if (! (sym.bassPc >= 0 && (sym.bassPc - sym.rootPc + 12) % 12 == q.tones[0])) CHECK (third);
                    // minor ninths only where the chord has them
                    for (int i = c.hasBass ? 1 : 0; i < c.count; ++i)
                        for (int j = i + 1; j < c.count; ++j)
                            if ((c.note[j] - c.note[i]) > 12 && (c.note[j] - c.note[i]) % 12 == 1)
                            {
                                ++flat9;
                                CHECK (minorNinthsAllowed (sym, colour));
                            }
                }
            }
    std::printf ("  %d chords voiced, %d minor ninths (all where the chord allows them), %d of %d sets unlike the table\n", chords,
                 flat9, differ, factoryBank::progressionCount * Voicing::count * 12);
    std::printf ("  %d of %d lifted pads add a colour tone the progression uses\n", withColour, lifted);
    // the bass, chosen for the whole progression: a bass that stays stays on its note, one that moves a step or a
    // semitone moves just that far, not by an octave. Only a line that runs further than G1-C3 holds breaks, once.
    int broken = 0;
    for (int p = 0; p < factoryBank::progressionCount; ++p)
        for (int k = 0; k < 12; ++k)
        {
            VoicingSet set;
            padChords (p, Voicing::neoSoul, k, set);
            const ParsedProgression& pp = parsedProgressions()[p];
            int breaks = 0;
            for (int i = 0; i + 1 < chordsPerProgression; ++i)
            {
                const auto bassPc = [&] (int c) { return pp.chords[c].bassPc >= 0 ? pp.chords[c].bassPc : pp.chords[c].rootPc; };
                int step = ((bassPc (i + 1) - bassPc (i)) % 12 + 12) % 12;
                if (step > 6) step -= 12;
                if (std::abs (step) <= 2) breaks += set.slot[i + 1].note[0] - set.slot[i].note[0] != step;
            }
            CHECK (breaks <= 1);
            broken += breaks;
        }
    std::printf ("  bass lines broken by the range: %d of %d progressions in a key\n", broken, factoryBank::progressionCount * 12);
    const ChordNotes odd = padChord (-5, 99, 14, 40), edge = padChord (0, Voicing::count - 1, 2, padSlots - 1); // out of range: clamped
    CHECK (std::memcmp (&odd, &edge, sizeof (odd)) == 0);
    // All 52,992 original pad voicings, in desktop order. Include the bass flag and every MIDI note.
    uint64_t originalHash = 14695981039346656037ull;
    auto add = [&] (int byte) { originalHash = (originalHash ^ (uint8_t) byte) * 1099511628211ull; };
    float values[paramCount] {};
    for (int p = 0; p < progressionCount; ++p)
    {
        values[ParamId::originalProgression] = (float) p;
        for (int v = 0; v < Voicing::count; ++v)
            for (int k = 0; k < 12; ++k)
                for (int slot = 0; slot < padSlots; ++slot)
                {
                    const auto c = performanceChord (selectedProgression (values), v, k, slot, 0, 0);
                    add (c.count | (c.hasBass ? 0x80 : 0));
                    for (int n = 0; n < c.count; ++n) add (c.note[n]);
                }
    }
    CHECK (originalHash == 0x65792b2a421b52ddull);
    // the pads and the banks
    CHECK (padOf (36).slot == 0 && padOf (36).shift == 0);
    CHECK (padOf (51).slot == 15 && padOf (51).shift == 0);
    CHECK (padOf (52).slot == 0 && padOf (52).shift == 12);
    CHECK (padOf (35).slot == 15 && padOf (35).shift == -12);
    CHECK (padOf (127).shift == 24 && padOf (0).shift == -24);
}

void testPadMap (const char* path)
{
    std::ifstream f (path, std::ios::binary);
    const std::string bytes ((std::istreambuf_iterator<char> (f)), std::istreambuf_iterator<char>());
    const size_t at = bytes.find ("{\"pad_layout\":\"drums\"");
    CHECK (at != std::string::npos);
    if (at == std::string::npos) return;
    const std::string json = bytes.substr (at, bytes.find ('\0', at) - at);
    int found = 0;
    for (int k = 36; k <= 51; ++k)
        found += json.find ("\"note\":" + std::to_string (k) + "}") != std::string::npos;
    CHECK (found == 16);
    CHECK (json.find ("Chord 1") != std::string::npos && json.find ("Lift 8") != std::string::npos);
}

void testState()
{
    std::vector<uint8_t> saved;
    {
        Instance a;
        a.flush ({ { 99, 45 }, { 97, 2 }, { 98, -3 }, { ParamId::progression, 12 }, { ParamId::key, 5 }, { ParamId::cutoff, 0.42 }, { ParamId::osc2Wave, 9 }, { ParamId::latch, 1 },
                   { ParamId::phaserMix, 0.74 }, { ParamId::phaserRate, 0.36 }, { ParamId::phaserFeedback, 0.63 }, { ParamId::phaserEnv, -0.85 } });
        clap_ostream out { &saved, [] (const clap_ostream* s, const void* d, uint64_t n) -> int64_t {
                              auto* v = static_cast<std::vector<uint8_t>*> (s->ctx);
                              v->insert (v->end(), (const uint8_t*) d, (const uint8_t*) d + n);
                              return (int64_t) n; } };
        CHECK (a.state->save (a.plugin, &out));
    }
    CHECK (saved.size() == 12 + 8 * (size_t) paramCount);
    struct Reader { const std::vector<uint8_t>* v; size_t at; };
    auto load = [] (Instance& b, const std::vector<uint8_t>& bytes) {
        Reader r { &bytes, 0 };
        clap_istream in { &r, [] (const clap_istream* s, void* d, uint64_t n) -> int64_t {
                             auto* rd = static_cast<Reader*> (s->ctx);
                             const uint64_t left = rd->v->size() - rd->at, take = std::min (left, n);
                             std::memcpy (d, rd->v->data() + rd->at, take);
                             rd->at += take;
                             return (int64_t) take; } };
        return b.state->load (b.plugin, &in);
    };
    {
        Instance b;
        CHECK (load (b, saved));
        CHECK_NEAR (b.value (99), 45, 0);
        CHECK_NEAR (b.value (100), 0, 0);
        CHECK_NEAR (b.value (97), 2, 0);
        CHECK_NEAR (b.value (98), -3, 0);
        CHECK_NEAR (b.value (ParamId::progression), 12, 0);
        CHECK_NEAR (b.value (ParamId::key), 5, 0);
        CHECK_NEAR (b.value (ParamId::cutoff), 0.42, 1e-6);
        CHECK_NEAR (b.value (ParamId::osc2Wave), 9, 0);
        CHECK_NEAR (b.value (ParamId::latch), 1, 0);
    }
    {   // A released 80-value project opened over a wet patch restores the old, dry sound.
        constexpr uint32_t oldCount = 80;
        static_assert (ParamId::volume == oldCount - 1 && ParamId::phaserMix == oldCount);
        const uint32_t oldVersion = 2;
        auto old = saved;
        old.resize (12 + 8 * oldCount);
        std::memcpy (old.data() + 4, &oldVersion, sizeof (oldVersion));
        std::memcpy (old.data() + 8, &oldCount, sizeof (oldCount));
        Instance restored, dryPatch;
        CHECK (load (restored, saved));
        CHECK_NEAR (restored.value (ParamId::phaserMix), 0.74, 1e-6);
        CHECK_NEAR (restored.value (ParamId::phaserRate), 0.36, 1e-6);
        CHECK_NEAR (restored.value (ParamId::phaserFeedback), 0.63, 1e-6);
        CHECK_NEAR (restored.value (ParamId::phaserEnv), -0.85, 1e-6);
        restored.render (1); // let the activated engine consume the wet state
        CHECK (load (restored, old));
        CHECK_NEAR (restored.value (97), 0, 0);
        CHECK_NEAR (restored.value (ParamId::phaserMix), 0, 0);
        for (auto id : { ParamId::phaserRate, ParamId::phaserFeedback, ParamId::phaserEnv })
            CHECK_NEAR (restored.value (id), paramTable[id].def, 0);
        dryPatch.render (1);
        dryPatch.flush ({ { 100, 1 }, { ParamId::progression, 12 }, { ParamId::key, 5 }, { ParamId::cutoff, 0.42 }, { ParamId::osc2Wave, 9 }, { ParamId::latch, 1 } });
        const std::vector<Cue> chord { On (0, 36), Off (4000, 36) };
        const Stereo reference = dryPatch.render (12000, chord);
        CHECK (rms (reference.l) > 1e-3);
        CHECK (same (reference, restored.render (12000, chord)));
    }
    {   // The Mood release stored the factory Prog, Mood and Shift, without either of the new controls.
        auto old = saved;
        old.resize (12 + 8 * 86);
        old[4] = 2;
        old[8] = 86;
        const float lastFactory = 207.0f;
        std::memcpy (old.data() + 16, &lastFactory, sizeof (lastFactory)); // first pair: Prog at ID 0
        Instance loaded, reference;
        CHECK (load (loaded, old));
        CHECK_NEAR (loaded.value (100), 1, 0);
        CHECK_NEAR (loaded.value (0), 207, 0);
        CHECK_NEAR (loaded.value (97), 2, 0);
        CHECK_NEAR (loaded.value (98), -3, 0);
        CHECK (load (reference, saved));
        reference.flush ({ { 100, 1 }, { 0, 207 } });
        const std::vector<Cue> phrase { On (0, 36), Off (2048, 36), P (4096, 97, 0), On (4096, 51) };
        CHECK (same (loaded.render (8192, phrase), reference.render (8192, phrase)));
    }
    {   // unknown ids skipped, missing ones default; a wrong tag refused
        std::vector<uint8_t> odd = { 'R', 'V', 'R', 'E', 1, 0, 0, 0, 2, 0, 0, 0 };
        auto pair = [&] (uint32_t id, float v) { const uint8_t* a = (const uint8_t*) &id; const uint8_t* b = (const uint8_t*) &v; odd.insert (odd.end(), a, a + 4); odd.insert (odd.end(), b, b + 4); };
        pair (5000, 1.0f), pair (ParamId::key, 9.0f);
        Instance b;
        CHECK (load (b, odd));
        CHECK_NEAR (b.value (ParamId::key), 9, 0);
        CHECK_NEAR (b.value (ParamId::cutoff), paramTable[ParamId::cutoff].def, 1e-6);
        std::vector<uint8_t> bad = saved;
        bad[0] = 'X';
        CHECK (! load (b, bad));
    }
    {   // a state from a newer Reverie than this one is refused whole, changing nothing
        std::vector<uint8_t> future = saved;
        future[4] = 99;
        Instance b;
        CHECK (! load (b, future));
        CHECK_NEAR (b.value (ParamId::cutoff), paramTable[ParamId::cutoff].def, 1e-6);
        CHECK_NEAR (b.value (ParamId::progression), paramTable[ParamId::progression].def, 0);
    }
    {   // a version-1 state with an original slot recovers that Prog
        std::vector<uint8_t> old = saved;
        old.resize (12 + 8 * 80);
        old[4] = 1;
        old[8] = 80;
        Instance b;
        CHECK (load (b, old));
        CHECK_NEAR (b.value (99), 12, 0);
        CHECK_NEAR (b.value (100), 0, 0);
        CHECK_NEAR (b.value (ParamId::key), 5, 0);
        CHECK_NEAR (b.value (ParamId::cutoff), 0.42, 1e-6);
        CHECK_NEAR (b.value (ParamId::latch), 1, 0);
    }
    {   // loaded while inactive, it plays as loaded
        Instance b (48000.0, 4096, false);
        CHECK (load (b, saved));
        CHECK (b.plugin->activate (b.plugin, 48000.0, 1, 4096) && b.plugin->start_processing (b.plugin));
        CHECK_NEAR (b.value (ParamId::progression), 12, 0);
        const Stereo s = b.render (24000, { On (0, 36) });
        CHECK (rms (s.l) > 1e-3);
    }
}

void testPresets()
{
    Instance inst;
    // the chord section stays as it is through every sound
    inst.flush ({ { 99, 29 }, { 100, 1 }, { 97, 18 }, { 98, 7 }, { ParamId::progression, 9 }, { ParamId::key, 3 }, { ParamId::voicing, 2 }, { ParamId::latch, 1 }, { ParamId::strum, 0.6 },
                  { ParamId::bass, 0.3 }, { ParamId::octave, -1 } });
    std::set<std::string> keys, names;
    for (const auto& p : presetTable)
    {
        keys.insert (p.key), names.insert (p.name);
        const int before = presetsLoaded;
        CHECK (inst.preset (p.key));
        CHECK (presetsLoaded == before + 1);
        CHECK_NEAR (inst.value (99), 29, 0);
        CHECK_NEAR (inst.value (100), 1, 0);
        CHECK_NEAR (inst.value (97), 18, 0);
        CHECK_NEAR (inst.value (98), 7, 0);
        CHECK_NEAR (inst.value (ParamId::progression), 9, 0);
        CHECK_NEAR (inst.value (ParamId::key), 3, 0);
        CHECK_NEAR (inst.value (ParamId::voicing), 2, 0);
        CHECK_NEAR (inst.value (ParamId::latch), 1, 0);
        CHECK_NEAR (inst.value (ParamId::strum), 0.6, 1e-6);
        CHECK_NEAR (inst.value (ParamId::bass), 0.3, 1e-6);
        CHECK_NEAR (inst.value (ParamId::octave), -1, 0);
        for (int i = 0; i < p.count; ++i)
        {
            CHECK (p.values[i].id >= ParamId::chordSection && p.values[i].id < (uint32_t) paramCount);
            const Param& q = paramTable[p.values[i].id];
            CHECK (p.values[i].value >= q.min && p.values[i].value <= q.max);
            CHECK_NEAR (inst.value (p.values[i].id), clampParam (q, p.values[i].value), 1e-6);
        }
    }
    CHECK (keys.size() == (size_t) presetCount && names.size() == (size_t) presetCount);
    const int errors = presetErrors;
    CHECK (! inst.preset ("no-such-sound"));
    CHECK (presetErrors == errors + 1);
    // preset discovery lists every sound
    const auto* pf = static_cast<const clap_preset_discovery_factory*> (entry->get_factory (CLAP_PRESET_DISCOVERY_FACTORY_ID));
    CHECK (pf != nullptr && pf->count (pf) == 1);
    struct Indexer { int locations = 0; } ix;
    clap_preset_discovery_indexer indexer { CLAP_VERSION, "test", "", "", "", &ix,
                                            [] (const clap_preset_discovery_indexer*, const clap_preset_discovery_filetype*) { return true; },
                                            [] (const clap_preset_discovery_indexer* i, const clap_preset_discovery_location*) { ++static_cast<Indexer*> (i->indexer_data)->locations; return true; },
                                            [] (const clap_preset_discovery_indexer*, const clap_preset_discovery_soundpack*) { return true; },
                                            [] (const clap_preset_discovery_indexer*, const char*) -> const void* { return nullptr; } };
    const auto* provider = pf->create (pf, &indexer, pf->get_descriptor (pf, 0)->id);
    CHECK (provider != nullptr && provider->init (provider) && ix.locations == 1);
    struct Got { std::vector<std::string> keys; } got;
    clap_preset_discovery_metadata_receiver r {};
    r.receiver_data = &got;
    r.on_error = [] (const clap_preset_discovery_metadata_receiver*, int32_t, const char*) {};
    r.begin_preset = [] (const clap_preset_discovery_metadata_receiver* m, const char*, const char* key) { static_cast<Got*> (m->receiver_data)->keys.push_back (key); return true; };
    r.add_plugin_id = [] (const clap_preset_discovery_metadata_receiver*, const clap_universal_plugin_id*) {};
    r.set_soundpack_id = [] (const clap_preset_discovery_metadata_receiver*, const char*) {};
    r.set_flags = [] (const clap_preset_discovery_metadata_receiver*, uint32_t) {};
    r.add_creator = [] (const clap_preset_discovery_metadata_receiver*, const char*) {};
    r.set_description = [] (const clap_preset_discovery_metadata_receiver*, const char*) {};
    r.set_timestamps = [] (const clap_preset_discovery_metadata_receiver*, clap_timestamp, clap_timestamp) {};
    r.add_feature = [] (const clap_preset_discovery_metadata_receiver*, const char*) {};
    r.add_extra_info = [] (const clap_preset_discovery_metadata_receiver*, const char*, const char*) {};
    CHECK (provider->get_metadata (provider, CLAP_PRESET_DISCOVERY_LOCATION_PLUGIN, nullptr, &r));
    CHECK (got.keys.size() == (size_t) presetCount);
    provider->destroy (provider);
    // every sound plays, bounded, and none is far louder than the rest
    for (const auto& p : presetTable)
    {
        Instance a;
        CHECK (a.preset (p.key));
        const Stereo s = a.render (5 * 48000, { On (0, 36, 0.8), Off (48000 * 2, 36), On (48000 * 2, 37, 0.8), Off (48000 * 4, 37) });
        const double level = 20.0 * std::log10 (std::max (rms (s.l), 1e-9));
        CHECK (finite (s));
        CHECK (peak (s) <= 1.0);
        if (level < -30.0 || level > -12.0) std::printf ("  %s plays at %.1f dBFS RMS\n", p.key, level);
        CHECK (level > -30.0 && level < -12.0);
        if (std::string (p.key) == "reverie") writeWav ((outDir + "/demo-reverie.wav").c_str(), s.l, s.r, 48000);
    }
}

// The engine itself, for what only it can tell (which voices sound, at which pitch).
struct Bench
{
    Telemetry tel;
    Engine e { &tel };
    std::vector<float> l, r;
    explicit Bench (double rate = 48000.0)
    {
        for (int i = 0; i < paramCount; ++i) e.set ((uint32_t) i, paramTable[i].def);
        e.prepare (rate);
    }
    void run (int frames)
    {
        l.resize ((size_t) frames), r.resize ((size_t) frames);
        e.render (l.data(), r.data(), (uint32_t) frames);
    }
    int voices() const { return __builtin_popcount (tel.sounding.load()); }
    std::vector<int> notes() const
    {
        std::vector<int> n;
        for (int i = 0; i < maxVoices; ++i)
            if ((tel.sounding.load() >> i) & 1u) n.push_back (tel.notes[i].load());
        std::sort (n.begin(), n.end());
        return n;
    }
};

// The sound for the stealing tests: one sine oscillator, the chords four octaves down (notes of a few to seventy
// hertz, so their own bends are tiny next to a step), no effects, unison, drift, LFO or Motion, a slow attack, held
// at full sustain, quiet enough that the limiter never bends the sum.
// The progression the stealing tests play: its pad 2 (F13) is six notes in Open, lifted too (a 13 chord has no colour
// tone left to add), so four strikes of it hold all 24 voices; its pads 1 and 3 (C13sus, Bb13sus) are six notes each.
int sixNoteProgression()
{
    for (int p = 0; p < factoryBank::progressionCount; ++p)
        if (std::strcmp (factoryBank::progressionTable[p].name, "Velvet Quartet") == 0) return p;
    return 0;
}

void slowSines (Bench& b, float volume)
{
    using namespace ParamId;
    b.e.set (progBank, 1);
    b.e.set (progression, (float) sixNoteProgression());
    for (auto [id, v] : std::vector<std::pair<uint32_t, float>> {
             { osc1Wave, 11 }, { osc1Pos, 0 }, { osc1Unison, 1 }, { osc1Detune, 0 }, { osc1Octave, -2 }, { osc2Level, 0 }, { air, 0 },
             { drift, 0 }, { cutoff, 1 }, { filterEnv, 0 }, { keyTrack, 0 }, { velCutoff, 0 }, { lfoPitch, 0 }, { lfoCutoff, 0 },
             { lfoWave, 0 }, { motionShape, 0 }, { ensMix, 0 }, { dlyMix, 0 }, { revMix, 0 }, { strum, 0 }, { humanize, 0 },
             { ampAttack, envKnob (300.0f) }, { ampSustain, 1 }, { width, 0 }, { octave, -2 }, { voicing, (float) Voicing::open },
             { velocity, 1 }, { ParamId::volume, volume } })
        b.e.set (id, v);
}

// `x` and `ref` the same until `x` steals; then 128 samples of each: the sharpest bend (second difference) of x less
// ref (the stolen notes' fade, as nothing else differs), against the stolen notes' level after it.
double stealBend (Bench& x, Bench& ref, const char* what)
{
    x.run (128), ref.run (128);
    double bend = 0.0, level = 0.0, d1 = 0.0, d2 = 0.0;
    for (int i = 0; i < 128; ++i)
    {
        const double d = (double) x.l[i] - (double) ref.l[i];
        bend = std::max (bend, std::fabs (d - 2.0 * d1 + d2));
        if (i >= 32) level = std::max (level, std::fabs (d));
        d2 = d1, d1 = d;
    }
    std::printf ("  %s: the sharpest bend %.3f of the stolen notes' level (%.4f)\n", what, bend / std::max (level, 1e-12), level);
    CHECK (level > 1e-3);
    return bend / std::max (level, 1e-12);
}

void testNotes()
{
    {   // a pad plays its chord: the voicing's notes, the bass among them
        Bench b;
        b.e.set (ParamId::strum, 0.0f), b.e.set (ParamId::humanize, 0.0f);
        b.e.noteOn (36, 0.8f);
        b.run (480);
        const ChordNotes& c = b.e.voicings().slot[0];
        std::vector<int> want (c.note, c.note + c.count);
        CHECK (b.notes() == want);
        CHECK (b.tel.lastSlot.load() == 0);
        CHECK ((b.tel.padsHeld.load() & 1u) != 0);
        // the pad up: they let go and end
        b.e.noteOff (36);
        b.run (480);
        CHECK (b.tel.padsHeld.load() == 0);
        b.run (48000 * 6);
        CHECK (b.voices() == 0);
    }
    {   // bank B: the same chord an octave up; Octave moves the whole chord
        Bench b;
        b.e.set (ParamId::strum, 0.0f);
        b.e.noteOn (52, 0.8f);
        b.run (240);
        const ChordNotes& c = b.e.voicings().slot[0];
        std::vector<int> want;
        for (int i = 0; i < c.count; ++i) want.push_back (c.note[i] + 12);
        CHECK (b.notes() == want);
        Bench d;
        d.e.set (ParamId::strum, 0.0f), d.e.set (ParamId::octave, -1.0f);
        d.e.noteOn (36, 0.8f);
        d.run (240);
        std::vector<int> down;
        for (int i = 0; i < c.count; ++i) down.push_back (c.note[i] - 12);
        CHECK (d.notes() == down);
    }
    {   // Bass at 0: no bass voice; Rootless: none either
        Bench b;
        b.e.set (ParamId::strum, 0.0f), b.e.set (ParamId::bass, 0.0f);
        b.e.noteOn (36, 0.8f);
        b.run (240);
        CHECK (b.voices() == b.e.voicings().slot[0].count - 1);
        Bench d;
        d.e.set (ParamId::strum, 0.0f), d.e.set (ParamId::voicing, (float) Voicing::rootless);
        d.e.noteOn (36, 0.8f);
        d.run (240);
        CHECK (d.voices() == d.e.voicings().slot[0].count && ! d.e.voicings().slot[0].hasBass);
    }
    {   // Strum: the notes enter one after another, bottom up, at its spacing
        Bench b;
        b.e.set (ParamId::strum, strumKnob (40.0f)), b.e.set (ParamId::humanize, 0.0f);
        b.e.noteOn (36, 0.8f);
        b.run (48 * 10);
        CHECK (b.voices() == 1);
        b.run (48 * 40);
        CHECK (b.voices() == 2);
        b.run (48 * 200);
        CHECK (b.voices() == b.e.voicings().slot[0].count);
    }
    {   // A pad let go before its strum is through: the late notes still play their turn, as long as the pad was
        // held, and every note ends
        Bench b;
        b.e.set (ParamId::strum, strumKnob (60.0f)), b.e.set (ParamId::humanize, 0.0f), b.e.set (ParamId::ampRelease, envKnob (100.0f));
        b.e.noteOn (36, 0.8f);
        b.run (48 * 30);
        b.e.noteOff (36);
        CHECK (b.voices() == 1);
        b.run (48 * 200);
        CHECK (b.voices() >= 2); // the later notes came in
        b.run (48000 * 3);
        CHECK (b.voices() == 0);
        // and a choke or All Notes Off before their turn: they never start
        Bench c;
        c.e.set (ParamId::strum, strumKnob (60.0f));
        c.e.noteOn (36, 0.8f);
        c.run (48 * 30);
        c.e.allNotesOff();
        c.run (48 * 400);
        CHECK (c.voices() <= 1);
        c.run (48000 * 6);
        CHECK (c.voices() == 0);
    }
    {   // (review) Latch + Glide: a chord struck on the same sample as a latched one whose notes have not begun
        // replaces it; nothing stays latched behind
        Bench b;
        b.e.set (ParamId::latch, 1.0f), b.e.set (ParamId::glide, glideKnob (300.0f)), b.e.set (ParamId::humanize, 1.0f);
        b.e.noteOn (36, 0.8f), b.e.noteOn (37, 0.8f);
        b.e.noteOff (36), b.e.noteOff (37);
        b.run (48000 * 2);
        CHECK (b.tel.padsHeld.load() == (1u << 1));
        b.e.noteOn (38, 0.8f);
        b.run (48000 * 6);
        CHECK (b.tel.padsHeld.load() == (1u << 2));
        CHECK (b.voices() == (int) b.e.voicings().slot[2].count);
    }
    {   // (review) Glide takes only notes that sound and are held: a chord let go mid-strum does not shrink the one
        // that glides from it
        Bench b;
        b.e.set (ParamId::strum, strumKnob (60.0f)), b.e.set (ParamId::humanize, 0.0f), b.e.set (ParamId::glide, glideKnob (200.0f));
        b.e.noteOn (36, 0.8f);
        b.run (48 * 30);
        b.e.noteOff (36);
        b.run (48 * 40);
        b.e.noteOn (37, 0.8f);
        b.run (48000 * 4);
        std::vector<int> want (b.e.voicings().slot[1].note, b.e.voicings().slot[1].note + b.e.voicings().slot[1].count);
        CHECK (b.notes() == want);
    }
    {   // (review) A pad struck again mid-strum: the old chord's notes yet to strum never start (no doubled strum)
        Bench b;
        b.e.set (ParamId::strum, strumKnob (60.0f)), b.e.set (ParamId::humanize, 0.0f);
        b.e.noteOn (36, 0.8f);
        b.run (48 * 100);
        const int started = b.voices();
        b.e.noteOn (36, 0.8f);
        b.run (48 * 400);
        CHECK (b.voices() <= started + (int) b.e.voicings().slot[0].count);
    }
    {   // (review) Sync + Retrig with the transport stopped: Motion still moves (at the tempo)
        Bench b;
        b.e.set (ParamId::motionSync, 1.0f), b.e.set (ParamId::motionRetrig, 1.0f), b.e.set (ParamId::motionDiv, 6.0f), b.e.set (ParamId::strum, 0.0f);
        b.e.transport (false, 0.0, 120.0);
        b.e.noteOn (36, 0.8f);
        b.run (480);
        const float x0 = b.tel.x[0].load(), y0 = b.tel.y[0].load();
        b.run (12000);
        CHECK (std::fabs (b.tel.x[0].load() - x0) + std::fabs (b.tel.y[0].load() - y0) > 0.05f);
    }
    {   // (review) A reset forgets the last strike: the same pad on the first sample after it plays
        Instance a;
        a.render (2048, { On (0, 36) });
        a.plugin->reset (a.plugin);
        const Stereo s = a.render (12000, { On (0, 36) });
        CHECK (rms (s.l) > 1e-3);
    }
    {   // (review) Stealing a sounding voice fades it out: no step in the output, even with every voice busy
        Instance a;
        std::vector<Cue> cues = dry() + std::vector<Cue> { P (0, ParamId::strum, strumKnob (25.0f)), P (0, ParamId::ampRelease, envKnob (4000.0f)),
                                                           P (0, ParamId::osc1Wave, 11), P (0, ParamId::voicing, Voicing::open),
                                                           P (0, ParamId::ampAttack, envKnob (20.0f)) };
        for (int k = 0; k < 24; ++k) cues.push_back (On (100 + 4000 * (uint64_t) k, 44 + (k % 8), 0.9)), cues.push_back (Off (2100 + 4000 * (uint64_t) k, 44 + (k % 8)));
        const Stereo s = a.render (48000 * 3, cues);
        double worst = 0.0, typical = 0.0;
        for (size_t i = 2; i < s.l.size(); ++i)
        {
            const double d2 = std::fabs ((double) s.l[i] - 2.0 * s.l[i - 1] + s.l[i - 2]);
            worst = std::max (worst, d2), typical += d2;
        }
        typical /= (double) s.l.size();
        std::printf ("  voices stolen: the sharpest bend %.1f times the typical\n", worst / typical);
        CHECK (worst < 40.0 * typical);
    }
    {   // Melodee: a key struck again while held (on, on, off): one chord sounding, the first letting go
        Bench b;
        b.e.set (ParamId::strum, 0.0f);
        b.e.noteOn (37, 0.8f);
        b.run (4800);
        const int n = b.voices();
        b.e.noteOn (37, 0.8f);
        b.run (480);
        CHECK (b.voices() == 2 * n); // the first chord's release and the new strike
        CHECK (__builtin_popcount (b.tel.padsHeld.load()) == 1);
        b.e.noteOff (37);
        b.run (480);
        CHECK (b.tel.padsHeld.load() == 0);
    }
    {   // On, off, on of one pad on one sample: the third strike plays (the off between them is not a same-sample
        // duplicate)
        Bench b;
        b.e.set (ParamId::strum, 0.0f), b.e.set (ParamId::humanize, 0.0f);
        b.e.noteOn (36, 0.8f);
        b.e.noteOff (36);
        b.e.noteOn (36, 0.8f);
        b.run (480);
        CHECK ((b.tel.padsHeld.load() & 1u) != 0);
        CHECK (b.voices() == (int) b.e.voicings().slot[0].count);
        b.run (48000 * 2);
        CHECK ((b.tel.padsHeld.load() & 1u) != 0);
    }
    {   // A wildcard note-off (every key) lets go of a latched chord too, as All Notes Off does; a latched chord's
        // own pad coming up still leaves it ringing
        Bench b;
        b.e.set (ParamId::latch, 1.0f), b.e.set (ParamId::strum, 0.0f);
        b.e.noteOn (36, 0.8f);
        b.run (480);
        b.e.noteOff (36);
        b.run (480);
        CHECK ((b.tel.padsHeld.load() & 1u) != 0);
        b.e.noteOff (-1);
        b.run (480);
        CHECK (b.tel.padsHeld.load() == 0);
    }
    {   // Ties and slides (Melodee sends CC 84 naming the held key, then the next note-on; CC 68 is the legato
        // pedal): the held chord goes on with no new attack
        Bench b;
        b.e.set (ParamId::strum, 0.0f), b.e.set (ParamId::humanize, 0.0f);
        b.e.noteOn (36, 0.8f);
        b.run (4800);
        const int n = b.voices();
        CHECK (n == (int) b.e.voicings().slot[0].count);
        b.e.controller (84, 36); // a tie: the same pad again
        b.e.noteOn (36, 0.8f);
        b.run (480);
        CHECK (b.voices() == n); // no second strike sounding beside the first
        CHECK (b.tel.padsHeld.load() == 1u);
        b.e.controller (84, 36); // a slide: on to the next pad, its own note-off still to come
        b.e.noteOn (37, 0.8f);
        b.run (48000);
        std::vector<int> want (b.e.voicings().slot[1].note, b.e.voicings().slot[1].note + b.e.voicings().slot[1].count);
        CHECK (b.notes() == want);
        CHECK (b.tel.padsHeld.load() == (1u << 1));
        b.e.noteOff (36); // the slide's late note-off finds nothing on the old pad
        b.run (480);
        CHECK (b.tel.padsHeld.load() == (1u << 1));
        // CC 84 naming a pad that is not held is spent: the next strike is a strike
        Bench c;
        c.e.set (ParamId::strum, 0.0f), c.e.set (ParamId::humanize, 0.0f);
        c.e.controller (84, 50);
        c.e.noteOn (36, 0.8f);
        c.run (480);
        CHECK (c.voices() == (int) c.e.voicings().slot[0].count);
        CHECK (c.tel.padsHeld.load() == 1u);
        // Reset All Controllers drops a pending slide and the legato pedal
        Bench d;
        d.e.set (ParamId::strum, 0.0f), d.e.set (ParamId::humanize, 0.0f);
        d.e.noteOn (36, 0.8f);
        d.run (480);
        d.e.controller (84, 36);
        d.e.controller (121, 0);
        d.e.noteOn (37, 0.8f);
        d.run (480);
        CHECK (d.tel.padsHeld.load() == (1u | (1u << 1))); // a plain second strike: both pads hold
    }
    {   // A slide travels at Chord Glide's pace; with the legato pedal down (CC 68) it lands at once
        Bench b;
        b.e.set (ParamId::strum, 0.0f), b.e.set (ParamId::humanize, 0.0f), b.e.set (ParamId::glide, 1.0f);
        b.e.noteOn (36, 0.8f);
        b.run (4800);
        b.e.controller (84, 36);
        b.e.noteOn (37, 0.8f);
        b.run (480);
        std::vector<int> want (b.e.voicings().slot[1].note, b.e.voicings().slot[1].note + b.e.voicings().slot[1].count);
        CHECK (b.notes() != want); // still on its way
        b.run (48000 * 4);
        CHECK (b.notes() == want);
        Bench c;
        c.e.set (ParamId::strum, 0.0f), c.e.set (ParamId::humanize, 0.0f), c.e.set (ParamId::glide, 1.0f);
        c.e.noteOn (36, 0.8f);
        c.run (4800);
        c.e.controller (68, 127);
        c.e.controller (84, 36);
        c.e.noteOn (37, 0.8f);
        c.run (480);
        std::vector<int> wantC (c.e.voicings().slot[1].note, c.e.voicings().slot[1].note + c.e.voicings().slot[1].count);
        CHECK (c.notes() == wantC); // there at once
    }
    {   // Two strikes of one pad on one sample play the louder once, exactly
        Instance a, c;
        const Stereo twice = a.render (24000, { On (0, 38, 0.5), On (0, 38, 0.9) });
        const Stereo once = c.render (24000, { On (0, 38, 0.9) });
        CHECK (same (twice, once));
        Instance d, e2;
        const Stereo softer = d.render (24000, { On (0, 38, 0.9), On (0, 38, 0.4) });
        CHECK (same (softer, e2.render (24000, { On (0, 38, 0.9) })));
    }
    {   // Latch: the chord rings on with the pad up, and gives way to the next
        Bench b;
        b.e.set (ParamId::latch, 1.0f), b.e.set (ParamId::strum, 0.0f);
        b.e.noteOn (36, 0.8f);
        b.run (480);
        b.e.noteOff (36);
        b.run (48000 * 2);
        CHECK ((b.tel.padsHeld.load() & 1u) != 0);
        b.e.noteOn (39, 0.8f);
        b.run (480);
        CHECK (b.tel.padsHeld.load() == (1u << 3));
        b.e.noteOff (39);
        b.run (480);
        CHECK (b.tel.padsHeld.load() == (1u << 3));
        // Latch off: the latched chord lets go
        b.e.set (ParamId::latch, 0.0f);
        b.run (480);
        CHECK (b.tel.padsHeld.load() == 0);
        // CC 123: latched or not, every chord lets go
        b.e.set (ParamId::latch, 1.0f);
        b.run (16);
        b.e.noteOn (40, 0.8f);
        b.run (480);
        b.e.controller (123, 0);
        b.run (480);
        CHECK (b.tel.padsHeld.load() == 0);
    }
    {   // Sustain: held past the pad until the pedal lifts
        Bench b;
        b.e.controller (64, 127);
        b.e.noteOn (36, 0.8f);
        b.run (480);
        b.e.noteOff (36);
        b.run (4800);
        CHECK ((b.tel.padsHeld.load() & 1u) != 0);
        b.e.controller (64, 0);
        b.run (480);
        CHECK (b.tel.padsHeld.load() == 0);
    }
    {   // Chord Glide: a chord struck while one is held takes its voices, which slide to the new notes
        Bench b;
        b.e.set (ParamId::glide, glideKnob (400.0f)), b.e.set (ParamId::strum, 0.0f), b.e.set (ParamId::humanize, 0.0f);
        b.e.noteOn (36, 0.8f);
        b.run (4800);
        const int n = b.voices();
        b.e.noteOn (37, 0.8f);
        b.run (480);
        const ChordNotes& c2 = b.e.voicings().slot[1];
        CHECK (b.voices() <= std::max (n, (int) c2.count) + std::max (0, n - (int) c2.count)); // no new strike of the shared notes
        b.run (48000);
        std::vector<int> held;
        for (int i = 0; i < maxVoices; ++i)
            if (((b.tel.sounding.load() >> i) & 1u)) held.push_back (b.tel.notes[i].load());
        std::sort (held.begin(), held.end());
        std::vector<int> want (c2.note, c2.note + c2.count);
        for (int w : want) CHECK (std::find (held.begin(), held.end(), w) != held.end());
        b.e.noteOff (36); // the first pad no longer holds anything
        b.run (480);
        CHECK ((b.tel.padsHeld.load() & 2u) != 0);
    }
    {   // A choke stops its key's voices within a few milliseconds (the tails ring on); All Sound Off (CC 120 at 0)
        // stops everything, tails too; CC 120 and 123 at another value do nothing
        Bench k;
        k.e.noteOn (36, 0.8f), k.e.noteOn (40, 0.8f);
        k.run (24000);
        const int both = k.voices();
        k.e.choke (36);
        k.run (480);
        CHECK (k.voices() < both && k.voices() > 0);
        CHECK ((k.tel.padsHeld.load() & 1u) == 0 && (k.tel.padsHeld.load() & 16u) != 0);
        Instance a;
        const Stereo s = a.render (48000, { On (0, 36), Choke (24000, 36), Midi (24000, 0xb0, 120, 0) });
        CHECK (peak (s, 24000 + 960, 48000) < 0.02);
        Instance c;
        const Stereo t = c.render (48000, { On (0, 36), Midi (24000, 0xb0, 120, 0) });
        CHECK (peak (t, 24000 + 960, 48000) < 0.02);
        Instance d;
        const Stereo u = d.render (48000, { On (0, 36), Midi (24000, 0xb0, 120, 64), Midi (24000, 0xb0, 123, 64) });
        CHECK (rms (u.l, 30000, 48000) > 0.01);
        Instance e2;
        const Stereo w = e2.render (48000, { On (0, 36), Midi (0, 0xb0, 123, 0) });
        CHECK (rms (w.l, 0, 48000) < rms (u.l, 0, 48000));
    }
    {   // a MIDI note plays as a CLAP one
        Instance a, c;
        CHECK (same (a.render (12000, { Midi (0, 0x90, 40, 100) }), c.render (12000, { On (0, 40, 100.0 / 127.0) })));
    }
    {   // velocity: softer is quieter
        Instance a, c;
        const double loud = rms (a.render (24000, { On (0, 36, 1.0) }).l), soft = rms (c.render (24000, { On (0, 36, 0.3) }).l);
        CHECK (soft < loud * 0.8);
    }
    {   // every voice taken: the oldest letting go give way, nothing breaks
        Bench b;
        b.e.set (ParamId::strum, 0.0f), b.e.set (ParamId::voicing, (float) Voicing::open);
        for (int k = 0; k < 16; ++k) { b.e.noteOn (36 + k, 0.8f); b.run (240); }
        CHECK (b.voices() <= maxVoices);
        CHECK (b.voices() >= 20);
        for (float x : b.l) CHECK (std::isfinite (x));
    }
    {   // (review) A voice stolen on a tick's first sample (a block's start, its tick yet to run) fades over the whole
        // ramp, as one stolen mid-tick does: no step. A loud chord and three near silent ones hold the 24 voices; a
        // fifth, near silent, takes the loud one's six. Their fade is a ramp: its bends stay at its corners, a 32nd
        // of their level (a step of half of it before).
        for (int at : { 0, 8 })
        {
            Bench x, ref;
            for (Bench* b : { &x, &ref })
            {
                slowSines (*b, 0.1f);
                b->e.noteOn (37, 1.0f), b->run (16);
                for (int k : { 45, 53, 61 }) b->e.noteOn (k, 0.05f), b->run (16);
                b->run (48000 - 64 + at);
            }
            CHECK (x.voices() == maxVoices);
            x.e.noteOn (36, 0.05f);
            CHECK (stealBend (x, ref, at == 0 ? "stolen on a tick's first sample" : "stolen mid-tick") < 1.0 / 16.0);
        }
    }
    {   // (review) Twelve sounding voices stolen on one sample (more than the eight fade slots there were): each fades,
        // none is cut
        Bench x, ref;
        for (Bench* b : { &x, &ref })
        {
            slowSines (*b, 0.03f);
            for (int k : { 37, 45, 53, 61 }) b->e.noteOn (k, 1.0f), b->run (16);
            b->run (48000 - 64 + 8);
        }
        CHECK (x.voices() == maxVoices);
        x.e.noteOn (36, 0.05f), x.e.noteOn (38, 0.05f);
        CHECK (stealBend (x, ref, "twelve stolen at once") < 1.0 / 16.0);
    }
    {   // (review) Five chords struck on one sample (more notes than the 24 voices): each takes the earliest chords'
        // notes, never its own, and never those of a chord struck after them on that sample
        Bench b;
        b.e.set (ParamId::strum, 0.0f), b.e.set (ParamId::humanize, 0.0f), b.e.set (ParamId::voicing, (float) Voicing::open);
        for (int k : { 44, 45, 46, 47, 48 }) b.e.noteOn (k, 0.8f);
        b.run (480);
        const VoicingSet& set = b.e.voicings();
        std::vector<int> all;
        for (int slot = 8; slot <= 12; ++slot)
            for (int i = 0; i < set.slot[slot].count; ++i) all.push_back (set.slot[slot].note[i]);
        CHECK (all.size() > (size_t) maxVoices);
        std::vector<int> want (all.end() - maxVoices, all.end());
        std::sort (want.begin(), want.end());
        CHECK (want.size() == (size_t) maxVoices);
        CHECK (b.notes() == want);
    }
    {   // (review) Reset All Controllers (CC 121 at 0) lifts the sustain pedal (RP-015): the chords it held let go, and
        // the next lets go with its pad
        Bench b;
        b.e.controller (64, 127);
        b.e.noteOn (36, 0.8f);
        b.run (480);
        b.e.noteOff (36);
        b.run (480);
        CHECK ((b.tel.padsHeld.load() & 1u) != 0);
        b.e.controller (121, 0);
        b.run (480);
        CHECK (b.tel.padsHeld.load() == 0);
        b.e.noteOn (37, 0.8f);
        b.run (480);
        b.e.noteOff (37);
        b.run (480);
        CHECK (b.tel.padsHeld.load() == 0);
    }
    {   // The first pad's strike address survives a glide or a full voice steal on its sample.
        // A stale ID cannot unlock it; its own identified release does, including an ID-only off.
        for (bool gliding : { false, true })
        {
            for (int releaseKey : { -1, 36 })
            {
                Bench b;
                b.e.set (ParamId::glide, gliding ? 0.35f : 0.0f);
                b.e.set (ParamId::strum, 0.0f), b.e.set (ParamId::humanize, 0.0f);
                const int strikes = gliding ? 2 : 12;
                for (int i = 0; i < strikes; ++i) b.e.noteOn (36 + i, 0.8f, 2, 100 + i);
                b.e.noteOff (releaseKey, 1, 100); // wrong channel
                b.e.noteOff (releaseKey, 2, 999); // stale ID
                b.e.noteOn (36, 0.8f, 2, 200);
                CHECK (b.tel.strikes.load() == (uint32_t) strikes);
                b.e.noteOff (releaseKey, 2, 200);
                b.e.noteOn (36, 0.8f, 2, 201);
                b.run (128);
                CHECK (b.tel.strikes.load() == (uint32_t) strikes + 1);
                CHECK (b.tel.lastSlot.load() == 0 && (b.tel.padsHeld.load() & 1u));
            }
        }
    }
    {   // A louder duplicate arriving after another pad must have the same attack and later
        // random modulation as a single loud hit, including between two control ticks.
        const std::vector<Cue> patch { P (0, ParamId::strum, 0), P (0, ParamId::humanize, 0),
                                       P (0, ParamId::ampAttack, 0), P (0, ParamId::velocity, 1),
                                       P (0, ParamId::velCutoff, 0.9), P (0, ParamId::air, 0.3) };
        for (uint64_t t : { 0, 1, 7, 15, 16 })
        {
            Instance a, b;
            CHECK (same (a.render (4800, patch + std::vector<Cue> {
                            On (t, 36, 0.08), On (t, 37, 0.6), On (t, 36, 0.98), On (1021, 40) }),
                         b.render (4800, patch + std::vector<Cue> {
                            On (t, 36, 0.98), On (t, 37, 0.6), On (1021, 40) })));
        }
    }
    {   // An interleaved, softer duplicate on the same sample cannot replace the loud strike.
        Instance a, b;
        CHECK (same (a.render (12000, { On (0, 36, 0.9), On (0, 37, 0.5), On (0, 36, 0.1) }),
                     b.render (12000, { On (0, 36, 0.9), On (0, 37, 0.5) })));
    }
    {   // Key, note ID and channel all constrain a release. A channel wildcard releases just
        // that channel, even when both channels use the same pitch.
        const std::vector<Cue> chord { On (0, 36, 0.8, 10, 0), On (0, 36, 0.7, 11, 1) };
        Instance a, b, c;
        const Stereo scoped = a.render (36000, chord + std::vector<Cue> { Off (12000, -1, -1, 0) });
        CHECK (same (scoped, b.render (36000, chord + std::vector<Cue> { Off (12000, 36, 10, 0) })));
        CHECK (!same (scoped, c.render (36000, chord + std::vector<Cue> { Off (12000, -1, -1, -1) })));
        Instance d, e;
        CHECK (same (d.render (36000, chord + std::vector<Cue> { Off (12000, 36, 11, 0), Choke (16000, -1, 10, 1) }),
                     e.render (36000, chord)));
    }
    {   // A release from the first strike can follow a new strike of the same key.
        const std::vector<Cue> chord { On (0, 36, 0.8, 10), On (8000, 36, 0.8, 11) };
        Instance a, b;
        CHECK (same (a.render (36000, chord + std::vector<Cue> { Off (12000, 36, 10), Choke (16000, 36, 10) }),
                     b.render (36000, chord)));
    }
    {   // Real gain at zero velocity depends on the velocity knob. CLAP still strikes;
        // MIDI's zero-velocity note-on releases the preceding note.
        const std::vector<Cue> neutral { P (0, ParamId::velocity, 0), P (0, ParamId::humanize, 0) };
        Instance a, b, c, d;
        CHECK (same (a.render (12000, neutral + std::vector<Cue> { On (0, 36, 0) }),
                     b.render (12000, neutral + std::vector<Cue> { On (0, 36, 0.02) })));
        CHECK (same (c.render (20000, { On (0, 36), Midi (8000, 0x90, 36, 0) }),
                     d.render (20000, { On (0, 36), Off (8000, 36) })));
    }
    {   // The pedal holds the channel that pressed it. A raw MIDI key release and
        // a CLAP note-on can address the same note.
        Bench b;
        b.e.noteOn (36, 0.8f, 0, 10);
        b.e.noteOn (37, 0.8f, 1, 11);
        b.e.controller (64, 127, 1);
        b.e.noteOff (36, 0);
        b.e.noteOff (37, 1);
        b.run (480);
        CHECK (b.tel.padsHeld.load() == 2);
        b.e.controller (64, 0, 0);
        b.run (480);
        CHECK (b.tel.padsHeld.load() == 2);
        b.e.controller (64, 0, 1);
        b.run (480);
        CHECK (b.tel.padsHeld.load() == 0);
    }
    {   // (review) A note-off or a choke naming only a note id (key -1) lets go of that note's chord alone; one naming
        // an id never played does nothing
        auto two = [] (const std::vector<Cue>& more) { return std::vector<Cue> { On (0, 36, 0.8, 7), On (0, 40, 0.8, 8) } + more; };
        Instance a, b2, c2, d2, e3, f3;
        CHECK (same (a.render (36000, two ({ Off (12000, -1, 7) })), b2.render (36000, two ({ Off (12000, 36) }))));
        CHECK (same (c2.render (36000, two ({ Off (12000, -1, 99), Choke (12000, -1, 99) })), d2.render (36000, two ({}))));
        CHECK (same (e3.render (36000, two ({ Choke (12000, -1, 8) })), f3.render (36000, two ({ Choke (12000, 40) }))));
    }
    {   // Note-offs by note id still find their pad after more than 32 id-named notes, and the host's reset
        // forgets every id (a stale id must not let go of a later chord on the same pad)
        auto many = [] (const Cue& off) {
            // One chord held throughout, then 39 short id-named strikes (short releases, spaced out, so the held
            // chord is never stolen): more id-named notes than any small table holds.
            std::vector<Cue> cues { P (0, ParamId::ampRelease, envKnob (50.0f)), P (0, ParamId::strum, 0.0), On (0, 36, 0.8, 100) };
            for (int i = 1; i < 40; ++i)
            {
                cues.push_back (On (6000 * (uint64_t) i, 40 + i % 8, 0.8, 100 + i));
                cues.push_back (Off (6000 * (uint64_t) i + 2000, 40 + i % 8));
            }
            cues.push_back (off);
            return cues;
        };
        Instance a, b2;
        CHECK (same (a.render (6000 * 40 + 48000, many (Off (6000 * 40, -1, 100))),
                     b2.render (6000 * 40 + 48000, many (Off (6000 * 40, 36)))));
        Instance c1, c2;
        for (Instance* p : { &c1, &c2 })
        {
            p->render (12000, { On (0, 36, 0.8, 7) });
            p->plugin->reset (p->plugin);
        }
        CHECK (same (c1.render (36000, { On (0, 36, 0.8, 9), Off (12000, -1, 7) }),
                     c2.render (36000, { On (0, 36, 0.8, 9) })));
    }
    {   // (review) A pad let go by key, by MIDI or by a wildcard off, then struck again with no id, keeps its new
        // chord: the first strike's id no longer names the pad, so a late off or choke for that id alone does nothing
        Instance a, b2, c2, d2, e3, f3, g3, h3;
        const std::vector<Cue> byKey { On (0, 36, 0.8, 7), Off (12000, 36), On (24000, 36) };
        CHECK (same (a.render (72000, byKey + std::vector<Cue> { Off (36000, -1, 7) }), b2.render (72000, byKey)));
        const std::vector<Cue> byChoke { On (0, 36, 0.8, 7), Choke (12000, 36), On (24000, 36) };
        CHECK (same (c2.render (72000, byChoke + std::vector<Cue> { Off (36000, -1, 7) }), d2.render (72000, byChoke)));
        const std::vector<Cue> byMidi { On (0, 36, 0.8, 7), Midi (12000, 0x80, 36, 0), On (24000, 36) };
        CHECK (same (e3.render (72000, byMidi + std::vector<Cue> { Choke (36000, -1, 7) }), f3.render (72000, byMidi)));
        const std::vector<Cue> byWild { On (0, 36, 0.8, 7), Off (12000, -1), On (24000, 36) };
        CHECK (same (g3.render (72000, byWild + std::vector<Cue> { Off (36000, -1, 7) }), h3.render (72000, byWild)));
    }
    {   // (review) A value sent with a note while asleep is the note's from its first sample (no glide in from the
        // value before it): as if sent well before the note
        Instance a, c;
        CHECK (same (a.render (12000, { P (1000, ParamId::cutoff, 0.05), On (1000, 36) }),
                     c.render (12000, { P (744, ParamId::cutoff, 0.05), On (1000, 36) })));
    }
}

// The effects cleared (All Sound Off, the host's reset, falling asleep) do no work on their lines, yet sound exactly
// as if their lines had been wiped; with nothing to clear, a clear does nothing at all. `used` plays noise, `quiet`
// (the same controls) silence; both are cleared, then must give the same output to the bit while their lines fill
// again: the places `used` has not written since read as silence. (Not a new effect: a reset keeps the reverb's
// modulation where it is.)
template <class Fx, class Control> bool clearedSoundsWiped (Control control)
{
    Fx used, quiet;
    used.prepare (48000.0f), quiet.prepare (48000.0f);
    float l[tickSize], r[tickSize], l2[tickSize], r2[tickSize];
    uint32_t noise = 1;
    auto fill = [&] (bool loud) {
        for (int i = 0; i < tickSize; ++i)
        {
            noise = noise * 1664525u + 1013904223u;
            l[i] = loud ? (float) (int32_t) noise * 4.6e-10f : 0.0f, r[i] = -0.5f * l[i];
        }
    };
    for (int t = 0; t < 3000 * 4; ++t)
    {
        control (used, t + 30000), control (quiet, t + 30000);
        fill (false);
        quiet.process (l, r, tickSize);
        fill (true);
        used.process (l, r, tickSize);
    }
    used.reset(), quiet.reset();
    bool same = true;
    for (int t = 0; t < 3000 * 7; ++t) // past the longest line's refill (5.5 s), a sound for its first second
    {
        control (used, t), control (quiet, t);
        fill (t < 3000);
        std::memcpy (l2, l, sizeof (l)), std::memcpy (r2, r, sizeof (r));
        used.process (l, r, tickSize), quiet.process (l2, r2, tickSize);
        same &= std::memcmp (l, l2, sizeof (l)) == 0 && std::memcmp (r, r2, sizeof (r)) == 0;
    }
    return same;
}

void testEffects()
{
    {   // Check the first wet repeat in sample time, including a division longer than the line.
        using namespace ParamId;
        constexpr int rate = 16000;
        struct Tap { double bpm; int division; double seconds; };
        for (const Tap tap : { Tap { 30, 8, 2.0 }, Tap { 360, 8, 1.0 / 6.0 }, Tap { 20, 11, 3.2 } })
        {
            Instance inst (rate);
            const auto cues = dry() + std::vector<Cue> { P (0, ampRelease, 0), P (0, dlyMix, 0.8),
                P (0, dlyTime, tap.division), P (0, dlyFeedback, 0), On (0, 36), Off (160, 36) };
            const int expected = (int) std::lround (rate * tap.seconds);
            const Stereo audio = inst.render (expected + 800, cues, 128, false, tap.bpm);
            CHECK (peak (audio, rate / 10, expected - 2) < 1e-5);
            CHECK (peak (audio, expected, expected + 160) > 0.01);
        }
    }
    // each effect cleared after its lines are full of sound, then played (its times and sizes moving while the lines
    // fill again): as if wiped, to the bit
    CHECK (clearedSoundsWiped<Ensemble> ([] (Ensemble& e, int) { e.control (0.7f, 0.8f, 0.5f); }));
    CHECK (clearedSoundsWiped<PingPong> ([] (PingPong& d, int t) { d.control (0.6f, t < 9000 ? 0.3f : 3.1f, 0.8f); }));
    CHECK (clearedSoundsWiped<Reverb> ([] (Reverb& v, int t) { v.control (0.6f, t < 6000 ? 0.2f : 1.0f, 8.0f, 9000.0f, 120.0f, 0.7f); }));
    // All Sound Off on all sixteen channels (as Melodee sends it) costs what one does: each effect reset once, and it
    // sounds as one
    {
        Bench one, all;
        for (Bench* b : { &one, &all }) b->e.noteOn (36, 0.8f), b->run (24000);
        const uint32_t before = one.e.fxResets();
        CHECK (all.e.fxResets() == before);
        one.e.controller (120, 0);
        for (int ch = 0; ch < 16; ++ch) all.e.controller (120, 0);
        CHECK (one.e.fxResets() == before + 3); // the ensemble, the delay and the reverb, once each
        CHECK (all.e.fxResets() == before + 3);
        one.run (24000), all.run (24000);
        CHECK (one.l == all.l && one.r == all.r);
        Instance a, c;
        std::vector<Cue> sixteen = { On (0, 36) };
        for (int ch = 0; ch < 16; ++ch) sixteen.push_back (Midi (24000, 0xb0 | ch, 120, 0));
        CHECK (same (a.render (48000, sixteen), c.render (48000, { On (0, 36), Midi (24000, 0xb0, 120, 0) })));
    }
    // asleep, the tails were cleared as it fell asleep: All Sound Off and the host's reset find nothing to clear
    {
        Bench b;
        b.e.set (ParamId::revDecay, 0.0f), b.e.set (ParamId::dlyFeedback, 0.0f), b.e.set (ParamId::ampRelease, envKnob (200.0f));
        b.e.noteOn (36, 0.8f);
        b.run (4800);
        b.e.noteOff (36);
        const uint32_t awake = b.e.fxResets();
        for (int i = 0; i < 20 && ! b.e.quiet(); ++i) b.run (48000);
        CHECK (b.e.quiet());
        CHECK (b.e.fxResets() == awake + 3); // falling asleep
        for (int ch = 0; ch < 16; ++ch) b.e.controller (120, 0);
        b.e.reset();
        CHECK (b.e.fxResets() == awake + 3);
        // and wakes as usual
        b.e.noteOn (40, 0.8f);
        b.run (4800);
        CHECK (rms (b.l) > 1e-3);
    }
}

void testPhaser()
{
    using namespace ParamId;
    auto sweep = [] (double mix, double env, double attack) {
        // Keep the voice filter fully open and independent of its envelope, and feed the phaser a bright chord.
        return dry() + std::vector<Cue> { P (0, osc1Wave, 1), P (0, fltAttack, attack), P (0, fltDecay, envKnob (180)),
                                         P (0, fltSustain, 0.18), P (0, fltRelease, envKnob (110)), P (0, phaserMix, mix),
                                         P (0, phaserRate, 0.23), P (0, phaserFeedback, 0.85), P (0, phaserEnv, env),
                                         On (37, 36), Off (10799, 36) };
    };
    const double quick = envKnob (15), slow = envKnob (600);
    auto play = [&] (double mix, double env, double attack) {
        Instance a;
        return a.render (18000, sweep (mix, env, attack));
    };
    const Stereo dryRef = play (0, 0, quick);
    CHECK (same (dryRef, play (0, 1, slow)));
    const Stereo freeSweep = play (0.82, 0, quick);
    CHECK (same (freeSweep, play (0.82, 0, slow)));
    CHECK (! same (freeSweep, dryRef));
    const auto staticSweep = sweep (0.82, 0, quick) + std::vector<Cue> { P (0, phaserRate, 0) };
    Instance parked, noFeedback;
    const Stereo parkedAudio = parked.render (18000, staticSweep);
    CHECK (! same (freeSweep, parkedAudio));
    CHECK (! same (parkedAudio, noFeedback.render (18000, staticSweep + std::vector<Cue> { P (0, phaserFeedback, 0) })));
    const Stereo fastEnvelope = play (0.82, 1, quick), slowEnvelope = play (0.82, 1, slow);
    double difference = 0;
    for (size_t i = 0; i < fastEnvelope.l.size(); ++i)
        difference += std::pow ((double) fastEnvelope.l[i] - slowEnvelope.l[i], 2);
    CHECK (std::sqrt (difference / fastEnvelope.l.size()) > 1e-3);
    CHECK (! same (fastEnvelope, play (0.82, -1, quick)));

    const auto scene = sweep (0.82, 1, slow) + std::vector<Cue> {
        P (4199, phaserRate, 1), P (7134, phaserEnv, -1), P (9999, phaserFeedback, 1),
        On (10997, 41), P (12011, phaserMix, 0), P (18313, phaserMix, 1), Off (21111, 41) };
    for (double rate : { 8000.0, 44100.0, 48000.0, 96000.0, 192000.0 })
    {
        Instance a (rate);
        const Stereo audio = a.render (24000, scene);
        CHECK (finite (audio) && peak (audio) <= 1.0 && rms (audio.l) > 1e-3);
        if (rate == 48000.0)
        {
            for (uint32_t block : { 1u, 37u, 4096u })
            {
                Instance b (rate);
                CHECK (same (audio, b.render (24000, scene, block)));
            }
            // Both instances play the same notes so their free oscillator phases and random state agree.
            // Only one ran the phaser before reset. Its history must be gone on the next wet strike.
            auto bypass = scene;
            for (auto& cue : bypass)
                if (cue.kind == Cue::param && cue.a == phaserMix) cue.b = 0;
            Instance clean (rate);
            clean.render (24000, bypass);
            clean.plugin->reset (clean.plugin);
            a.plugin->reset (a.plugin);
            CHECK (peak (a.render (4096)) == 0.0);
            clean.render (4096);
            CHECK (same (clean.render (18000, scene), a.render (18000, scene)));
        }
    }
}

void testTiming()
{
    // the same output whatever the block size
    const std::vector<Cue> scene = { P (0, ParamId::motionShape, Motion::star), P (0, ParamId::strum, 0.4), On (100, 36), On (7777, 44, 0.6),
                                     P (9000, ParamId::cutoff, 0.4), Off (30000, 36), On (30001, 37), P (31000, ParamId::glide, 0.4), On (40000, 38),
                                     Midi (45000, 0xb0, 1, 90), Off (50000, 44), Off (60000, 37), Off (61000, 38) };
    Instance a;
    const Stereo ref = a.render (96000, scene, 128);
    CHECK (rms (ref.l) > 1e-3);
    for (uint32_t block : { 1u, 37u, 4096u })
    {
        Instance b;
        const bool ok = same (ref, b.render (96000, scene, block));
        if (! ok) std::printf ("  blocks of %u differ from blocks of 128\n", block);
        CHECK (ok);
    }
    // an event lands on its own sample: a note at frame 1000 is silent before it
    Instance c;
    const Stereo s = c.render (4000, { P (0, ParamId::ampAttack, 0), P (0, ParamId::strum, 0), On (1000, 36) });
    CHECK (peak (s, 0, 1000) == 0.0);
    CHECK (peak (s, 1000, 1200) > 0.0);
}

void testTuning()
{
    // the bass of Velvet Night's first chord (F2) in tune at several rates
    for (double rate : { 44100.0, 48000.0, 96000.0 })
    {
        Instance a (rate);
        const Stereo s = a.render ((uint64_t) (rate * 1.5), dry() + std::vector<Cue> { On (0, 36, 1.0) });
        const double want = 440.0 * std::pow (2.0, (41 - 69) / 12.0); // F2
        const double got = strongest (s.l, (size_t) (rate * 0.5), (size_t) (rate * 0.9), rate, want * 0.8, want * 1.2);
        CHECK_NEAR (1200.0 * std::log2 (got / want), 0.0, 1.0);
        CHECK (finite (s));
    }
    // the Key moves the bass
    Instance b;
    const Stereo s = b.render (72000, dry() + std::vector<Cue> { P (0, ParamId::key, 2), On (0, 36, 1.0) });
    const double want = 440.0 * std::pow (2.0, (43 - 69) / 12.0); // G2
    const double got = strongest (s.l, 24000, 43200, 48000.0, want * 0.8, want * 1.2);
    CHECK_NEAR (1200.0 * std::log2 (got / want), 0.0, 1.0);
}

void testExtremes()
{
    using namespace ParamId;
    // every knob at an end: bounded and finite
    for (int end = 0; end < 2; ++end)
    {
        Instance a;
        std::vector<Cue> cues;
        for (int i = (int) chordSection; i < paramCount; ++i) cues.push_back (P (0, paramTable[i].id, end ? paramTable[i].max : paramTable[i].min));
        cues.push_back (P (0, volume, 2.0));
        for (int k = 0; k < 8; ++k) cues.push_back (On (100 + 4800 * k, 36 + 2 * k, 1.0));
        const Stereo s = a.render (48000 * 3, cues);
        CHECK (finite (s));
        CHECK (peak (s) <= 1.0);
    }
    // the loudest sound we can make stays under 0 dBFS
    Instance b;
    const Stereo s = b.render (48000 * 2, { P (0, volume, 2.0), P (0, osc1Unison, 4), P (0, osc2Unison, 4), P (0, resonance, 1.0), P (0, voicing, Voicing::open),
                                            On (0, 44, 1.0), On (0, 45, 1.0), On (0, 46, 1.0) });
    CHECK (finite (s) && peak (s) <= 1.0);
    // pitch bend and a sweep of every motion path
    for (int shape = 0; shape < Motion::count; ++shape)
    {
        Instance c;
        const Stereo t = c.render (24000, { P (0, motionShape, shape), P (0, motionDepth, 1), P (0, motionRate, 1), On (0, 36), Midi (1000, 0xe0, 0, 127) });
        CHECK (finite (t) && peak (t) <= 1.0);
    }
}

void testSleep()
{
    Instance a;
    // silent from the start: asleep at once
    a.render (1280);
    CHECK (a.statuses.back() == CLAP_PROCESS_SLEEP);
    // a chord: awake while it and its tails sound, asleep after
    const Stereo s = a.render (48000 * 20, { P (0, ParamId::revDecay, 0.3), On (0, 36), Off (48000, 36) });
    CHECK (a.statuses[100] == CLAP_PROCESS_CONTINUE);
    CHECK (a.statuses.back() == CLAP_PROCESS_SLEEP);
    CHECK (peak (s, 48000 * 19, 48000 * 20) == 0.0);
    // and wakes for the next
    const Stereo t = a.render (24000, { On (0, 37) });
    CHECK (rms (t.l) > 1e-3);
}

// Model the stopped instrument gate, including its request_process wake path.
void testStoppedDelay()
{
    using namespace ParamId;
    constexpr int block = 128, rate = 48000;
    const std::vector<Cue> setup {
        P (0, ampAttack, 0), P (0, ampRelease, 0), P (0, strum, 0), P (0, humanize, 0),
        P (0, ensMix, 0), P (0, revMix, 0), P (0, dlyMix, 0.8), P (0, dlyTime, 11),
        P (0, dlyFeedback, 0), On (0, 36)
    };
    Instance inst;
    int quietFrames = 0, lastRequests = processRequests, startRequests = processRequests;
    double echo = 0;
    bool sleeping = false;
    for (int frame = 0; frame < 8 * rate; frame += block)
    {
        const bool tap = frame == 0, release = frame == 1920;
        if (tap || release || lastRequests != processRequests) quietFrames = 0;
        lastRequests = processRequests;
        sleeping = quietFrames >= rate / 2;
        if (sleeping) continue;
        const Stereo audio = inst.render (block, tap ? setup : release ? std::vector<Cue> { Off (0, 36) } : std::vector<Cue> {});
        const double level = peak (audio);
        quietFrames = level > 1e-5 ? 0 : quietFrames + block;
        if (frame >= rate && frame < rate + rate / 5) echo = std::max (echo, level);
    }
    CHECK (echo > 0.1 && sleeping);
    CHECK (processRequests > startRequests && processRequests - startRequests <= 33);
    const int before = processRequests;
    inst.render (rate, { P (0, dlyMix, 0), On (0, 36), Off (1920, 36) });
    inst.render (rate, { P (0, dlyMix, 0.8), On (0, 37), Off (1920, 37) }, block, true);
    CHECK (processRequests == before);
}

void testScreen()
{
    Instance inst;
    CHECK (inst.screen != nullptr);
    constexpr int stride = 327;
    constexpr uint32_t guard = 0x12345678;
    std::vector<uint32_t> px (stride * 218, guard);
    melodee_screen_frame_t c {};
    c.pixels = px.data(), c.width = 320, c.height = 218, c.stride = stride, c.area = MELODEE_SCREEN_PAGE, c.redraw_all = 1, c.colour = 0x008a5cffu;
    for (int k = 0; k < 3; ++k) c.pots[k] = -1, c.pots_live[k] = 1;
    CHECK (inst.screen->draw (inst.plugin, &c));
    c.redraw_all = 0;
    CHECK (! inst.screen->draw (inst.plugin, &c)); // nothing changed
    // K1-K3 on the CHORDS page: Progression, Voicing, Key
    uint32_t ids[3];
    inst.screen->controls (inst.plugin, ids);
    CHECK (ids[0] == 99 && ids[1] == ParamId::voicing && ids[2] == ParamId::key);
    auto* remote = static_cast<const clap_plugin_remote_controls*> (inst.plugin->get_extension (inst.plugin, CLAP_EXT_REMOTE_CONTROLS));
    clap_remote_controls_page page {};
    CHECK (remote && remote->count (inst.plugin) == 2);
    CHECK (remote->get (inst.plugin, 0, &page) && page.param_ids[0] == ids[0]);
    const int changes = remoteChanges;
    inst.flush ({ { 100, 1 } });
    inst.plugin->on_main_thread (inst.plugin);
    CHECK (remoteChanges == changes + 1);
    CHECK (remote->get (inst.plugin, 0, &page) && page.param_ids[0] == 0);
    inst.screen->controls (inst.plugin, ids);
    CHECK (ids[0] == 0);
    inst.flush ({ { 100, 0 }, { paramTable[ParamId::mood].id, 1 } });
    CHECK (inst.screen->draw (inst.plugin, &c));
    inst.screen->controls (inst.plugin, ids);
    CHECK (std::all_of (ids, ids + 3, [] (uint32_t id) { return id == MELODEE_SCREEN_NO_CONTROL; }));
    melodee_screen_input_t advance { MELODEE_SCREEN_WHEEL, 1, 0 };
    CHECK (inst.screen->input (inst.plugin, &advance));
    inst.screen->controls (inst.plugin, ids);
    CHECK (ids[0] == ParamId::octave);
    inst.flush ({ { paramTable[ParamId::mood].id, 0 } });
    advance = { MELODEE_SCREEN_WHEEL, -3, 0 };
    CHECK (inst.screen->input (inst.plugin, &advance));
    inst.screen->controls (inst.plugin, ids);
    CHECK (ids[0] == 99 && ids[1] == ParamId::voicing && ids[2] == ParamId::key);
    // a chord sounding, then each page drawn
    inst.render (24000, { On (0, 36), On (0, 44) });
    static const char* const names[] = { "chords", "motion", "osc", "filter", "amp", "fx" };
    for (int p = 0; p < 6; ++p)
    {
        c.time_ms += 100;
        c.redraw_all = 1;
        CHECK (inst.screen->draw (inst.plugin, &c));
        writePng ((outDir + "/screen-" + names[p] + ".png").c_str(), px.data(), 320, 218, stride, 2);
        // An animated partial repaint must produce exactly the complete picture, including panel edges.
        inst.render (1920);
        c.time_ms += 40;
        c.redraw_all = 0;
        inst.screen->draw (inst.plugin, &c);
        const auto incremental = px;
        std::fill (px.begin(), px.end(), guard);
        c.redraw_all = 1;
        CHECK (inst.screen->draw (inst.plugin, &c));
        CHECK (incremental == px);
        bool complete = true;
        for (int y = 0; y < 218; ++y)
            for (int x = 0; x < stride; ++x)
                complete &= (px[y * stride + x] == guard) == (x >= 320);
        CHECK (complete);
        if (p == 1)
        {
            inst.screen->controls (inst.plugin, ids);
            CHECK (ids[0] == ParamId::motionX && ids[1] == ParamId::motionY && ids[2] == ParamId::motionDepth);
        }
        melodee_screen_input_t next { MELODEE_SCREEN_STEP, 1, 0 };
        CHECK (inst.screen->input (inst.plugin, &next));
        inst.render (128);
    }
    // back on CHORDS: the wheel moves to KEY, Push edits it, the wheel turns it, and the host hears of it
    melodee_screen_input_t wheel { MELODEE_SCREEN_WHEEL, 1, 0 }, push { MELODEE_SCREEN_PUSH, 0, 0 };
    CHECK (inst.screen->input (inst.plugin, &wheel));
    CHECK (inst.screen->input (inst.plugin, &wheel));
    CHECK (inst.screen->input (inst.plugin, &push));
    const int before = rescans;
    CHECK (inst.screen->input (inst.plugin, &wheel));
    CHECK_NEAR (inst.value (ParamId::key), 1, 0);
    CHECK (rescans > before);
    inst.told.clear();
    inst.render (128);
    bool told = false;
    for (auto& [id, v] : inst.told) told |= id == ParamId::key && v == 1.0;
    CHECK (told);
    CHECK (inst.screen->input (inst.plugin, &push)); // done
    // Push on a switch (LATCH) flips it
    for (int i = 0; i < 6; ++i) inst.screen->input (inst.plugin, &wheel);
    inst.screen->controls (inst.plugin, ids);
    CHECK (ids[0] == ParamId::latch);
    CHECK (inst.screen->input (inst.plugin, &push));
    CHECK_NEAR (inst.value (ParamId::latch), 1, 0);
    // Shift draws the legend
    c.shift = 1;
    c.time_ms += 100;
    CHECK (inst.screen->draw (inst.plugin, &c));
    writePng ((outDir + "/screen-legend.png").c_str(), px.data(), 320, 218, stride, 2);
    // a bad canvas is refused
    melodee_screen_frame_t bad = c;
    bad.width = 100;
    CHECK (! inst.screen->draw (inst.plugin, &bad));
}
} // namespace

int main (int argc, char** argv)
{
    if (argc < 3) { std::printf ("host PLUGIN.clap OUT_FOLDER\n"); return 2; }
    outDir = argv[2];
    void* lib = dlopen (argv[1], RTLD_NOW | RTLD_LOCAL);
    if (lib == nullptr) { std::printf ("%s\n", dlerror()); return 1; }
    entry = static_cast<const clap_plugin_entry*> (dlsym (lib, "clap_entry"));
    CHECK (entry != nullptr && entry->init (argv[1]));
    factory = static_cast<const clap_plugin_factory*> (entry->get_factory (CLAP_PLUGIN_FACTORY_ID));
    CHECK (factory != nullptr);
    testDescriptor();
    testParams();
    testProgressions();
    testPadMap (argv[1]);
    testState();
    testPresets();
    CHECK (checkMoodNotes());
    testNotes();
    testEffects();
    testPhaser();
    testTiming();
    testTuning();
    testExtremes();
    testSleep();
    testStoppedDelay();
    testScreen();
    entry->deinit();
    std::printf ("%d checks, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
