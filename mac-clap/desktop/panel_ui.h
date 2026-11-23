// The desktop panel: displays all of the plug-in's live 320x218 MPC screens simultaneously on one unified hardware
// dashboard, with direct mouse, trackpad, and touch control over every parameter cell, row, bar, K1-K3 card, step
// sequencer lane, pad, 2D X/Y field, 3D cube, rhythm wheel, and punch-card bit.
#pragma once

#include "desktop_wrapper.h"
#include "panel_renderer.h"

#include <cstdint>
#include <string>
#include <vector>

namespace hdslave_desktop
{

enum Mods : uint32_t
{
    kModShift = 1,  // fine adjustment / alternate action
    kModAlt = 2,    // reset to default on click
    kModCmd = 4,    // reset to default on click (Cmd on macOS, Ctrl on Windows)
    kModTouch = 8,  // event came from a touch screen or pen
};

enum class Item : uint8_t
{
    PresetPrev,
    PresetBox,
    PresetNext,
    Zoom,
    Shift,
    Learn,
    ClearCc,
    Readout,
    Count
};

struct TileView
{
    uint32_t index = 0;
    std::string title;
    Rect cardRect {};
    Rect headerRect {};
    Rect zoomBtnRect {};
    Rect screenRect {};
    Rect tabRect {}; // used when a tile is zoomed to full panel
};

class PanelUI
{
public:
    explicit PanelUI (DesktopPlugin* plugin);

    void setSize (int w, int h, int scale);
    Rect update (uint64_t now);
    void invalidateAll() { fullDirty_ = true; }
    void layoutChanged() { layoutDirty_ = true; }

    const uint32_t* pixels() const { return frame_.data(); }
    int physWidth() const { return physW_; }
    int physHeight() const { return physH_; }
    int scale() const { return scale_; }
    int width() const { return w_; }
    int height() const { return h_; }

    // Input in logical coordinates. button: 0 left, 1 right. clicks: 1, 2 (double).
    void mouseDown (float x, float y, int button, uint32_t mods, int clicks);
    void mouseMove (float x, float y, uint32_t mods);
    void mouseUp (float x, float y, int button, uint32_t mods);
    void wheel (float x, float y, float dx, float dy, bool precise, uint32_t mods);
    void rotate (float x, float y, float degrees);
    void mouseExit();
    bool key (int k);

    double lastRenderMs() const { return lastRenderMs_; }

private:
    DesktopPlugin* p_;
    int w_ = int (kDefaultGuiWidth), h_ = int (kDefaultGuiHeight), scale_ = 1;
    std::vector<uint32_t> frame_;
    int physW_ = 0, physH_ = 0;
    Canvas c_;
    bool fullDirty_ = true, layoutDirty_ = true;
    Rect dirty_ {};
    double lastRenderMs_ = 0.0;
    uint64_t now_ = 0;

    // Layout
    Rect top_ {}, gridArea_ {};
    Rect items_[(size_t) Item::Count] {};
    std::vector<TileView> tiles_;
    int focusedTile_ = 0;
    int zoomedTile_ = -1; // -1 = show all MPC screens at once; 0..N-1 = zoomed single screen

    void layout();

    // Interaction state
    enum class Drag : uint8_t
    {
        None = 0,
        Param1D,
        Param2D,
        ParamAbsolute,
        BitPaint,
        NotePad,
    };
    Drag drag_ = Drag::None;
    int dragTile_ = -1;
    int dragParam_ = -1;
    int dragParamY_ = -1;
    hdslave_tile_hotspot_t dragSpot_ {};
    float downX_ = 0.0f, downY_ = 0.0f, lastX_ = 0.0f, lastY_ = 0.0f;
    double dragStartNorm_ = 0.0;
    double dragStartNormY_ = 0.0;
    double dragAccum_ = 0.0;
    bool moved_ = false;
    int dragButton_ = 0;
    uint32_t downMods_ = 0;
    bool bitPaintValue_ = true;
    int activeNote_ = -1;

    // Hover state
    Item hoverItem_ = Item::Count;
    int hoverTile_ = -1;
    int hoverZoomBtnTile_ = -1;
    int hoverTabTile_ = -1;
    hdslave_tile_hotspot_t hoverSpot_ {};
    int hoverParam_ = -1;
    int lastTouched_ = -1;

    // Wheel gesture state
    int wheelParam_ = -1;
    uint64_t wheelAt_ = 0;
    float wheelAccum_ = 0.0f;

    // Preset / Choice / Menu popup
    enum class PopupKind : uint8_t { None = 0, Presets, Choices, Menu };
    PopupKind popup_ = PopupKind::None;
    int popupParam_ = -1;
    std::vector<std::string> popupItems_;
    std::vector<double> popupValues_;
    std::vector<Rect> popupRects_;
    Rect popupRect_ {};
    int popupHover_ = -1;
    int popupSelected_ = -1;

    void openPresets();
    void openChoices (int param, const Rect& anchor);
    void openMenu (int param, float x, float y);
    void closePopup();
    void layoutPopup (const Rect& anchor);
    void popupPick (int i);

    // Parameter value helpers
    double norm (int param) const;
    double fromNorm (int param, double n) const;
    void setParam (int param, double v);
    void stepParam (int param, int steps);
    void endWheelGesture();
    void resetParam (int param);
    void applyAbsoluteSpot (int tileIdx, const hdslave_tile_hotspot_t& spot, float lx, float ly);

    // Hit testing helpers
    Item hitItem (float x, float y) const;
    int hitTileScreen (float x, float y, int& outMx, int& outMy) const;
    Rect spotToLogicalRect (int tileIdx, const hdslave_tile_hotspot_t& spot) const;

    // Drawing
    void renderAll();
    void drawHeaderBar();
    void drawTile (int tileIdx);
    void drawPopup();
    std::string valueText (int param) const;
    std::string readoutText (bool& learnLine) const;
};

} // namespace hdslave_desktop
