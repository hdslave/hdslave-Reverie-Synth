#pragma once

#include "screen.h"
#include "sixteen_screen.h"

namespace reverie
{
class Sp16Screen final
{
public:
    Sp16Screen (const std::atomic<float>* values, const Telemetry& telemetry, const Screen& plots)
        : values (values), telemetry (telemetry), plots (plots) {}

    bool render (const sixteen_screen_surface_t& surface, bool full);
    void knobs (uint32_t ids[6], char name[SIXTEEN_TAB_NAME_SIZE]) const;
    bool turn (int delta);
    void soundLoaded (int index);
    int push();

    struct Touch
    {
        bool used = false;
        int pad = -1;
        int sound = -1;
        bool release = false;
        hdui::Edits edits;
    };
    Touch touch (int phase, int x, int y);
    void close() { gesture = Gesture::none; soundsOpen = false; dirty = true; }

private:
    enum class Gesture { none, pad, motion };
    using Raster = hdui::Raster;

    void header (const Raster& r, int sound) const;
    void sounds (const Raster& r) const;
    void performance (const Raster& r, uint32_t sounding, uint32_t held) const;
    bool performanceChanged (uint32_t sounding);
    void synthesis (const Raster& r, bool sounding) const;
    void output (const Raster& r, bool sounding) const;
    void field (const Raster& r, int x, int y, int width, const char* label, uint32_t id) const;
    int matchingSound() const;
    float value (uint32_t id) const { return values[id].load (std::memory_order_relaxed); }
    int choice (uint32_t id) const { return static_cast<int> (std::lround (value (id))); }

    const std::atomic<float>* values;
    const Telemetry& telemetry;
    const Screen& plots;
    std::array<uint32_t, SIXTEEN_SCREEN_WIDTH * SIXTEEN_SCREEN_HEIGHT> pixels {};
    std::array<float, paramCount> seen {};
    std::array<int, Screen::pageCount> banks {};
    int page = Screen::chordsPage;
    int focusedSound = 0, lastFactory = 0;
    bool soundsOpen = false;
    Gesture gesture = Gesture::none;
    bool dirty = true, wasMoving = false;
    uint32_t shownHeld = 0, shownSounding = 0, shownStrikes = 0, frameAt = 0;
    struct VoiceLight { int note = -1, x = 0, y = 0; float level = 0; };
    std::array<VoiceLight, maxVoices> shownLights {};
    Screen::MotionCursor shownCursor;
    int shownSlot = -1;
};
}
