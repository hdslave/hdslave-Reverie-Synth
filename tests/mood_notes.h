// Exercise note-on and read the pitches of actual sounding voices. The expected
// FNV-1a digest covers the note count and ascending MIDI bytes for each of the
// 288 database entries in pad order, at their documented concert pitches.
#pragma once

#include <algorithm>
#include <memory>

inline bool checkMoodNotes()
{
    using namespace reverie;
    Telemetry telemetry;
    auto engine = std::make_unique<Engine> (&telemetry);
    engine->prepare (48000);
    engine->set (ParamId::strum, 0);
    engine->set (ParamId::humanize, 0);
    engine->set (ParamId::key, 7);
    engine->set (ParamId::voicing, Voicing::rootless);
    for (int shift : { -12, 0, 7, 12 })
    {
        uint64_t digest = 14695981039346656037ull;
        auto add = [&] (int value) { digest = (digest ^ (uint8_t) value) * 1099511628211ull; };
        engine->set (ParamId::moodShift, (float) shift);
        for (int set = 1; set <= moodCount; ++set)
        {
            engine->set (ParamId::mood, (float) set);
            for (int slot = 0; slot < padSlots; ++slot)
            {
                engine->reset();
                engine->noteOn (36 + slot, 1);
                float left[64], right[64];
                engine->render (left, right, 64);
                int pitches[maxVoices], count = 0;
                const auto sounding = telemetry.sounding.load();
                for (int v = 0; v < maxVoices; ++v)
                    if ((sounding >> v) & 1u) pitches[count++] = telemetry.notes[v].load() - shift;
                std::sort (pitches, pitches + count);
                add (count);
                for (int i = 0; i < count; ++i) add (pitches[i]);
            }
        }
        if (digest != 0xd46e06bce3d689eaull) return false;
    }
    return true;
}
