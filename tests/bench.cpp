// Reverie's cost: renders through the CLAP entry for SECONDS at 48 kHz in 128-frame blocks, as Melodee plays it.
// tools/bench.sh runs it under callgrind for instruction counts (exact and repeatable, unlike timings on a computer
// that is not the MPC).
//   bench [worst|pad|typical|silent|paint|motion] [SECONDS]   (REVERIE_BENCH_RATE=44100 for another host rate)
// worst: every voice busy (three Open-voiced lifted chords struck over each other and held: 24 voices), unison 4 + 4,
// LP 24, Motion on every destination, every effect up; pad: the default sound holding one chord; typical: the
// default sound playing its progression, a chord every two seconds, the releases overlapping; silent: no notes;
// paint / motion: the CHORDS / MOTION page painted 25 times a second while the pad plays.
#include "../src/reverie.cpp"

#include <chrono>
#include <string>
#include <vector>

int main (int argc, char** argv)
{
    const std::string mode = argc > 1 ? argv[1] : "worst";
    const double seconds = argc > 2 ? std::atof (argv[2]) : 10.0;
    const double rate = std::getenv ("REVERIE_BENCH_RATE") ? std::atof (std::getenv ("REVERIE_BENCH_RATE")) : 48000.0;
    constexpr uint32_t block = 128;
    static const clap_host host { CLAP_VERSION, nullptr, "bench", "", "", "1", [] (const clap_host*, const char*) -> const void* { return nullptr; },
                                  [] (const clap_host*) {}, [] (const clap_host*) {}, [] (const clap_host*) {} };
    clap_entry.init ("");
    const auto* factory = static_cast<const clap_plugin_factory*> (clap_entry.get_factory (CLAP_PLUGIN_FACTORY_ID));
    const clap_plugin* plugin = factory->create_plugin (factory, &host, "com.hdslave.reverie");
    plugin->init (plugin);
    plugin->activate (plugin, rate, 1, block);
    plugin->start_processing (plugin);

    using namespace reverie::ParamId;
    struct Any { union { clap_event_header header; clap_event_midi midi; clap_event_param_value param; }; };
    std::vector<Any> first;
    auto param = [&] (uint32_t id, double v) { Any a {}; a.param = { { sizeof (clap_event_param_value), 0, CLAP_CORE_EVENT_SPACE_ID, CLAP_EVENT_PARAM_VALUE, 0 }, id, nullptr, -1, -1, -1, -1, v }; first.push_back (a); };
    auto note = [&] (std::vector<Any>& list, uint32_t t, uint8_t s, uint8_t k, uint8_t v) { Any a {}; a.midi.header = { sizeof (clap_event_midi), t, CLAP_CORE_EVENT_SPACE_ID, CLAP_EVENT_MIDI, 0 }; a.midi.data[0] = s, a.midi.data[1] = k, a.midi.data[2] = v; list.push_back (a); };
    if (mode == "worst")
    {
        param (voicing, reverie::Voicing::open), param (osc1Unison, 4), param (osc2Unison, 4), param (filterType, reverie::Filter::lp24);
        param (resonance, 0.5), param (air, 0.4), param (ensMix, 1.0), param (dlyMix, 0.5), param (dlyFeedback, 0.7);
        param (revMix, 0.8), param (shimmer, 0.8), param (revDecay, 1.0), param (strum, 0.0);
        param (xDest1, reverie::Dest::waves), param (xDest2, reverie::Dest::detune), param (yDest1, reverie::Dest::cutoff), param (yDest2, reverie::Dest::width);
        for (int k : { 44, 45, 46 }) note (first, 0, 0x90, (uint8_t) k, 110);
    }
    if (mode == "pad" || mode == "paint" || mode == "motion") note (first, 0, 0x90, 36, 100);

    clap_input_events none { nullptr, [] (const clap_input_events*) { return 0u; }, [] (const clap_input_events*, uint32_t) -> const clap_event_header* { return nullptr; } };
    clap_output_events out { nullptr, [] (const clap_output_events*, const clap_event_header*) { return true; } };
    std::vector<float> l (block), r (block);
    float* channels[2] = { l.data(), r.data() };
    clap_audio_buffer buffer { channels, nullptr, 2, 0, 0 };
    const auto* screen = static_cast<const clap_plugin_melodee_screen*> (plugin->get_extension (plugin, CLAP_EXT_MELODEE_SCREEN));
    if (mode == "motion")
    {
        melodee_screen_input_t nudge { MELODEE_SCREEN_NUDGE, 1, false };
        screen->input (plugin, &nudge);
    }
    std::vector<uint32_t> pixels (320 * 218);
    melodee_screen_canvas_t canvas {};
    canvas.pixels = pixels.data(), canvas.width = 320, canvas.height = 218, canvas.stride = 320;
    const long blocks = (long) (seconds * rate / block);
    const long chordEvery = (long) (2.0 * rate / block);
    const auto t0 = std::chrono::steady_clock::now();
    for (long b = 0; b < 40 + blocks; ++b)
    {
        std::vector<Any> list;
        if (b == 0) list = first;
        if (mode == "typical" && b % chordEvery == 0)
        {
            const uint8_t k = (uint8_t) (36 + (b / chordEvery) % 8), prev = (uint8_t) (36 + (b / chordEvery + 7) % 8);
            note (list, 0, 0x80, prev, 0), note (list, 0, 0x90, k, 100);
        }
        clap_input_events in { &list, [] (const clap_input_events* e) { return (uint32_t) static_cast<std::vector<Any>*> (e->ctx)->size(); },
                               [] (const clap_input_events* e, uint32_t i) -> const clap_event_header* { return &(*static_cast<std::vector<Any>*> (e->ctx))[i].header; } };
        clap_process p {};
        p.frames_count = block;
        p.audio_outputs = &buffer;
        p.audio_outputs_count = 1;
        p.in_events = list.empty() ? &none : &in;
        p.out_events = &out;
        plugin->process (plugin, &p);
        if ((mode == "paint" || mode == "motion") && b >= 40 && (b - 40) % 15 == 0)
            canvas.now_ms = (uint64_t) (b * 1000 / 375), canvas.whole = false, screen->paint (plugin, &canvas);
    }
    const double took = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();
    std::printf ("%-8s %.1f us a block here (%.2f%% of real time)\n", mode.c_str(), took / (double) (40 + blocks) * 1e6,
                 took / ((double) (40 + blocks) * block / rate) * 100.0);
    plugin->stop_processing (plugin);
    plugin->deactivate (plugin);
    plugin->destroy (plugin);
    return 0;
}
