// Renders Reverie through its CLAP entry, as a host plays it, for listening: a playlist of segments, each a fresh
// instance playing chords from the pads, joined into one stereo WAV (16-bit). Prints each segment's peak and RMS.
//   render PLUGIN.clap OUT.wav PLAYLIST [RATE]
// A playlist line (blank lines and # comments skipped), every field optional:
//   preset=velvet-nebula prog=3 voicing=1 key=5 hold=2.5 tail=4 vel=0.8 pads=1,2,3,4,5,6,7,8 set=31:0.6,74:0.9
// pads are 1-16 (pad 1 = A01 = note 36); hold is the seconds a chord is held; tail the seconds after the last; set
// gives parameters by id (plain values). legato=1 strikes the next chord before letting go of the one before.
#include "wav.h"
#include "params.h"

#include <clap/clap.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dlfcn.h>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace
{
const clap_host host { CLAP_VERSION, nullptr, "render", "", "", "1", [] (const clap_host*, const char*) -> const void* { return nullptr; },
                       [] (const clap_host*) {}, [] (const clap_host*) {}, [] (const clap_host*) {} };

struct Segment
{
    std::string preset;
    std::vector<std::pair<uint32_t, double>> params;
    std::vector<int> pads { 1, 2, 3, 4, 5, 6, 7, 8 };
    double hold = 2.0, tail = 4.0, vel = 0.8;
    bool legato = false;
};

struct Any { union { clap_event_header header; clap_event_note note; clap_event_param_value param; }; };
}

int main (int argc, char** argv)
{
    if (argc < 4) { std::fprintf (stderr, "render PLUGIN.clap OUT.wav PLAYLIST [RATE]\n"); return 2; }
    const double rate = argc > 4 ? std::atof (argv[4]) : 48000.0;
    void* lib = dlopen (argv[1], RTLD_NOW | RTLD_LOCAL);
    if (lib == nullptr) { std::fprintf (stderr, "%s\n", dlerror()); return 1; }
    const auto* entry = static_cast<const clap_plugin_entry*> (dlsym (lib, "clap_entry"));
    entry->init (argv[1]);
    const auto* factory = static_cast<const clap_plugin_factory*> (entry->get_factory (CLAP_PLUGIN_FACTORY_ID));

    std::vector<Segment> segments;
    std::ifstream list (argv[3]);
    std::string line;
    while (std::getline (list, line))
    {
        if (line.empty() || line[0] == '#') continue;
        Segment s;
        std::istringstream in (line);
        std::string field;
        while (in >> field)
        {
            const size_t eq = field.find ('=');
            if (eq == std::string::npos) continue;
            const std::string k = field.substr (0, eq), v = field.substr (eq + 1);
            if (k == "preset") s.preset = v;
            else if (k == "prog") // existing playlists select the factory bank
            {
                s.params.push_back ({ reverie::ParamId::publicId (reverie::ParamId::progBank), 1 });
                s.params.push_back ({ reverie::ParamId::progression, std::atof (v.c_str()) });
            }
            else if (k == "original") s.params.push_back ({ reverie::ParamId::publicId (reverie::ParamId::originalProgression), std::atof (v.c_str()) });
            else if (k == "voicing") s.params.push_back ({ reverie::ParamId::voicing, std::atof (v.c_str()) });
            else if (k == "key") s.params.push_back ({ reverie::ParamId::key, std::atof (v.c_str()) });
            else if (k == "hold") s.hold = std::atof (v.c_str());
            else if (k == "tail") s.tail = std::atof (v.c_str());
            else if (k == "vel") s.vel = std::atof (v.c_str());
            else if (k == "legato") s.legato = v == "1";
            else if (k == "pads")
            {
                s.pads.clear();
                std::istringstream p (v);
                std::string n;
                while (std::getline (p, n, ',')) s.pads.push_back (std::atoi (n.c_str()));
            }
            else if (k == "set")
            {
                std::istringstream p (v);
                std::string pair;
                while (std::getline (p, pair, ','))
                {
                    const size_t c = pair.find (':');
                    if (c != std::string::npos) s.params.push_back ({ (uint32_t) std::atoi (pair.substr (0, c).c_str()), std::atof (pair.substr (c + 1).c_str()) });
                }
            }
        }
        segments.push_back (s);
    }

    std::vector<float> L, R;
    constexpr uint32_t block = 128;
    for (size_t si = 0; si < segments.size(); ++si)
    {
        const Segment& s = segments[si];
        const clap_plugin* plugin = factory->create_plugin (factory, &host, "com.hdslave.reverie");
        plugin->init (plugin);
        if (! s.preset.empty())
        {
            const auto* presets = static_cast<const clap_plugin_preset_load*> (plugin->get_extension (plugin, CLAP_EXT_PRESET_LOAD));
            if (! presets->from_location (plugin, CLAP_PRESET_DISCOVERY_LOCATION_PLUGIN, nullptr, s.preset.c_str()))
                std::fprintf (stderr, "no preset %s\n", s.preset.c_str());
        }
        plugin->activate (plugin, rate, 1, block);
        plugin->start_processing (plugin);
        const uint64_t holdFrames = (uint64_t) (s.hold * rate);
        const uint64_t total = holdFrames * s.pads.size() + (uint64_t) (s.tail * rate);
        std::vector<float> l (block), r (block);
        float* ch[2] = { l.data(), r.data() };
        clap_audio_buffer buffer { ch, nullptr, 2, 0, 0 };
        clap_output_events out { nullptr, [] (const clap_output_events*, const clap_event_header*) { return true; } };
        double peak = 0.0, power = 0.0;
        const size_t start = L.size();
        for (uint64_t at = 0; at < total; at += block)
        {
            std::vector<Any> ev;
            auto note = [&] (uint16_t type, int pad, uint32_t t) {
                Any a {};
                a.note = { { sizeof (clap_event_note), t, CLAP_CORE_EVENT_SPACE_ID, type, 0 }, -1, 0, 0, (int16_t) (35 + pad), type == CLAP_EVENT_NOTE_ON ? s.vel : 0.0 };
                ev.push_back (a);
            };
            if (at == 0)
                for (const auto& p : s.params)
                {
                    Any a {};
                    a.param = { { sizeof (clap_event_param_value), 0, CLAP_CORE_EVENT_SPACE_ID, CLAP_EVENT_PARAM_VALUE, 0 }, p.first, nullptr, -1, -1, -1, -1, p.second };
                    ev.push_back (a);
                }
            for (size_t c = 0; c < s.pads.size(); ++c)
            {
                const uint64_t on = c * holdFrames, off = on + holdFrames - (s.legato ? 0 : (uint64_t) (0.05 * rate));
                const uint64_t offAt = s.legato ? on + holdFrames + (uint64_t) (0.02 * rate) : off;
                if (on >= at && on < at + block) note (CLAP_EVENT_NOTE_ON, s.pads[c], (uint32_t) (on - at));
                if (offAt >= at && offAt < at + block) note (CLAP_EVENT_NOTE_OFF, s.pads[c], (uint32_t) (offAt - at));
            }
            std::stable_sort (ev.begin(), ev.end(), [] (const Any& a, const Any& b) { return a.header.time < b.header.time; });
            clap_input_events in { &ev, [] (const clap_input_events* e) { return (uint32_t) static_cast<std::vector<Any>*> (e->ctx)->size(); },
                                   [] (const clap_input_events* e, uint32_t i) -> const clap_event_header* { return &(*static_cast<std::vector<Any>*> (e->ctx))[i].header; } };
            clap_process p {};
            p.frames_count = (uint32_t) std::min<uint64_t> (block, total - at);
            p.audio_outputs = &buffer;
            p.audio_outputs_count = 1;
            p.in_events = &in;
            p.out_events = &out;
            plugin->process (plugin, &p);
            for (uint32_t i = 0; i < p.frames_count; ++i)
            {
                L.push_back (l[i]), R.push_back (r[i]);
                peak = std::max (peak, (double) std::max (std::fabs (l[i]), std::fabs (r[i])));
                power += 0.5 * ((double) l[i] * l[i] + (double) r[i] * r[i]);
            }
        }
        const double seconds = (double) (L.size() - start) / rate;
        std::printf ("segment %2zu %-18s peak %6.1f dBFS  rms %6.1f dBFS  %.1f s\n", si + 1, s.preset.empty() ? "(default)" : s.preset.c_str(),
                     20.0 * std::log10 (std::max (peak, 1e-9)), 10.0 * std::log10 (std::max (power / (double) (L.size() - start), 1e-18)), seconds);
        plugin->stop_processing (plugin);
        plugin->deactivate (plugin);
        plugin->destroy (plugin);
    }
    if (! writeWav (argv[2], L, R, (int) rate)) { std::fprintf (stderr, "cannot write %s\n", argv[2]); return 1; }
    entry->deinit();
    return 0;
}
