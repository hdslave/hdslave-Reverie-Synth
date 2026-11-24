// Black-box CLAP screen contract test. Runs on the native preview and the ARMv7 device build.
#include "png.h"
#include "sixteen_screen.h"
#include "params.h"

#include <clap/clap.h>
#include <dlfcn.h>

#include <array>
#include <cassert>
#include <cmath>
#include <cstring>
#include <string>

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

    int padSample (int slot) const { return (144 + (3 - slot / 4) * 37) * stride + 122 + (slot % 4) * 121; }
    uint32_t padPixel (int slot) const { return pixels[padSample (slot)]; }
    uint32_t heldPads (const std::vector<uint32_t>& idle) const
    {
        uint32_t held = 0;
        for (int slot = 0; slot < 16; ++slot)
            if (padPixel (slot) != idle[padSample (slot)]) held |= 1u << slot;
        return held;
    }
};
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
    Fixture f;
    f.attach (*factory);

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

    // Every parameter belongs to exactly one bank. Each page keeps the bank selected last.
    constexpr int bankCounts[] = { 2, 4, 4, 2, 2, 2 };
    constexpr const char* pageNames[] = { "chords", "motion", "osc", "filter", "amp", "fx" };
    std::array<int, count> uses {};
    for (int p = 0; p < 6; ++p)
    {
        if (p > 0) assert (f.touch (SIXTEEN_TOUCH_DOWN, 72 + 127 * p, 85));
        assert (f.frame() || p == 0);
        f.capture (out, pageNames[p]);
        for (int b = 0; b < bankCounts[p]; ++b)
        {
            uint32_t ids[6];
            char label[SIXTEEN_TAB_NAME_SIZE] {};
            assert (screen->knobs (plugin, ids, label));
            assert (std::strstr (label, " / ") != nullptr);
            for (auto id : ids)
                if (id != SIXTEEN_INVALID_PARAM_ID) { assert (id < count); ++uses[id]; }
            if (p == 0 && b == 0)
            {
                const uint32_t expected[] = { progression, voicing, key, octave, strum, latch };
                assert (std::equal (std::begin (ids), std::end (ids), std::begin (expected)));
            }
            assert (screen->selector_turn (plugin, 1));
        }
    }
    for (int used : uses) assert (used == 1);
    assert (screen->selector_push (plugin)); // FX → CHORDS

    // Touch each chord, then release outside the grid. The selected pad lights, and audio stays finite.
    assert (f.frame());
    for (int slot = 0; slot < 16; ++slot)
    {
        plugin->reset (plugin);
        f.frame();
        const auto idle = f.padPixel (slot);
        const int x = 70 + 121 * (slot % 4), y = 138 + 37 * (3 - slot / 4);
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
    assert (f.touch (SIXTEEN_TOUCH_DOWN, 70, 249));
    f.block();
    assert (f.touch (SIXTEEN_TOUCH_UP, 70, 249));
    f.block();
    f.frame();
    assert (f.padPixel (0) != first[f.padSample (0)]);
    f.block (CLAP_EVENT_NOTE_OFF);
    f.frame();
    assert (f.padPixel (0) == first[f.padSample (0)]);

    // A tap between blocks is played once. The close callback releases a hold.
    plugin->reset (plugin);
    assert (f.touch (SIXTEEN_TOUCH_DOWN, 70, 249) && f.touch (SIXTEEN_TOUCH_UP, 70, 249));
    f.block();
    f.frame();
    assert (f.padPixel (0) != first[f.padSample (0)]);
    f.block();
    f.frame();
    assert (f.padPixel (0) == first[f.padSample (0)]);
    assert (f.touch (SIXTEEN_TOUCH_DOWN, 70, 249));
    f.block();
    screen->on_close (plugin);
    f.block();
    f.frame();
    assert (f.padPixel (0) == first[f.padSample (0)]);

    // The freesp16 host stops rendering when leaving the plugin page. In that
    // case release the audition even if the touchscreen never sent touch-up.
    assert (f.touch (SIXTEEN_TOUCH_DOWN, 70, 249));
    for (int i = 0; i < 200; ++i) f.block();
    f.frame();
    assert (f.padPixel (0) == first[f.padSample (0)]);
    screen->on_close (plugin);

    // Drag outside the Motion field clamps the two existing bipolar parameters.
    assert (f.touch (SIXTEEN_TOUCH_DOWN, 199, 85));
    assert (f.touch (SIXTEEN_TOUCH_DOWN, 88, 188));
    assert (f.value (motionX) == 0.0 && f.value (motionY) == 0.0);
    assert (f.touch (SIXTEEN_TOUCH_MOVE, 116, 160));
    assert (f.value (motionX) == 0.5 && f.value (motionY) == 0.5);
    assert (f.touch (SIXTEEN_TOUCH_MOVE, 779, 284));
    assert (f.value (motionX) == 1.0 && f.value (motionY) == -1.0);
    assert (f.touch (SIXTEEN_TOUCH_UP, 779, 284));
    assert (! f.touch (SIXTEEN_TOUCH_MOVE, 20, 140));
    assert (f.value (motionX) == 1.0 && f.value (motionY) == -1.0);
    assert (wakes >= 36);

    const double before = f.value (osc1Wave), prog = f.value (progression), tonic = f.value (key);
    assert (f.touch (SIXTEEN_TOUCH_DOWN, 350, 30));
    assert (f.frame());
    f.capture (out, "sounds");
    assert (f.touch (SIXTEEN_TOUCH_DOWN, 242, 244)); // Ethereal Vox
    assert (f.value (osc1Wave) != before && f.value (progression) == prog && f.value (key) == tonic);
    assert (f.frame());
    assert (f.touch (SIXTEEN_TOUCH_DOWN, 350, 30));
    assert (screen->selector_turn (plugin, -16));
    assert (screen->selector_push (plugin)); // back to Reverie's default sound
    assert (f.value (osc1Wave) == before);
    // freesp16's unshifted chromatic SCALE mode sends 60–75. Legacy bank A
    // still sends 36–51. Both ranges address the same pads in the same order.
    assert (f.touch (SIXTEEN_TOUCH_DOWN, 72, 85));
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
    assert (f.touch (SIXTEEN_TOUCH_DOWN, 350, 30));
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
    assert (transition.touch (SIXTEEN_TOUCH_DOWN, 350, 30));
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
    std::puts ("SP-16 screen, 80 bindings, chord pads, MIDI independence, Motion and sound switching passed");
    return 0;
}
