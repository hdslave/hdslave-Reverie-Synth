// Black-box CLAP screen contract test. Runs on the native preview and the ARMv7 device build.
#include "png.h"
#include "sixteen_screen.h"
#include "presets.h"
#include "engine.h"
#include "mood_notes.h"

#include <clap/clap.h>
#include <dlfcn.h>

#include <array>
#include <cassert>
#include <cmath>
#include <cstring>
#include <string>
#include <utility>

namespace
{
using namespace reverie::ParamId;
constexpr int canvasWidth = SIXTEEN_SCREEN_WIDTH, height = SIXTEEN_SCREEN_HEIGHT, stride = canvasWidth + 12;
constexpr uint32_t guard = 0x12345678;
int wakes = 0;

struct Fixture
{
    clap_host host {};
    const clap_plugin* plugin = nullptr;
    const sixteen_plugin_screen_t* screen = nullptr;
    const clap_plugin_params_t* params = nullptr;
    clap_event_note_t note {};
    bool sendNote = false;
    clap_input_events_t in {};
    clap_audio_buffer_t audio {};
    clap_process_t process {};
    float left[256] {}, right[256] {};
    float* channels[2] = { left, right };
    std::vector<uint32_t> pixels = std::vector<uint32_t> (stride * height, guard);
    sixteen_screen_surface_t surface { pixels.data(), canvasWidth, height, stride, 0 };

    void attach (const clap_plugin_factory_t& factory)
    {
        host.clap_version = CLAP_VERSION;
        host.name = "Screen test";
        host.get_extension = [] (const clap_host*, const char*) -> const void* { return nullptr; };
        host.request_process = [] (const clap_host*) { ++wakes; };
        plugin = factory.create_plugin (&factory, &host, factory.get_plugin_descriptor (&factory, 0)->id);
        assert (plugin && plugin->init (plugin));
        screen = static_cast<const sixteen_plugin_screen_t*> (plugin->get_extension (plugin, SIXTEEN_EXT_SCREEN));
        params = static_cast<const clap_plugin_params_t*> (plugin->get_extension (plugin, CLAP_EXT_PARAMS));
        assert (screen && params && plugin->activate (plugin, 48000.0, 1, 256) && plugin->start_processing (plugin));

        in.ctx = this;
        in.size = [] (const clap_input_events_t* e) -> uint32_t { return static_cast<Fixture*> (e->ctx)->sendNote ? 1 : 0; };
        in.get = [] (const clap_input_events_t* e, uint32_t) -> const clap_event_header_t* { return &static_cast<Fixture*> (e->ctx)->note.header; };
        audio.channel_count = 2;
        audio.data32 = channels;
        process.audio_outputs_count = 1;
        process.audio_outputs = &audio;
        process.frames_count = 256;
        process.in_events = &in;
    }

    void stop()
    {
        plugin->stop_processing (plugin);
        plugin->deactivate (plugin);
        plugin->destroy (plugin);
    }

    double value (uint32_t id)
    {
        double v = 0.0;
        assert (params->get_value (plugin, id, &v));
        return v;
    }

    void setValue (uint32_t id, double value)
    {
        clap_event_param_value event { { sizeof (event), 0, CLAP_CORE_EVENT_SPACE_ID, CLAP_EVENT_PARAM_VALUE, 0 },
                                       id, nullptr, -1, -1, -1, -1, value };
        clap_input_events changes { &event,
            [] (const clap_input_events*) { return 1u; },
            [] (const clap_input_events* e, uint32_t) { return &static_cast<const clap_event_param_value*> (e->ctx)->header; } };
        params->flush (plugin, &changes, nullptr);
    }

    std::array<double, count> values()
    {
        std::array<double, count> result {};
        for (uint32_t id = 0; id < count; ++id) result[id] = value (reverie::paramTable[id].id);
        return result;
    }

    bool touch (int phase, int x, int y) { return screen->touch (plugin, phase, x, y); }

    bool frame (bool forced = false)
    {
        surface.now_ms += 45;
        return screen->render (plugin, &surface, forced);
    }

    void capture (const char* folder, const char* name)
    {
        if (! folder) return;
        const std::string path = std::string (folder) + "/" + name + ".png";
        assert (writePng (path.c_str(), pixels.data(), canvasWidth, height, stride));
    }

    float block (int event = -1, int keyNumber = 36, int32_t noteId = 7, double vel = 0.8)
    {
        sendNote = event != -1;
        if (sendNote)
        {
            note = {};
            note.header = { sizeof (note), 0, CLAP_CORE_EVENT_SPACE_ID, uint16_t (event), 0 };
            note.note_id = noteId;
            note.key = static_cast<int16_t> (keyNumber);
            note.velocity = vel;
        }
        assert (plugin->process (plugin, &process) != CLAP_PROCESS_ERROR);
        float peak = 0.0f;
        for (int i = 0; i < 256; ++i)
        {
            assert (std::isfinite (left[i]) && std::isfinite (right[i]));
            peak = std::max (peak, std::max (std::abs (left[i]), std::abs (right[i])));
        }
        return peak;
    }

    int padSample (int slot) const { return (135 + (3 - slot / 4) * 33) * stride + 56 + (slot % 4) * 94; }
    uint32_t padPixel (int slot) const { return pixels[padSample (slot)]; }
    uint32_t heldPads (const std::vector<uint32_t>& idle) const
    {
        uint32_t held = 0;
        for (int slot = 0; slot < 16; ++slot)
            if (padPixel (slot) != idle[padSample (slot)]) held |= 1u << slot;
        return held;
    }
};
// Read the actual CLAP provider as the host does, including the display name and stable load key.
void checkFactoryList (const clap_plugin_entry_t* entry)
{
    const auto* factory = static_cast<const clap_preset_discovery_factory*> (entry->get_factory (CLAP_PRESET_DISCOVERY_FACTORY_ID));
    assert (factory && factory->count (factory) == 1);
    clap_preset_discovery_indexer indexer {};
    indexer.clap_version = CLAP_VERSION;
    indexer.name = "Screen test";
    indexer.declare_location = [] (const clap_preset_discovery_indexer*, const clap_preset_discovery_location* location) {
        return location && location->kind == CLAP_PRESET_DISCOVERY_LOCATION_PLUGIN;
    };
    const auto* provider = factory->create (factory, &indexer, factory->get_descriptor (factory, 0)->id);
    assert (provider && provider->init (provider));
    std::vector<std::pair<std::string, std::string>> sounds;
    clap_preset_discovery_metadata_receiver receiver {};
    receiver.receiver_data = &sounds;
    receiver.begin_preset = [] (const clap_preset_discovery_metadata_receiver* r, const char* name, const char* key) {
        static_cast<decltype (sounds)*> (r->receiver_data)->emplace_back (name, key);
        return true;
    };
    receiver.add_plugin_id = [] (const clap_preset_discovery_metadata_receiver*, const clap_universal_plugin_id*) {};
    receiver.set_flags = [] (const clap_preset_discovery_metadata_receiver*, uint32_t) {};
    receiver.add_creator = [] (const clap_preset_discovery_metadata_receiver*, const char*) {};
    assert (provider->get_metadata (provider, CLAP_PRESET_DISCOVERY_LOCATION_PLUGIN, nullptr, &receiver));
    provider->destroy (provider);
    const std::pair<std::string, std::string> additions[] = {
        { "Undertow Wah", "undertow-wah" }, { "Opal Rain", "opal-rain" }, { "Lantern Ferry", "lantern-ferry" },
        { "Cinder Cello", "cinder-cello" }, { "Cloud Loom", "cloud-loom" }
    };
    assert (sounds.size() == reverie::presetCount && sounds.size() >= 25);
    assert (std::equal (std::begin (additions), std::end (additions), sounds.begin() + 20));
}

void checkProgressionRecall (Fixture& f)
{
    using namespace reverie;
    assert (f.value (99) == 0 && f.value (100) == 0);
    clap_param_info info {};
    assert (f.params->get_info (f.plugin, originalProgression, &info) && info.id == 99 && info.max_value == 45);
    char name[32];
    for (int i = 0; i < progressionCount; ++i)
    {
        assert (f.params->value_to_text (f.plugin, 99, i, name, sizeof (name)));
        assert (std::strcmp (name, progressionTable[i].name) == 0);
    }
    const auto* state = static_cast<const clap_plugin_state*> (f.plugin->get_extension (f.plugin, CLAP_EXT_STATE));
    struct OldState {
        char tag[4];
        uint32_t version, count;
        struct { uint32_t id; float value; } pairs[3];
    } old { { 'R', 'V', 'R', 'E' }, 2, 3, {{ 0, 207 }, { 97, 18 }, { 98, -3 }} };
    static_assert (sizeof (old) == 36);
    size_t at = 0;
    struct Reader { const OldState& old; size_t& at; } reader { old, at };
    clap_istream stream { &reader, [] (const clap_istream* stream, void* data, uint64_t n) -> int64_t {
        auto& r = *static_cast<Reader*> (stream->ctx);
        n = std::min<uint64_t> (n, sizeof (r.old) - r.at);
        std::memcpy (data, reinterpret_cast<const uint8_t*> (&r.old) + r.at, n);
        r.at += n;
        return (int64_t) n;
    } };
    assert (state && state->load (f.plugin, &stream));
    assert (f.value (100) == 1 && f.value (0) == 207 && f.value (97) == 18 && f.value (98) == -3);
    assert (f.params->value_to_text (f.plugin, 0, 207, name, sizeof (name)) && std::strcmp (name, "Onward") == 0);
    old.version = 1;
    old.count = 1;
    old.pairs[0].value = 45;
    at = 0;
    assert (state->load (f.plugin, &stream));
    assert (f.value (100) == 0 && f.value (99) == 45 && f.value (97) == 0);
    assert (f.params->value_to_text (f.plugin, 99, 45, name, sizeof (name)) && std::strcmp (name, "Moonstone") == 0);
    f.setValue (99, 0);
    f.setValue (0, 0);

    uint64_t hash = 14695981039346656037ull;
    auto add = [&] (int value) { hash = (hash ^ (uint8_t) value) * 1099511628211ull; };
    float values[paramCount] {};
    for (int p = 0; p < progressionCount; ++p)
    {
        values[originalProgression] = (float) p;
        for (int style = 0; style < Voicing::count; ++style)
            for (int tonic = 0; tonic < 12; ++tonic)
                for (int pad = 0; pad < padSlots; ++pad)
                {
                    const auto c = performanceChord (selectedProgression (values), style, tonic, pad, 0, 0);
                    add (c.count | (c.hasBass ? 0x80 : 0));
                    for (int i = 0; i < c.count; ++i) add (c.note[i]);
                }
    }
    assert (hash == 0x65792b2a421b52ddull); // 52,992 original desktop pad voicings
}

void checkUndertow (Fixture& f, const char* out)
{
    const auto* loader = static_cast<const clap_plugin_preset_load*> (f.plugin->get_extension (f.plugin, CLAP_EXT_PRESET_LOAD));
    assert (loader);
    auto load = [&] (const char* key) {
        return loader->from_location (f.plugin, CLAP_PRESET_DISCOVERY_LOCATION_PLUGIN, nullptr, key);
    };
    auto openSounds = [&] { assert (f.touch (SIXTEEN_TOUCH_DOWN, 638, 21)); };
    f.setValue (97, 2);
    f.setValue (98, -3);
    f.setValue (strum, 0.45);
    const auto original = f.values();
    assert (load ("undertow-wah"));
    const auto expected = f.values();
    assert (expected[mood] == 2 && expected[moodShift] == -3);
    assert (std::equal (original.begin(), original.begin() + chordSection, expected.begin()));
    assert (expected[phaserMix] == 0.78f && expected[phaserRate] == 0.0f && expected[phaserEnv] == 0.69f);
    openSounds();
    assert (f.frame());
    f.capture (out, "sounds-undertow");
    assert (f.screen->selector_turn (f.plugin, -1) && f.screen->selector_push (f.plugin));
    assert (f.values() != expected);
    // The host load placed focus on Undertow, so one step back lands on Satin Keys.
    const auto preceding = f.values();
    assert (load ("satin-keys") && f.values() == preceding);
    openSounds();
    assert (f.screen->selector_turn (f.plugin, 1) && f.screen->selector_push (f.plugin));
    assert (f.values() == expected);
    assert (load ("reverie"));
    openSounds();
    assert (f.touch (SIXTEEN_TOUCH_DOWN, 86, 262)); // first tile in the fifth row
    assert (f.values() == expected);
    assert (load ("reverie"));
    f.setValue (97, 0);
    f.setValue (98, 0);
}

void checkUndertowAudio (const clap_plugin_factory_t& factory)
{
    Fixture host, touch;
    host.attach (factory);
    touch.attach (factory);
    const auto* loader = static_cast<const clap_plugin_preset_load*> (host.plugin->get_extension (host.plugin, CLAP_EXT_PRESET_LOAD));
    assert (loader && loader->from_location (host.plugin, CLAP_PRESET_DISCOVERY_LOCATION_PLUGIN, nullptr, "undertow-wah"));
    assert (touch.touch (SIXTEEN_TOUCH_DOWN, 638, 21) && touch.touch (SIXTEEN_TOUCH_DOWN, 86, 262));
    float peak = 0.0f;
    for (int b = 0; b < 80; ++b)
    {
        const int event = b == 0 ? CLAP_EVENT_NOTE_ON : -1;
        peak = std::max (peak, host.block (event, 62));
        touch.block (event, 62);
        assert (std::equal (std::begin (host.left), std::end (host.left), touch.left));
        assert (std::equal (std::begin (host.right), std::end (host.right), touch.right));
    }
    assert (peak > 0.01f);
    touch.setValue (phaserMix, 0.0);
    float difference = 0.0f;
    for (int b = 0; b < 32; ++b)
    {
        host.block();
        touch.block();
        for (int i = 0; i < 256; ++i) difference = std::max (difference, std::abs (host.left[i] - touch.left[i]));
    }
    assert (difference > 0.001f);
    touch.stop();
    host.stop();
}
} // namespace

int main (int argc, char** argv)
{
    assert (argc >= 2);
    void* dylib = dlopen (argv[1], RTLD_NOW | RTLD_LOCAL);
    if (dylib == nullptr) { std::fprintf (stderr, "%s\n", dlerror()); return 1; }
    const auto* entry = static_cast<const clap_plugin_entry_t*> (dlsym (dylib, "clap_entry"));
    assert (entry && entry->init (argv[1]));
    const auto* factory = static_cast<const clap_plugin_factory_t*> (entry->get_factory (CLAP_PLUGIN_FACTORY_ID));
    assert (factory);
    checkFactoryList (entry);
    checkUndertowAudio (*factory);
    Fixture f;
    f.attach (*factory);
    checkProgressionRecall (f);

    const char* out = argc > 2 ? argv[2] : nullptr;
    const auto* screen = f.screen;
    const auto* plugin = f.plugin;
    assert (f.frame (true));
    for (int y = 0; y < height; ++y)
    {
        for (int x = 0; x < canvasWidth; ++x) assert ((f.pixels[y * stride + x] >> 24) == 255);
        for (int x = canvasWidth; x < stride; ++x) assert (f.pixels[y * stride + x] == guard);
    }
    assert (! f.frame());
    const auto first = f.pixels;
    assert (f.frame (true) && f.pixels == first);
    f.capture (out, "chords");

    // The Mood encoder bank, screen pads and physical chromatic notes must agree.
    assert (f.touch (SIXTEEN_TOUCH_DOWN, 367, 97));
    uint32_t moodIds[6];
    char bankName[SIXTEEN_TAB_NAME_SIZE] {};
    assert (screen->knobs (plugin, moodIds, bankName) && moodIds[0] == 97 && moodIds[1] == 98);
    f.setValue (moodIds[0], 2);
    f.setValue (moodIds[1], -3);
    assert (f.touch (SIXTEEN_TOUCH_DOWN, 240, 97));
    uint32_t progIds[6];
    assert (screen->knobs (plugin, progIds, bankName));
    assert (progIds[0] == SIXTEEN_INVALID_PARAM_ID && progIds[1] == SIXTEEN_INVALID_PARAM_ID && progIds[2] == SIXTEEN_INVALID_PARAM_ID);
    assert (progIds[3] == octave && progIds[4] == strum && progIds[5] == SIXTEEN_INVALID_PARAM_ID);
    assert (f.touch (SIXTEEN_TOUCH_DOWN, 367, 97));
    assert (f.frame (true));
    const auto moodIdle = f.pixels;
    f.capture (out, "mood");
    for (int slot : { 0, 8, 15 })
    {
        const int x = 52 + 94 * (slot % 4), y = 132 + 33 * (3 - slot / 4);
        assert (f.touch (SIXTEEN_TOUCH_DOWN, x, y));
        f.block();
        f.frame();
        assert (f.heldPads (moodIdle) == (1u << slot));
        assert (f.touch (SIXTEEN_TOUCH_UP, x, y));
        f.block();
        f.block (CLAP_EVENT_NOTE_ON, 60 + slot);
        f.frame();
        assert (f.heldPads (moodIdle) == (1u << slot));
        f.block (CLAP_EVENT_NOTE_OFF, 60 + slot);
        f.frame();
        assert (f.heldPads (moodIdle) == 0u);
    }
    f.setValue (moodIds[0], 0);
    f.setValue (moodIds[1], 0);
    assert (f.touch (SIXTEEN_TOUCH_DOWN, 240, 97)); // progression bank

    assert (screen->knobs (plugin, progIds, bankName));
    assert (progIds[0] == 99 && progIds[1] == voicing && progIds[2] == key && progIds[5] == 100);
    f.setValue (progIds[5], 1);
    assert (screen->knobs (plugin, progIds, bankName) && progIds[0] == 0);
    f.setValue (progIds[5], 0);

    // Desktop patch strip. The old button location is scenery. Fast arrow taps,
    // external host loads, browser focus and cancelling all follow the same factory order.
    assert (! f.touch (SIXTEEN_TOUCH_DOWN, 350, 30));
    auto expectSound = [&] (int index) {
        float expected[reverie::paramCount] {};
        reverie::presetValuesOf (reverie::presetTable[index], expected);
        for (uint32_t i = chordSection; i < mood; ++i) assert (std::abs (f.value (i) - expected[i]) < 1.0e-6);
    };
    expectSound (0);
    for (int i = 1; i <= reverie::presetCount; ++i)
    {
        assert (f.touch (SIXTEEN_TOUCH_DOWN, 754, 21));
        expectSound (i % reverie::presetCount);
    }
    assert (f.touch (SIXTEEN_TOUCH_DOWN, 527, 21));
    expectSound (reverie::presetCount - 1);
    const auto* load = static_cast<const clap_plugin_preset_load_t*> (plugin->get_extension (plugin, CLAP_EXT_PRESET_LOAD));
    assert (load && load->from_location (plugin, CLAP_PRESET_DISCOVERY_LOCATION_PLUGIN, nullptr, "glass-cathedral"));
    assert (f.touch (SIXTEEN_TOUCH_DOWN, 638, 21));
    assert (screen->selector_turn (plugin, 1));
    assert (f.touch (SIXTEEN_TOUCH_DOWN, 638, 21));
    expectSound (2);
    assert (f.touch (SIXTEEN_TOUCH_DOWN, 754, 21));
    expectSound (3);
    assert (load->from_location (plugin, CLAP_PRESET_DISCOVERY_LOCATION_PLUGIN, nullptr, "reverie"));
    f.block();
    plugin->reset (plugin);

    // Every parameter belongs to exactly one bank. Each page keeps the bank selected last.
    constexpr int bankCounts[] = { 3, 4, 4, 2, 2, 3 };
    constexpr const char* pageNames[] = { "chords", "motion", "osc", "filter", "amp", "fx" };
    std::array<int, count> uses {};
    for (int p = 0; p < 6; ++p)
    {
        if (p > 0)
        {
            assert (f.touch (SIXTEEN_TOUCH_DOWN, 550 + 87 * (p / 2), 60));
            assert (f.touch (SIXTEEN_TOUCH_DOWN, p % 2 ? 408 : 16, 97));
        }
        assert (f.frame() || p == 0);
        f.capture (out, pageNames[p]);
        for (int b = 0; b < bankCounts[p]; ++b)
        {
            uint32_t ids[6];
            char label[SIXTEEN_TAB_NAME_SIZE] {};
            assert (screen->knobs (plugin, ids, label));
            assert (std::strstr (label, " / ") != nullptr);
            for (auto id : ids)
                if (id != SIXTEEN_INVALID_PARAM_ID) { const int row = reverie::paramIndex (id); assert (row >= 0); ++uses[row]; }
            if (p == 0 && b == 0)
            {
                const uint32_t expected[] = { 99, voicing, key, octave, strum, 100 };
                assert (std::equal (std::begin (ids), std::end (ids), std::begin (expected)));
            }
            assert (screen->selector_turn (plugin, 1));
        }
    }
    assert (uses[progression] == 0); // Shares the first encoder with Original Prog when Factory is selected.
    for (uint32_t i = 1; i < count; ++i) assert (uses[i] == 1);
    assert (screen->selector_push (plugin)); // FX → CHORDS

    // Touch each chord, then release outside the grid. The selected pad lights, and audio stays finite.
    assert (f.frame());
    for (int slot = 0; slot < 16; ++slot)
    {
        plugin->reset (plugin);
        f.frame();
        const auto idle = f.padPixel (slot);
        const int x = 52 + 94 * (slot % 4), y = 132 + 33 * (3 - slot / 4);
        assert (f.touch (SIXTEEN_TOUCH_DOWN, x, y));
        float peak = 0.0f;
        for (int i = 0; i < 10; ++i) peak = std::max (peak, f.block());
        assert (peak > 1.0e-4f && f.frame() && f.padPixel (slot) != idle);
        if (slot == 5) f.capture (out, "chord-held");
        assert (f.touch (SIXTEEN_TOUCH_UP, 779, 284));
        f.block();
        assert (f.frame() && f.padPixel (slot) == idle);
    }

    // Releasing the screen pad must leave the same chord held by MIDI alone.
    plugin->reset (plugin);
    f.block (CLAP_EVENT_NOTE_ON);
    assert (f.touch (SIXTEEN_TOUCH_DOWN, 52, 231));
    f.block();
    assert (f.touch (SIXTEEN_TOUCH_UP, 52, 231));
    f.block();
    f.frame();
    assert (f.padPixel (0) != first[f.padSample (0)]);
    f.block (CLAP_EVENT_NOTE_OFF);
    f.frame();
    assert (f.padPixel (0) == first[f.padSample (0)]);

    // A tap between blocks is played once. The close callback releases a hold.
    plugin->reset (plugin);
    assert (f.touch (SIXTEEN_TOUCH_DOWN, 52, 231) && f.touch (SIXTEEN_TOUCH_UP, 52, 231));
    f.block();
    f.frame();
    assert (f.padPixel (0) != first[f.padSample (0)]);
    f.block();
    f.frame();
    assert (f.padPixel (0) == first[f.padSample (0)]);
    assert (f.touch (SIXTEEN_TOUCH_DOWN, 52, 231));
    f.block();
    screen->on_close (plugin);
    f.block();
    f.frame();
    assert (f.padPixel (0) == first[f.padSample (0)]);

    // The freesp16 host stops rendering when leaving the plugin page. In that
    // case release the audition even if the touchscreen never sent touch-up.
    assert (f.touch (SIXTEEN_TOUCH_DOWN, 52, 231));
    for (int i = 0; i < 200; ++i) f.block();
    f.frame();
    assert (f.padPixel (0) == first[f.padSample (0)]);
    screen->on_close (plugin);

    // Drag outside the Motion field clamps the two existing bipolar parameters.
    assert (f.touch (SIXTEEN_TOUCH_DOWN, 405, 97));
    assert (f.touch (SIXTEEN_TOUCH_DOWN, 472, 188));
    assert (f.value (motionX) == 0.0 && f.value (motionY) == 0.0);
    assert (f.touch (SIXTEEN_TOUCH_MOVE, 500, 160));
    assert (f.value (motionX) == 0.5 && f.value (motionY) == 0.5);
    assert (f.touch (SIXTEEN_TOUCH_MOVE, 779, 284));
    assert (f.value (motionX) == 1.0 && f.value (motionY) == -1.0);
    assert (f.touch (SIXTEEN_TOUCH_UP, 779, 284));
    assert (! f.touch (SIXTEEN_TOUCH_MOVE, 20, 140));
    assert (f.value (motionX) == 1.0 && f.value (motionY) == -1.0);
    assert (wakes >= 36);

    const double before = f.value (osc1Wave), prog = f.value (progression), tonic = f.value (key);
    assert (f.touch (SIXTEEN_TOUCH_DOWN, 638, 21));
    assert (f.frame());
    f.capture (out, "sounds");
    assert (f.touch (SIXTEEN_TOUCH_DOWN, 242, 244)); // Ethereal Vox
    assert (f.value (osc1Wave) != before && f.value (progression) == prog && f.value (key) == tonic);
    assert (f.frame());
    assert (f.touch (SIXTEEN_TOUCH_DOWN, 638, 21));
    assert (screen->selector_turn (plugin, -16));
    assert (screen->selector_push (plugin)); // back to Reverie's default sound
    assert (f.value (osc1Wave) == before);
    checkUndertow (f, out);
    // Every tile in the new bottom row selects the same sound as its factory key.
    for (int col = 0; col < 5; ++col)
    {
        assert (f.touch (SIXTEEN_TOUCH_DOWN, 638, 21));
        assert (f.touch (SIXTEEN_TOUCH_DOWN, 86 + col * 152, 262));
        expectSound (20 + col);
    }
    assert (load->from_location (plugin, CLAP_PRESET_DISCOVERY_LOCATION_PLUGIN, nullptr, "reverie"));
    // freesp16's unshifted chromatic SCALE mode sends 60–75. Legacy bank A
    // still sends 36–51. Both ranges address the same pads in the same order.
    assert (f.touch (SIXTEEN_TOUCH_DOWN, 16, 97));
    bool bothRangesMatch = true;
    for (int base : { 60, 36 })
        for (int slot = 0; slot < 16; ++slot)
        {
            plugin->reset (plugin);
            f.block (CLAP_EVENT_NOTE_ON, base + slot);
            f.frame();
            bothRangesMatch &= f.heldPads (first) == (1u << slot);
            f.block (CLAP_EVENT_NOTE_OFF, base + slot);
            f.frame();
            bothRangesMatch &= f.heldPads (first) == 0u;
        }

    // A sound change releases a held chord even if no note-off reaches CLAP.
    plugin->reset (plugin);
    f.block (CLAP_EVENT_NOTE_ON, 36);
    f.frame();
    assert (f.heldPads (first) == 1u);
    assert (f.touch (SIXTEEN_TOUCH_DOWN, 638, 21));
    assert (f.touch (SIXTEEN_TOUCH_DOWN, 242, 244)); // Ethereal Vox
    f.block();
    f.frame();
    const bool oldChordReleased = f.heldPads (first) == 0u;
    f.block (CLAP_EVENT_NOTE_OFF, 36); // the physical release can arrive after the sound changed
    f.block (CLAP_EVENT_NOTE_ON, 37);
    f.frame();
    assert (f.heldPads (first) == 2u);
    f.block (CLAP_EVENT_NOTE_OFF, 37);
    f.frame();
    assert (f.heldPads (first) == 0u);

    // A late off or choke for the first strike must not end a retrigger of the same key.
    bool retriggerKept = true;
    for (int staleEvent : { CLAP_EVENT_NOTE_OFF, CLAP_EVENT_NOTE_CHOKE })
    {
        plugin->reset (plugin);
        f.block (CLAP_EVENT_NOTE_ON, 60, 301);
        f.block (CLAP_EVENT_NOTE_ON, 60, 302);
        f.block (staleEvent, 60, 301);
        f.frame();
        retriggerKept &= f.heldPads (first) == 1u;
        f.block (CLAP_EVENT_NOTE_OFF, 60, 302);
        f.frame();
        assert (f.heldPads (first) == 0u);
    }

    // Change sounds mid-waveform. The first new sample must meet the old one.
    Fixture transition;
    transition.attach (*factory);
    transition.block (CLAP_EVENT_NOTE_ON, 60, 7, 0.85);
    for (int i = 1; i < 211; ++i) transition.block();
    const float beforeLeft = transition.left[255], beforeRight = transition.right[255];
    assert (std::max (std::abs (beforeLeft), std::abs (beforeRight)) > 0.05f);
    assert (transition.touch (SIXTEEN_TOUCH_DOWN, 638, 21));
    assert (transition.touch (SIXTEEN_TOUCH_DOWN, 697, 209)); // Dark Matter
    transition.block();
    const float transitionJump = std::max (std::abs (transition.left[0] - beforeLeft),
                                           std::abs (transition.right[0] - beforeRight));
    transition.stop();

    std::printf ("Chromatic/legacy pad order: %s\nHeld chord on sound change: %s\n",
                 bothRangesMatch ? "PASS" : "FAIL", oldChordReleased ? "PASS" : "FAIL");
    std::printf ("Late keyed NOTE_OFF/CHOKE: %s\nPreset boundary jump: %.7f\n",
                 retriggerKept ? "PASS" : "FAIL", transitionJump);
    f.stop();
    entry->deinit();
    dlclose (dylib);
    if (! bothRangesMatch || ! oldChordReleased || ! retriggerKept || transitionJump > 1.0e-5f) return 1;
    assert (checkMoodNotes());
    std::puts ("SP-16 screen, Mood, Prog banks, 88 parameters, chord pads, Motion, factory discovery, bottom-row selection and wet phaser audio passed");
    return 0;
}
