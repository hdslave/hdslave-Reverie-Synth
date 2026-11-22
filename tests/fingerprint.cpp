// The sound's fingerprint: renders fixed scenarios through the CLAP entry, as a host plays them, and prints a 64-bit
// hash of every output sample's bits for each. tests/fingerprint.txt holds the hashes the current sound gives, and
// `make test` fails if one changes: work meant to keep the sound (a speed-up, a clean-up) must leave them all as they
// are. A change meant to change the sound updates the file with `make fingerprint-update`.
//   fingerprint PLUGIN.clap [EXPECTED.txt]
// Scenarios: every sound playing a progression, every voicing, Strum, Latch, Chord Glide, each Motion path, other
// rates, and the same scene in blocks of 1, 37 and 4096 frames, which must hash as in blocks of 128. Hashes hold for
// this toolchain and these flags (Melodee's build image, -O3 -mcpu=cortex-a55 -ffp-contract=fast).
#include "../src/params.h"
#include "../src/presets.h"

#include <clap/clap.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dlfcn.h>
#include <map>
#include <string>
#include <vector>

namespace
{
using namespace reverie;
const clap_plugin_factory* factory = nullptr;
const clap_host host { CLAP_VERSION, nullptr, "fingerprint", "", "", "1", [] (const clap_host*, const char*) -> const void* { return nullptr; },
                       [] (const clap_host*) {}, [] (const clap_host*) {}, [] (const clap_host*) {} };

struct Event { uint64_t frame; int kind; int key; double value; uint32_t id = 0; }; // kind 0 on, 1 off, 2 param
Event On (uint64_t f, int key, double vel = 0.8) { return { f, 0, key, vel }; }
Event Off (uint64_t f, int key) { return { f, 1, key, 0.0 }; }
Event P (uint64_t f, uint32_t id, double v) { Event e { f, 2, 0, v }; e.id = id; return e; }

struct Scenario
{
    std::string name, preset;
    std::vector<Event> events;
    double rate = 48000.0, seconds = 6.0;
    uint32_t block = 128;
};

uint64_t fnv (uint64_t h, const void* data, size_t n)
{
    const auto* p = static_cast<const uint8_t*> (data);
    for (size_t i = 0; i < n; ++i) h = (h ^ p[i]) * 0x100000001b3ull;
    return h;
}

uint64_t render (const Scenario& s)
{
    const clap_plugin* plugin = factory->create_plugin (factory, &host, "com.hdslave.reverie");
    plugin->init (plugin);
    if (! s.preset.empty())
    {
        const auto* presets = static_cast<const clap_plugin_preset_load*> (plugin->get_extension (plugin, CLAP_EXT_PRESET_LOAD));
        if (! presets->from_location (plugin, CLAP_PRESET_DISCOVERY_LOCATION_PLUGIN, nullptr, s.preset.c_str()))
            std::printf ("no preset %s\n", s.preset.c_str());
    }
    plugin->activate (plugin, s.rate, 1, 4096);
    plugin->start_processing (plugin);
    struct Any { union { clap_event_header header; clap_event_note note; clap_event_param_value param; }; };
    std::vector<Any> list;
    clap_input_events in { &list, [] (const clap_input_events* e) { return (uint32_t) static_cast<std::vector<Any>*> (e->ctx)->size(); },
                           [] (const clap_input_events* e, uint32_t i) -> const clap_event_header* { return &(*static_cast<std::vector<Any>*> (e->ctx))[i].header; } };
    clap_output_events out { nullptr, [] (const clap_output_events*, const clap_event_header*) { return true; } };
    const uint64_t frames = (uint64_t) (s.seconds * s.rate);
    std::vector<float> l (s.block), r (s.block);
    uint64_t h = 0xcbf29ce484222325ull;
    for (uint64_t at = 0; at < frames; at += s.block)
    {
        const uint32_t n = (uint32_t) std::min<uint64_t> (s.block, frames - at);
        list.clear();
        for (const auto& e : s.events)
            if (e.frame >= at && e.frame < at + n)
            {
                Any a {};
                const uint32_t t = (uint32_t) (e.frame - at);
                if (e.kind == 2)
                    a.param = { { sizeof (clap_event_param_value), t, CLAP_CORE_EVENT_SPACE_ID, CLAP_EVENT_PARAM_VALUE, 0 }, e.id, nullptr, -1, -1, -1, -1, e.value };
                else
                    a.note = { { sizeof (clap_event_note), t, CLAP_CORE_EVENT_SPACE_ID, (uint16_t) (e.kind == 0 ? CLAP_EVENT_NOTE_ON : CLAP_EVENT_NOTE_OFF), 0 },
                               -1, 0, 0, (int16_t) e.key, e.value };
                list.push_back (a);
            }
        std::stable_sort (list.begin(), list.end(), [] (const Any& a, const Any& b) { return a.header.time < b.header.time; });
        float* ch[2] = { l.data(), r.data() };
        clap_audio_buffer buffer { ch, nullptr, 2, 0, 0 };
        clap_process p {};
        p.frames_count = n;
        p.audio_outputs = &buffer;
        p.audio_outputs_count = 1;
        p.in_events = &in;
        p.out_events = &out;
        plugin->process (plugin, &p);
        for (uint32_t i = 0; i < n; ++i) // frame by frame, so the hash does not depend on the block
        {
            h = fnv (h, &l[i], 4);
            h = fnv (h, &r[i], 4);
        }
    }
    plugin->stop_processing (plugin);
    plugin->deactivate (plugin);
    plugin->destroy (plugin);
    return h;
}

// Four chords of a progression, a second and a half each.
std::vector<Event> phrase (int first = 36)
{
    std::vector<Event> e;
    for (int c = 0; c < 4; ++c) e.push_back (On (72000 * (uint64_t) c / 2 + 64, first + c)), e.push_back (Off (72000 * (uint64_t) (c + 1) / 2 - 2400, first + c));
    return e;
}

std::vector<Scenario> scenarios()
{
    using namespace ParamId;
    std::vector<Scenario> list;
    for (const auto& p : presetTable)
        list.push_back ({ std::string ("sound-") + p.key, p.key, phrase() });
    for (int v = 0; v < Voicing::count; ++v)
    {
        auto e = phrase (44);
        e.insert (e.begin(), P (0, voicing, v));
        e.insert (e.begin(), P (0, progression, 7 + 3 * v));
        list.push_back ({ "voicing-" + std::to_string (v), "", e });
    }
    for (int m = 1; m < Motion::count; ++m)
    {
        auto e = phrase();
        e.insert (e.begin(), { P (0, motionShape, m), P (0, motionDepth, 0.9), P (0, motionRate, 0.7), P (0, motionSpread, 1.0), P (0, xDest2, Dest::pitch), P (0, yDest2, Dest::width) });
        list.push_back ({ "motion-" + std::to_string (m), "", e });
    }
    list.push_back ({ "strum-humanize", "", { P (0, strum, 0.8), P (0, humanize, 1.0), On (0, 36), Off (96000, 36), On (100000, 41, 0.4) } });
    list.push_back ({ "latch", "", { P (0, latch, 1), On (0, 36), Off (1000, 36), On (72000, 37), Off (73000, 37), P (200000, latch, 0) } });
    list.push_back ({ "glide", "", { P (0, glide, 0.5), On (0, 36), On (60000, 37), Off (61000, 36), On (120000, 42), Off (121000, 37), Off (200000, 42) } });
    list.push_back ({ "retrig-sync", "", { P (0, motionRetrig, 1), P (0, motionSync, 1), P (0, motionDiv, 6), On (0, 38), Off (200000, 38) } });
    list.push_back ({ "keys-octaves", "", { P (0, key, 7), P (0, octave, 1), On (0, 40), On (60000, 56), On (120000, 23) } });
    list.push_back ({ "lp24-bp-hp", "", { P (0, filterType, 1), On (0, 36), P (60000, filterType, 2), P (120000, filterType, 3), Off (200000, 36) } });
    Scenario rate44 { "rate-44100", "", phrase() };
    rate44.rate = 44100.0;
    list.push_back (rate44);
    Scenario rate96 { "rate-96000", "", phrase() };
    rate96.rate = 96000.0, rate96.seconds = 3.0;
    list.push_back (rate96);
    // the same scene in other blocks must hash as in blocks of 128 (fingerprint.txt has one line for all four)
    const std::vector<Event> scene = { P (0, motionShape, Motion::star), P (0, strum, 0.4), On (100, 36), On (7777, 44, 0.6), P (9000, cutoff, 0.4),
                                       Off (30000, 36), On (30001, 37), P (31000, glide, 0.4), On (40000, 38), Off (50000, 44), Off (60000, 37), Off (61000, 38) };
    for (uint32_t block : { 128u, 1u, 37u, 4096u })
    {
        Scenario s { "blocks-" + std::to_string (block), "", scene };
        s.block = block, s.seconds = 3.0;
        list.push_back (s);
    }
    return list;
}
} // namespace

int main (int argc, char** argv)
{
    if (argc < 2) { std::printf ("fingerprint PLUGIN.clap [EXPECTED.txt]\n"); return 2; }
    void* lib = dlopen (argv[1], RTLD_NOW | RTLD_LOCAL);
    if (lib == nullptr) { std::printf ("%s\n", dlerror()); return 1; }
    const auto* entry = static_cast<const clap_plugin_entry*> (dlsym (lib, "clap_entry"));
    entry->init (argv[1]);
    factory = static_cast<const clap_plugin_factory*> (entry->get_factory (CLAP_PLUGIN_FACTORY_ID));
    std::map<std::string, std::string> expected;
    if (argc > 2)
        if (FILE* f = std::fopen (argv[2], "r"))
        {
            char name[128], hash[32];
            while (std::fscanf (f, "%127s %31s", name, hash) == 2) expected[name] = hash;
            std::fclose (f);
        }
    int failures = 0;
    std::string blocks128;
    const char* only = std::getenv ("FINGERPRINT_ONLY"); // a part of the names to render (debugging)
    for (const auto& s : scenarios())
    {
        if (only != nullptr && s.name.find (only) == std::string::npos) continue;
        char hash[32];
        std::snprintf (hash, sizeof (hash), "%016llx", (unsigned long long) render (s));
        if (s.name.rfind ("blocks-", 0) == 0)
        {
            if (s.name == "blocks-128") blocks128 = hash;
            else if (blocks128 != hash) { std::fprintf (stderr, "FAIL %s hashes %s, blocks of 128 %s\n", s.name.c_str(), hash, blocks128.c_str()); ++failures; }
            if (s.name != "blocks-128") continue;
        }
        std::printf ("%s %s\n", s.name.c_str(), hash);
        if (argc > 2 && expected[s.name] != hash)
        {
            std::fprintf (stderr, "FAIL %s: %s, expected %s\n", s.name.c_str(), hash, expected[s.name].empty() ? "(none)" : expected[s.name].c_str());
            ++failures;
        }
    }
    entry->deinit();
    if (failures) std::fprintf (stderr, "%d fingerprint failures\n", failures);
    return failures == 0 ? 0 : 1;
}
