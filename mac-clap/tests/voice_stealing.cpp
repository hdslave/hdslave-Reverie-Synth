#include "engine.h"

#include <algorithm>
#include <array>
#include <cstdio>

int main()
{
    using namespace reverie;

    // MPC and SP-16 keep these voices when Velvet Night pads A01–A06 arrive
    // in pad order at sample zero. Keep repeated pitches in the comparison.
    constexpr std::array<int, 24> expected {
        36, 38, 43, 45, 53, 53, 59, 59, 60, 60, 62, 64, 64, 64, 66, 67, 67, 69, 69, 71, 71, 74, 74, 76
    };
    Telemetry telemetry;
    Engine engine (&telemetry);
    engine.prepare (48000);
    engine.set (ParamId::voicing, Voicing::open);
    engine.set (ParamId::strum, 0.0f);
    engine.set (ParamId::humanize, 0.0f);
    for (int key = 36; key <= 41; ++key)
        engine.noteOn (key, 0.8f);

    std::array<float, 480> left {}, right {};
    engine.render (left.data(), right.data(), left.size());
    const uint32_t sounding = telemetry.sounding.load();
    std::array<int, maxVoices> actual {};
    size_t count = 0;
    for (int voice = 0; voice < maxVoices; ++voice)
        if ((sounding >> voice) & 1u)
            actual[count++] = telemetry.notes[voice].load();
    std::sort (actual.begin(), actual.begin() + count);
    if (count != expected.size() || !std::equal (expected.begin(), expected.end(), actual.begin()))
    {
        std::fprintf (stderr, "Six simultaneous pads kept %zu voices:", count);
        for (size_t i = 0; i < count; ++i)
            std::fprintf (stderr, " %d", actual[i]);
        std::fputc ('\n', stderr);
        return 1;
    }
    std::puts ("PASS: six simultaneous pads keep the same 24 pitches as MPC and SP-16");
}
