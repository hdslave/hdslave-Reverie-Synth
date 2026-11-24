#include "panel_ui.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>

namespace hdslave_desktop
{

namespace
{

std::string upper (const std::string& s)
{
    std::string out = s;
    for (char& ch : out)
        if (ch >= 'a' && ch <= 'z') ch = char (ch - 'a' + 'A');
    return out;
}

void drawButtonBox (Canvas& c, const Rect& r, const std::string& label, const PluginTheme& th,
                    bool active, bool hot)
{
    const uint32_t bg = active ? th.accent : (hot ? mix (th.panel, th.accent, 0.22f) : th.panel);
    const uint32_t border = active ? 0xFFFFFFFFu : (hot ? th.accent : th.border);
    const uint32_t fg = active ? 0xFF06080Cu : (hot ? 0xFFFFFFFFu : th.text);
    c.fillChamfer (r, 3, bg);
    c.frameChamfer (r, 3, border);
    c.textAligned (r, label, fg, 1, 1);
}

} // namespace

PanelUI::PanelUI (DesktopPlugin* plugin)
    : p_ (plugin)
{
    if (!p_->params.empty())
        lastTouched_ = 0;
}

void PanelUI::setSize (int w, int h, int scale)
{
    w = std::clamp (w, int (kMinGuiWidth), int (kMaxGuiWidth));
    h = std::clamp (h, int (kMinGuiHeight), int (kMaxGuiHeight));
    scale = std::clamp (scale, 1, 4);
    if (w == w_ && h == h_ && scale == scale_ && !frame_.empty())
        return;
    w_ = w;
    h_ = h;
    scale_ = scale;
    physW_ = w_ * scale_;
    physH_ = h_ * scale_;
    frame_.assign (size_t (physW_) * size_t (physH_), p_->theme.bgBottom | 0xFF000000u);
    c_.begin (frame_.data(), physW_, physH_, scale_);
    layoutDirty_ = true;
    fullDirty_ = true;
}

void PanelUI::layout()
{
    layoutDirty_ = false;
    top_ = { 8, 6, w_ - 16, 42 };

    // Top bar items
    int x = top_.x + 230;
    const int by = top_.y + 7, bh = 28;
    items_[(size_t) Item::PresetPrev] = { x, by, 26, bh }; x += 28;
    items_[(size_t) Item::PresetBox]  = { x, by, 236, bh }; x += 238;
    items_[(size_t) Item::PresetNext] = { x, by, 26, bh }; x += 34;

    items_[(size_t) Item::Zoom]    = { x, by, 102, bh }; x += 108;
    items_[(size_t) Item::Shift]   = { x, by, 62, bh };  x += 68;
    items_[(size_t) Item::Learn]   = { x, by, 86, bh };  x += 90;
    items_[(size_t) Item::ClearCc] = { x, by, 68, bh };  x += 76;

    const int readoutW = std::max (160, (top_.x + top_.w - 8) - x);
    items_[(size_t) Item::Readout] = { x, top_.y + 5, readoutW, 32 };

    gridArea_ = { 8, 54, w_ - 16, h_ - 62 };

    const uint32_t count = std::max<uint32_t> (1, p_->tileCount);
    const int cols = std::max (1, p_->tileCols);
    const int rows = std::max (1, p_->tileRows);

    tiles_.resize (count);
    for (uint32_t t = 0; t < count; ++t)
    {
        tiles_[t].index = t;
        if (p_->innerTiles && p_->innerTiles->title)
            tiles_[t].title = p_->innerTiles->title (p_->inner, t);
        else
            tiles_[t].title = p_->pluginName;
    }

    if (zoomedTile_ >= 0 && zoomedTile_ < (int) count)
    {
        // Tab bar at the top of gridArea_ for instant tile switching while zoomed
        const int tabH = 24;
        const int tabGap = 4;
        const int tabW = std::max (60, (gridArea_.w - (int (count) - 1) * tabGap) / int (count));
        for (uint32_t t = 0; t < count; ++t)
        {
            tiles_[t].tabRect = { gridArea_.x + int (t) * (tabW + tabGap), gridArea_.y, tabW, tabH };
            tiles_[t].cardRect = {};
            tiles_[t].headerRect = {};
            tiles_[t].zoomBtnRect = {};
            tiles_[t].screenRect = {};
        }

        const Rect card { gridArea_.x, gridArea_.y + tabH + 6, gridArea_.w, gridArea_.h - tabH - 6 };
        auto& zt = tiles_[(size_t) zoomedTile_];
        zt.cardRect = card;
        zt.headerRect = { card.x + 2, card.y + 2, card.w - 4, 22 };
        zt.zoomBtnRect = { card.x + card.w - 82, card.y + 4, 76, 18 };

        const int scrH = std::max (120, p_->tileScreenH);
        const Rect body { card.x + 6, card.y + 26, card.w - 12, card.h - 32 };
        int sw = body.w;
        int sh = sw * scrH / MELODEE_SCREEN_WIDTH;
        if (sh > body.h)
        {
            sh = body.h;
            sw = sh * MELODEE_SCREEN_WIDTH / scrH;
        }
        zt.screenRect = { body.x + (body.w - sw) / 2, body.y + (body.h - sh) / 2, sw, sh };
        return;
    }

    // All-Screens Grid View: size each card tightly around its 320x(tileScreenH) MPC screen
    const int scrH = std::max (120, p_->tileScreenH);
    const int gapX = 8, gapY = 8;
    const int maxCellW = (gridArea_.w - (cols - 1) * gapX) / cols;
    const int maxCellH = (gridArea_.h - (rows - 1) * gapY) / rows;
    int sw = std::max (64, maxCellW - 8);
    int sh = sw * scrH / MELODEE_SCREEN_WIDTH;
    if (sh > std::max (44, maxCellH - 25))
    {
        sh = std::max (44, maxCellH - 25);
        sw = sh * MELODEE_SCREEN_WIDTH / scrH;
    }
    const int cellW = sw + 8;
    const int cellH = sh + 25;
    const int gridTotalH = rows * cellH + (rows - 1) * gapY;
    const int offsetY = (gridArea_.h - gridTotalH) / 2;

    for (uint32_t t = 0; t < count; ++t)
    {
        const int r = int (t) / cols;
        const int c = int (t) % cols;
        // Count how many tiles are on this row so partial last rows (e.g. 7 tiles in 4x2) center cleanly
        const int rowStart = r * cols;
        const int tilesInRow = std::min (cols, int (count) - rowStart);
        const int rowTotalW = tilesInRow * cellW + (tilesInRow - 1) * gapX;
        const int rowOffsetX = (gridArea_.w - rowTotalW) / 2;

        const Rect card {
            gridArea_.x + rowOffsetX + c * (cellW + gapX),
            gridArea_.y + offsetY + r * (cellH + gapY),
            cellW,
            cellH
        };
        auto& tv = tiles_[t];
        tv.tabRect = {};
        tv.cardRect = card;
        tv.headerRect = { card.x + 2, card.y + 2, card.w - 4, 18 };
        tv.zoomBtnRect = { card.x + card.w - 52, card.y + 3, 48, 15 };
        tv.screenRect = { card.x + 4, card.y + 21, sw, sh };
    }
}

Rect PanelUI::update (uint64_t now)
{
    now_ = now;
    if (wheelParam_ >= 0 && now_ - wheelAt_ > 240)
        endWheelGesture();

    if (layoutDirty_)
        layout();

    // Paint all MPC screen tiles every frame (~60 fps) when open or requested
    const auto t0 = std::chrono::steady_clock::now();
    p_->paintAllTiles (now_);
    renderAll();
    const auto t1 = std::chrono::steady_clock::now();
    lastRenderMs_ = std::chrono::duration<double, std::milli> (t1 - t0).count();

    fullDirty_ = false;
    return { 0, 0, w_, h_ };
}

void PanelUI::renderAll()
{
    const auto& th = p_->theme;
    c_.resetClip();
    c_.gradient ({ 0, 0, w_, h_ }, th.bgTop, th.bgBottom);

    drawHeaderBar();

    if (zoomedTile_ >= 0 && zoomedTile_ < (int) tiles_.size())
    {
        // Draw tile switcher tabs across the top
        for (size_t t = 0; t < tiles_.size(); ++t)
        {
            const bool active = ((int) t == zoomedTile_);
            const bool hot = ((int) t == hoverTabTile_);
            drawButtonBox (c_, tiles_[t].tabRect, tiles_[t].title, th, active, hot);
        }
        drawTile (zoomedTile_);
    }
    else
    {
        for (size_t t = 0; t < tiles_.size(); ++t)
            drawTile ((int) t);
    }

    if (popup_ != PopupKind::None)
        drawPopup();
}

void PanelUI::drawHeaderBar()
{
    const auto& th = p_->theme;
    c_.fillChamfer (top_, 5, th.headerTop);
    c_.frameChamfer (top_, 5, th.border);
    c_.fill (top_.x + 6, top_.y, 190, 2, th.accent);

    // Plugin brand + name + version
    std::string shortName = p_->pluginName;
    if (shortName.rfind ("hdslave ", 0) == 0)
        shortName = shortName.substr (8);
    shortName = upper (shortName);

    c_.tag (top_.x + 10, top_.y + 8, "HDSLAVE", th.accent, 0xFF06080Cu, 4);
    c_.text (top_.x + 74, top_.y + 9, shortName, th.text, 1, 148);
    char sub[64];
    std::snprintf (sub, sizeof (sub), "MPC // %u SCREENS // v%s",
                   std::max<uint32_t> (1, p_->tileCount), p_->pluginVersion.c_str());
    c_.text (top_.x + 10, top_.y + 25, sub, th.muted, 1, 218);

    // Preset selector
    drawButtonBox (c_, items_[(size_t) Item::PresetPrev], "<", th, false, hoverItem_ == Item::PresetPrev);
    std::string presetLabel = "INIT PATCH";
    if (!p_->presets.empty())
    {
        const int idx = std::clamp (p_->currentPresetIndex, 0, int (p_->presets.size()) - 1);
        char num[16];
        std::snprintf (num, sizeof (num), "%02d/%02d ",
                       p_->currentPresetIndex >= 0 ? idx + 1 : 0,
                       int (p_->presets.size()));
        presetLabel = std::string (num) + (p_->currentPresetIndex >= 0 ? p_->presets[(size_t) idx].name : "DEFAULT");
    }
    drawButtonBox (c_, items_[(size_t) Item::PresetBox], presetLabel, th,
                   popup_ == PopupKind::Presets, hoverItem_ == Item::PresetBox);
    drawButtonBox (c_, items_[(size_t) Item::PresetNext], ">", th, false, hoverItem_ == Item::PresetNext);

    // View / Shift / MIDI CC controls
    drawButtonBox (c_, items_[(size_t) Item::Zoom],
                   zoomedTile_ >= 0 ? "ALL SCREENS" : "ZOOM SCREEN",
                   th, zoomedTile_ >= 0, hoverItem_ == Item::Zoom);

    drawButtonBox (c_, items_[(size_t) Item::Shift], "SHIFT",
                   th, p_->screenShift, hoverItem_ == Item::Shift);

    const uint32_t learnTgt = p_->midiLearnTarget.load (std::memory_order_relaxed);
    const bool learning = (learnTgt != CLAP_INVALID_ID);
    drawButtonBox (c_, items_[(size_t) Item::Learn],
                   learning ? "LEARNING..." : "MIDI LEARN",
                   th, learning, hoverItem_ == Item::Learn);

    char ccText[24];
    const int mappedCount = p_->countMappedCc();
    if (mappedCount > 0) std::snprintf (ccText, sizeof (ccText), "CLR (%d)", mappedCount);
    else std::snprintf (ccText, sizeof (ccText), "CLR CC");
    drawButtonBox (c_, items_[(size_t) Item::ClearCc], ccText, th, false, hoverItem_ == Item::ClearCc);

    // Readout box
    const Rect& ro = items_[(size_t) Item::Readout];
    c_.fillChamfer (ro, 3, 0xFF07090Eu);
    c_.frameChamfer (ro, 3, learning ? th.accent : th.border);
    bool learnLine = false;
    const std::string line = readoutText (learnLine);
    c_.textAligned (ro.inset (4), line, learnLine ? th.accent : th.text, 1, 0);
}

void PanelUI::drawTile (int tileIdx)
{
    if (tileIdx < 0 || tileIdx >= (int) tiles_.size()) return;
    const auto& tv = tiles_[(size_t) tileIdx];
    if (tv.cardRect.empty()) return;

    const auto& th = p_->theme;
    const bool isFocused = (focusedTile_ == tileIdx);
    const bool isHovered = (hoverTile_ == tileIdx);

    // Tile hardware bezel card
    c_.fillChamfer (tv.cardRect, 4, th.panel);
    c_.frameChamfer (tv.cardRect, 4, isFocused ? th.accent : (isHovered ? mix (th.border, th.accent, 0.45f) : th.border));

    // Tile header strip
    c_.fill (tv.headerRect.x, tv.headerRect.y, tv.headerRect.w, tv.headerRect.h, th.headerTop);
    c_.disc (float (tv.headerRect.x + 8), float (tv.headerRect.y + tv.headerRect.h / 2), 3.0f,
             isFocused ? th.accent : th.muted);
    c_.text (tv.headerRect.x + 16, tv.headerRect.y + (tv.headerRect.h - 12) / 2,
             tv.title, isFocused ? 0xFFFFFFFFu : th.text, 1,
             tv.headerRect.w - tv.zoomBtnRect.w - 24);

    // Tile [ZOOM] / [GRID] mini button
    const bool zoomHot = (hoverZoomBtnTile_ == tileIdx);
    drawButtonBox (c_, tv.zoomBtnRect, zoomedTile_ >= 0 ? "GRID" : "ZOOM", th, zoomedTile_ == tileIdx, zoomHot);

    // Recessed screen bezel + sharp scaled 320x218 MPC screen blit
    const Rect bezel = tv.screenRect.inset (-2);
    c_.fill (bezel, 0xFF040608u);
    c_.frame (bezel, isFocused ? mix (th.border, th.accent, 0.5f) : th.border);

    const uint32_t* srcPx = nullptr;
    if (tileIdx < (int) p_->tilePixels.size() && p_->tilePixels[(size_t) tileIdx].size() == size_t (MELODEE_SCREEN_WIDTH * MELODEE_SCREEN_PAGE_HEIGHT))
        srcPx = p_->tilePixels[(size_t) tileIdx].data();
    else if (p_->screenPixels.size() == size_t (MELODEE_SCREEN_WIDTH * MELODEE_SCREEN_PAGE_HEIGHT))
        srcPx = p_->screenPixels.data();

    if (srcPx)
        c_.blitSharp (srcPx, MELODEE_SCREEN_WIDTH, std::max (120, p_->tileScreenH), tv.screenRect);

    // Interactive hotspot highlight overlay (hover or active drag)
    const bool draggingHere = (drag_ != Drag::None && dragTile_ == tileIdx && dragSpot_.kind != HDSLAVE_HOTSPOT_NONE);
    const bool hoveringHere = (drag_ == Drag::None && hoverTile_ == tileIdx && hoverSpot_.kind != HDSLAVE_HOTSPOT_NONE);

    if (draggingHere || hoveringHere)
    {
        const hdslave_tile_hotspot_t& sp = draggingHere ? dragSpot_ : hoverSpot_;
        const Rect hr = spotToLogicalRect (tileIdx, sp);
        if (!hr.empty())
        {
            const uint32_t col = draggingHere ? 0xFFFFFFFFu : th.accent;
            c_.fill (hr, th.accent, draggingHere ? 38 : 22);
            c_.frame (hr, col, draggingHere ? 245 : 190);

            // Corner ticks for crisp visual feedback
            const int tk = std::min (5, std::min (hr.w, hr.h) / 3);
            if (tk >= 2)
            {
                c_.fill (hr.x, hr.y, tk, 2, col);
                c_.fill (hr.x, hr.y, 2, tk, col);
                c_.fill (hr.x + hr.w - tk, hr.y, tk, 2, col);
                c_.fill (hr.x + hr.w - 2, hr.y, 2, tk, col);
                c_.fill (hr.x, hr.y + hr.h - 2, tk, 2, col);
                c_.fill (hr.x, hr.y + hr.h - tk, 2, tk, col);
                c_.fill (hr.x + hr.w - tk, hr.y + hr.h - 2, tk, 2, col);
                c_.fill (hr.x + hr.w - 2, hr.y + hr.h - tk, 2, tk, col);
            }

            // Show MIDI CC badge if the hovered parameter is mapped to a CC
            if (sp.param_id != CLAP_INVALID_ID)
            {
                const int cc = p_->findCcForParam (sp.param_id);
                if (cc >= 0)
                {
                    char ccTag[16];
                    std::snprintf (ccTag, sizeof (ccTag), "CC%d", cc);
                    c_.tag (hr.x + 2, std::max (tv.screenRect.y, hr.y - 14), ccTag, th.accent, 0xFF05070Au, 2);
                }
            }
        }
    }
}

Rect PanelUI::spotToLogicalRect (int tileIdx, const hdslave_tile_hotspot_t& spot) const
{
    if (tileIdx < 0 || tileIdx >= (int) tiles_.size() || spot.w <= 0 || spot.h <= 0)
        return {};
    const Rect& sr = tiles_[(size_t) tileIdx].screenRect;
    if (sr.empty()) return {};
    const int scrH = std::max (120, p_->tileScreenH);
    const int x0 = sr.x + (spot.x * sr.w) / MELODEE_SCREEN_WIDTH;
    const int y0 = sr.y + (spot.y * sr.h) / scrH;
    const int x1 = sr.x + ((spot.x + spot.w) * sr.w) / MELODEE_SCREEN_WIDTH;
    const int y1 = sr.y + ((spot.y + spot.h) * sr.h) / scrH;
    return { x0, y0, std::max (4, x1 - x0), std::max (4, y1 - y0) };
}

int PanelUI::hitTileScreen (float x, float y, int& outMx, int& outMy) const
{
    const int ix = int (x), iy = int (y);
    const int scrH = std::max (120, p_->tileScreenH);
    for (size_t t = 0; t < tiles_.size(); ++t)
    {
        if (zoomedTile_ >= 0 && (int) t != zoomedTile_)
            continue;
        const Rect& sr = tiles_[t].screenRect;
        if (sr.contains (ix, iy) && sr.w > 0 && sr.h > 0)
        {
            outMx = std::clamp ((ix - sr.x) * MELODEE_SCREEN_WIDTH / sr.w, 0, MELODEE_SCREEN_WIDTH - 1);
            outMy = std::clamp ((iy - sr.y) * scrH / sr.h, 0, scrH - 1);
            return (int) t;
        }
    }
    return -1;
}

Item PanelUI::hitItem (float x, float y) const
{
    const int ix = int (x), iy = int (y);
    for (size_t i = 0; i < (size_t) Item::Count; ++i)
        if (items_[i].contains (ix, iy))
            return (Item) i;
    return Item::Count;
}

double PanelUI::norm (int param) const
{
    if (param < 0 || param >= int (p_->params.size())) return 0.0;
    const auto& pr = p_->params[(size_t) param];
    const double r = pr.range();
    return r > 0.0 ? std::clamp ((p_->displayValue (param) - pr.minVal) / r, 0.0, 1.0) : 0.0;
}

double PanelUI::fromNorm (int param, double n) const
{
    if (param < 0 || param >= int (p_->params.size())) return 0.0;
    const auto& pr = p_->params[(size_t) param];
    n = std::clamp (n, 0.0, 1.0);
    double v = pr.minVal + n * pr.range();
    if (pr.isStepped()) v = std::clamp (std::round (v), pr.minVal, pr.maxVal);
    return v;
}

void PanelUI::setParam (int param, double v)
{
    if (param < 0 || param >= int (p_->params.size())) return;
    p_->setEdit (param, v);
    lastTouched_ = param;
}

void PanelUI::stepParam (int param, int steps)
{
    if (param < 0 || param >= int (p_->params.size()) || steps == 0) return;
    if (wheelParam_ != param)
    {
        endWheelGesture();
        wheelParam_ = param;
        p_->beginEdit (param);
    }
    wheelAt_ = nowMs();
    const auto& pr = p_->params[(size_t) param];
    if (pr.isStepped())
    {
        setParam (param, std::clamp (p_->displayValue (param) + double (steps), pr.minVal, pr.maxVal));
    }
    else
    {
        setParam (param, fromNorm (param, norm (param) + double (steps) / 160.0));
    }
}

void PanelUI::endWheelGesture()
{
    if (wheelParam_ >= 0)
    {
        p_->endEdit (wheelParam_);
        wheelParam_ = -1;
    }
}

void PanelUI::resetParam (int param)
{
    if (param < 0 || param >= int (p_->params.size())) return;
    endWheelGesture();
    p_->setValueOnce (param, p_->params[(size_t) param].defVal);
    lastTouched_ = param;
}

void PanelUI::applyAbsoluteSpot (int tileIdx, const hdslave_tile_hotspot_t& spot, float lx, float ly)
{
    if (tileIdx < 0 || tileIdx >= (int) tiles_.size()) return;
    const Rect& sr = tiles_[(size_t) tileIdx].screenRect;
    if (sr.w <= 0 || sr.h <= 0) return;

    const int scrH = std::max (120, p_->tileScreenH);
    const float mx = (lx - float (sr.x)) * float (MELODEE_SCREEN_WIDTH) / float (sr.w);
    const float my = (ly - float (sr.y)) * float (scrH) / float (sr.h);

    const float nx = spot.w > 1 ? std::clamp ((mx - float (spot.x)) / float (spot.w), 0.0f, 1.0f) : 0.5f;
    float ny = spot.h > 1 ? std::clamp ((my - float (spot.y)) / float (spot.h), 0.0f, 1.0f) : 0.5f;
    if (spot.invert_y) ny = 1.0f - ny;

    if (dragParam_ >= 0 && dragParamY_ >= 0)
    {
        setParam (dragParam_, fromNorm (dragParam_, nx));
        setParam (dragParamY_, fromNorm (dragParamY_, ny));
    }
    else if (dragParam_ >= 0)
    {
        setParam (dragParam_, fromNorm (dragParam_, ny));
    }
}

void PanelUI::mouseDown (float x, float y, int button, uint32_t mods, int clicks)
{
    endWheelGesture();
    downX_ = lastX_ = x;
    downY_ = lastY_ = y;
    moved_ = false;
    dragButton_ = button;
    downMods_ = mods;

    // 1. Active popup handling
    if (popup_ != PopupKind::None)
    {
        if (popupRect_.contains (int (x), int (y)))
        {
            for (size_t i = 0; i < popupRects_.size(); ++i)
            {
                if (popupRects_[i].contains (int (x), int (y)))
                {
                    popupPick (int (i));
                    return;
                }
            }
            return;
        }
        closePopup();
        return;
    }

    // 2. Top header bar controls
    const Item it = hitItem (x, y);
    if (it != Item::Count)
    {
        switch (it)
        {
            case Item::PresetPrev:
                if (!p_->presets.empty())
                {
                    const int n = int (p_->presets.size());
                    const int next = p_->currentPresetIndex <= 0 ? n - 1 : p_->currentPresetIndex - 1;
                    p_->loadPresetByIndex (next);
                }
                return;
            case Item::PresetNext:
                if (!p_->presets.empty())
                {
                    const int n = int (p_->presets.size());
                    const int next = (p_->currentPresetIndex + 1) % n;
                    p_->loadPresetByIndex (next);
                }
                return;
            case Item::PresetBox:
                openPresets();
                return;
            case Item::Zoom:
                if (zoomedTile_ >= 0) zoomedTile_ = -1;
                else zoomedTile_ = std::clamp (focusedTile_, 0, std::max (0, int (tiles_.size()) - 1));
                layoutDirty_ = true;
                return;
            case Item::Shift:
                p_->screenShift = !p_->screenShift;
                p_->screenWholeNeeded = true;
                return;
            case Item::Learn:
            {
                const uint32_t cur = p_->midiLearnTarget.load (std::memory_order_relaxed);
                if (cur != CLAP_INVALID_ID)
                    p_->midiLearnTarget.store (CLAP_INVALID_ID, std::memory_order_release);
                else if (lastTouched_ >= 0 && lastTouched_ < int (p_->params.size()))
                    p_->midiLearnTarget.store (p_->params[(size_t) lastTouched_].id, std::memory_order_release);
                else
                    p_->midiLearnTarget.store (DesktopPlugin::kLearnArmedNoParam, std::memory_order_release);
                return;
            }
            case Item::ClearCc:
                if (lastTouched_ >= 0 && lastTouched_ < int (p_->params.size())
                    && p_->findCcForParam (p_->params[(size_t) lastTouched_].id) >= 0)
                    p_->clearCcForParam (p_->params[(size_t) lastTouched_].id);
                else
                    p_->clearAllCc();
                return;
            default:
                return;
        }
    }

    // 3. Zoomed-mode tile tab bar
    if (zoomedTile_ >= 0)
    {
        for (size_t t = 0; t < tiles_.size(); ++t)
        {
            if (tiles_[t].tabRect.contains (int (x), int (y)))
            {
                zoomedTile_ = (int) t;
                focusedTile_ = (int) t;
                layoutDirty_ = true;
                return;
            }
        }
    }

    // 4. Tile header & [ZOOM] button
    for (size_t t = 0; t < tiles_.size(); ++t)
    {
        if (zoomedTile_ >= 0 && (int) t != zoomedTile_)
            continue;
        if (tiles_[t].zoomBtnRect.contains (int (x), int (y)))
        {
            focusedTile_ = (int) t;
            zoomedTile_ = (zoomedTile_ == (int) t) ? -1 : (int) t;
            layoutDirty_ = true;
            return;
        }
        if (tiles_[t].headerRect.contains (int (x), int (y)))
        {
            focusedTile_ = (int) t;
            if (clicks >= 2)
            {
                zoomedTile_ = (zoomedTile_ == (int) t) ? -1 : (int) t;
                layoutDirty_ = true;
            }
            return;
        }
    }

    // 5. Direct Mouse / Touch inside any 320x218 MPC Screen Tile
    int mx = 0, my = 0;
    const int tileIdx = hitTileScreen (x, y, mx, my);
    if (tileIdx < 0 || !p_->innerTiles)
        return;

    focusedTile_ = tileIdx;
    const bool shift = ((mods & kModShift) != 0) || p_->screenShift;

    if (p_->innerTiles->focus)
        p_->innerTiles->focus (p_->inner, (uint32_t) tileIdx, mx, my, shift);

    hdslave_tile_hotspot_t spot {};
    if (!p_->innerTiles->hit_test || !p_->innerTiles->hit_test (p_->inner, (uint32_t) tileIdx, mx, my, &spot)
        || spot.kind == HDSLAVE_HOTSPOT_NONE)
        return;

    const int pIdx = (spot.param_id != CLAP_INVALID_ID) ? p_->findParamIndex (spot.param_id) : -1;
    const int pIdxY = (spot.param_id_y != CLAP_INVALID_ID) ? p_->findParamIndex (spot.param_id_y) : -1;

    if (pIdx >= 0)
    {
        lastTouched_ = pIdx;
        // If MIDI Learn is armed waiting for a parameter pick, bind this parameter
        if (p_->midiLearnTarget.load (std::memory_order_relaxed) == DesktopPlugin::kLearnArmedNoParam)
            p_->midiLearnTarget.store (spot.param_id, std::memory_order_release);
    }

    // Right-click on a parameter opens its quick context menu (Reset / MIDI Learn / Choices)
    if (button == 1 && pIdx >= 0 && spot.kind == HDSLAVE_HOTSPOT_PARAM)
    {
        openMenu (pIdx, x, y);
        return;
    }

    // Double-click or Alt/Cmd-click on a parameter resets it to default
    if (pIdx >= 0 && (clicks >= 2 || (mods & (kModAlt | kModCmd)) != 0) && spot.kind == HDSLAVE_HOTSPOT_PARAM)
    {
        resetParam (pIdx);
        if (pIdxY >= 0) resetParam (pIdxY);
        return;
    }

    dragTile_ = tileIdx;
    dragSpot_ = spot;
    dragParam_ = pIdx;
    dragParamY_ = pIdxY;

    switch (spot.kind)
    {
        case HDSLAVE_HOTSPOT_BUTTON:
            if (pIdx >= 0)
            {
                const auto& pr = p_->params[(size_t) pIdx];
                p_->setValueOnce (pIdx, pr.maxVal);
            }
            break;

        case HDSLAVE_HOTSPOT_ACTION:
            if (p_->innerTiles->action)
                p_->innerTiles->action (p_->inner, (uint32_t) tileIdx, spot.custom_id, 0, shift || button == 1);
            break;

        case HDSLAVE_HOTSPOT_NOTE_PAD:
            activeNote_ = std::clamp ((int) spot.custom_id, 0, 127);
            p_->sendNote (activeNote_, true, 0.85);
            drag_ = Drag::NotePad;
            break;

        case HDSLAVE_HOTSPOT_BIT_TOGGLE:
            if (pIdx >= 0)
            {
                const int bitIdx = std::clamp ((int) spot.custom_id, 0, 15);
                const int mask = (int) std::lround (p_->displayValue (pIdx));
                bitPaintValue_ = ((mask & (1 << bitIdx)) == 0);
                const int nextMask = bitPaintValue_ ? (mask | (1 << bitIdx)) : (mask & ~(1 << bitIdx));
                p_->beginEdit (pIdx);
                setParam (pIdx, double (nextMask));
                drag_ = Drag::BitPaint;
            }
            break;

        case HDSLAVE_HOTSPOT_PARAM:
            if (pIdx >= 0)
            {
                p_->beginEdit (pIdx);
                dragStartNorm_ = norm (pIdx);
                dragAccum_ = 0.0;
                if (pIdxY >= 0)
                {
                    p_->beginEdit (pIdxY);
                    dragStartNormY_ = norm (pIdxY);
                }
                if (spot.is_absolute)
                {
                    drag_ = Drag::ParamAbsolute;
                    applyAbsoluteSpot (tileIdx, spot, x, y);
                }
                else if (pIdxY >= 0)
                {
                    drag_ = Drag::Param2D;
                }
                else
                {
                    drag_ = Drag::Param1D;
                }
            }
            break;

        default:
            break;
    }
}

void PanelUI::mouseMove (float x, float y, uint32_t mods)
{
    if (popup_ != PopupKind::None)
    {
        popupHover_ = -1;
        for (size_t i = 0; i < popupRects_.size(); ++i)
            if (popupRects_[i].contains (int (x), int (y)))
                popupHover_ = int (i);
        return;
    }

    if (drag_ != Drag::None)
    {
        const float dx = x - lastX_;
        const float dy = y - lastY_;
        lastX_ = x;
        lastY_ = y;
        if (std::fabs (x - downX_) > 2.5f || std::fabs (y - downY_) > 2.5f)
            moved_ = true;

        const bool fine = ((mods & kModShift) != 0) || p_->screenShift || (dragButton_ == 1);
        const double sens = fine ? 950.0 : 170.0;

        if (drag_ == Drag::Param1D && dragParam_ >= 0)
        {
            const double delta = (double (-dy) + double (dx) * 0.45) / sens;
            dragStartNorm_ = std::clamp (dragStartNorm_ + delta, 0.0, 1.0);
            setParam (dragParam_, fromNorm (dragParam_, dragStartNorm_));
        }
        else if (drag_ == Drag::Param2D && dragParam_ >= 0 && dragParamY_ >= 0)
        {
            const double deltaX = double (dx) / sens;
            const double deltaY = double (dragSpot_.invert_y ? -dy : dy) / sens;
            dragStartNorm_ = std::clamp (dragStartNorm_ + deltaX, 0.0, 1.0);
            dragStartNormY_ = std::clamp (dragStartNormY_ + deltaY, 0.0, 1.0);
            setParam (dragParam_, fromNorm (dragParam_, dragStartNorm_));
            setParam (dragParamY_, fromNorm (dragParamY_, dragStartNormY_));
        }
        else if (drag_ == Drag::ParamAbsolute)
        {
            applyAbsoluteSpot (dragTile_, dragSpot_, x, y);
        }
        else if (drag_ == Drag::BitPaint && dragParam_ >= 0)
        {
            int mx = 0, my = 0;
            const int tIdx = hitTileScreen (x, y, mx, my);
            if (tIdx == dragTile_ && p_->innerTiles && p_->innerTiles->hit_test)
            {
                hdslave_tile_hotspot_t sp {};
                if (p_->innerTiles->hit_test (p_->inner, (uint32_t) tIdx, mx, my, &sp)
                    && sp.kind == HDSLAVE_HOTSPOT_BIT_TOGGLE && sp.param_id == dragSpot_.param_id)
                {
                    const int bitIdx = std::clamp ((int) sp.custom_id, 0, 15);
                    const int mask = (int) std::lround (p_->displayValue (dragParam_));
                    const int nextMask = bitPaintValue_ ? (mask | (1 << bitIdx)) : (mask & ~(1 << bitIdx));
                    if (nextMask != mask)
                        setParam (dragParam_, double (nextMask));
                }
            }
        }
        else if (drag_ == Drag::NotePad)
        {
            int mx = 0, my = 0;
            const int tIdx = hitTileScreen (x, y, mx, my);
            if (tIdx == dragTile_ && p_->innerTiles && p_->innerTiles->hit_test)
            {
                hdslave_tile_hotspot_t sp {};
                if (p_->innerTiles->hit_test (p_->inner, (uint32_t) tIdx, mx, my, &sp)
                    && sp.kind == HDSLAVE_HOTSPOT_NOTE_PAD)
                {
                    const int newNote = std::clamp ((int) sp.custom_id, 0, 127);
                    if (newNote != activeNote_)
                    {
                        if (activeNote_ >= 0) p_->sendNote (activeNote_, false, 0.0);
                        activeNote_ = newNote;
                        p_->sendNote (activeNote_, true, 0.85);
                        dragSpot_ = sp;
                    }
                }
            }
        }
        return;
    }

    // Hover tracking
    hoverItem_ = hitItem (x, y);
    hoverZoomBtnTile_ = -1;
    hoverTabTile_ = -1;
    for (size_t t = 0; t < tiles_.size(); ++t)
    {
        if (zoomedTile_ >= 0 && tiles_[t].tabRect.contains (int (x), int (y)))
            hoverTabTile_ = (int) t;
        if (zoomedTile_ >= 0 && (int) t != zoomedTile_)
            continue;
        if (tiles_[t].zoomBtnRect.contains (int (x), int (y)))
            hoverZoomBtnTile_ = (int) t;
    }

    int mx = 0, my = 0;
    hoverTile_ = hitTileScreen (x, y, mx, my);
    hoverSpot_ = {};
    hoverSpot_.param_id = CLAP_INVALID_ID;
    hoverSpot_.param_id_y = CLAP_INVALID_ID;
    hoverParam_ = -1;

    if (hoverTile_ >= 0 && p_->innerTiles && p_->innerTiles->hit_test)
    {
        if (p_->innerTiles->hit_test (p_->inner, (uint32_t) hoverTile_, mx, my, &hoverSpot_))
        {
            if (hoverSpot_.param_id != CLAP_INVALID_ID)
                hoverParam_ = p_->findParamIndex (hoverSpot_.param_id);
        }
    }
}

void PanelUI::mouseUp (float, float, int, uint32_t mods)
{
    if (drag_ == Drag::None)
        return;

    const bool shift = ((mods & kModShift) != 0) || ((downMods_ & kModShift) != 0) || p_->screenShift;

    // If the user clicked a boolean or discrete choice cell without dragging, step/toggle it immediately!
    if (drag_ == Drag::Param1D && !moved_ && dragParam_ >= 0 && dragParam_ < int (p_->params.size()))
    {
        const auto& pr = p_->params[(size_t) dragParam_];
        if (pr.isBool())
        {
            const double cur = p_->displayValue (dragParam_);
            setParam (dragParam_, cur >= 0.5 ? 0.0 : 1.0);
        }
        else if (pr.isStepped() && pr.range() <= 32.0)
        {
            const double cur = std::round (p_->displayValue (dragParam_));
            const double span = pr.range() + 1.0;
            const double dir = shift ? -1.0 : 1.0;
            double next = cur + dir;
            if (next > pr.maxVal) next = pr.minVal;
            if (next < pr.minVal) next = pr.maxVal;
            (void) span;
            setParam (dragParam_, next);
        }
    }

    if (drag_ == Drag::NotePad && activeNote_ >= 0)
    {
        p_->sendNote (activeNote_, false, 0.0);
        activeNote_ = -1;
    }

    if (dragParam_ >= 0 && (drag_ == Drag::Param1D || drag_ == Drag::Param2D || drag_ == Drag::ParamAbsolute || drag_ == Drag::BitPaint))
        p_->endEdit (dragParam_);
    if (dragParamY_ >= 0 && (drag_ == Drag::Param2D || drag_ == Drag::ParamAbsolute))
        p_->endEdit (dragParamY_);

    drag_ = Drag::None;
    dragTile_ = -1;
    dragParam_ = -1;
    dragParamY_ = -1;
    dragSpot_ = {};
}

void PanelUI::wheel (float x, float y, float, float dy, bool precise, uint32_t mods)
{
    if (std::fabs (dy) < 1e-4f || popup_ != PopupKind::None)
        return;

    int mx = 0, my = 0;
    const int tileIdx = hitTileScreen (x, y, mx, my);
    if (tileIdx < 0 || !p_->innerTiles || !p_->innerTiles->hit_test)
        return;

    focusedTile_ = tileIdx;
    const bool shift = ((mods & kModShift) != 0) || p_->screenShift;

    hdslave_tile_hotspot_t spot {};
    if (!p_->innerTiles->hit_test (p_->inner, (uint32_t) tileIdx, mx, my, &spot) || spot.kind == HDSLAVE_HOTSPOT_NONE)
        return;

    wheelAccum_ += precise ? (dy * 0.15f) : dy;
    const int steps = (int) std::trunc (wheelAccum_);
    if (steps == 0)
        return;
    wheelAccum_ -= float (steps);

    if (spot.kind == HDSLAVE_HOTSPOT_ACTION && p_->innerTiles->action)
    {
        p_->innerTiles->action (p_->inner, (uint32_t) tileIdx, spot.custom_id, steps, shift);
        return;
    }

    if (spot.param_id != CLAP_INVALID_ID)
    {
        const int pIdx = p_->findParamIndex (spot.param_id);
        if (pIdx >= 0)
        {
            if (p_->innerTiles->focus)
                p_->innerTiles->focus (p_->inner, (uint32_t) tileIdx, mx, my, shift);
            stepParam (pIdx, steps);
        }
    }
}

void PanelUI::rotate (float x, float y, float degrees)
{
    wheel (x, y, 0.0f, degrees * 0.25f, true, 0);
}

void PanelUI::mouseExit()
{
    hoverItem_ = Item::Count;
    hoverTile_ = -1;
    hoverZoomBtnTile_ = -1;
    hoverTabTile_ = -1;
    hoverSpot_ = {};
    hoverParam_ = -1;
}

bool PanelUI::key (int k)
{
    if (k == 5) // Escape
    {
        if (popup_ != PopupKind::None)
        {
            closePopup();
            return true;
        }
        if (zoomedTile_ >= 0)
        {
            zoomedTile_ = -1;
            layoutDirty_ = true;
            return true;
        }
    }
    const int target = hoverParam_ >= 0 ? hoverParam_ : lastTouched_;
    if (target >= 0)
    {
        if (k == 0 || k == 3) { stepParam (target, -1); return true; }
        if (k == 1 || k == 2) { stepParam (target, +1); return true; }
    }
    return false;
}

std::string PanelUI::valueText (int param) const
{
    if (param < 0 || param >= int (p_->params.size())) return "";
    const auto& pr = p_->params[(size_t) param];
    return p_->formatParamValue (pr.id, p_->displayValue (param));
}

std::string PanelUI::readoutText (bool& learnLine) const
{
    learnLine = false;
    const uint32_t learnTgt = p_->midiLearnTarget.load (std::memory_order_relaxed);
    if (learnTgt != CLAP_INVALID_ID)
    {
        learnLine = true;
        if (learnTgt == DesktopPlugin::kLearnArmedNoParam)
            return "MIDI LEARN: CLICK ANY PARAMETER ON ANY SCREEN";
        const int idx = p_->findParamIndex (learnTgt);
        if (idx >= 0)
            return "WAIT CC -> " + upper (p_->params[(size_t) idx].name);
    }

    const int activeParam = (dragParam_ >= 0) ? dragParam_ : ((hoverParam_ >= 0) ? hoverParam_ : lastTouched_);
    if (activeParam >= 0 && activeParam < int (p_->params.size()))
    {
        const auto& pr = p_->params[(size_t) activeParam];
        std::string line;
        if (!pr.module.empty())
            line += upper (pr.module) + " / ";
        line += upper (pr.name) + ": " + valueText (activeParam);

        if (dragParamY_ >= 0 && dragParamY_ < int (p_->params.size()))
        {
            const auto& prY = p_->params[(size_t) dragParamY_];
            line += "  |  " + upper (prY.name) + ": " + valueText (dragParamY_);
        }

        const int cc = p_->findCcForParam (pr.id);
        if (cc >= 0)
        {
            char ccBuf[16];
            std::snprintf (ccBuf, sizeof (ccBuf), " [CC %d]", cc);
            line += ccBuf;
        }
        return line;
    }
    return "CLICK, DRAG, OR SCROLL ANY PARAMETER ON ANY MPC SCREEN";
}

// Popups (Presets / Choices / Right-click Parameter Menu) -----------------------------------------------------

void PanelUI::openPresets()
{
    if (p_->presets.empty()) return;
    popup_ = PopupKind::Presets;
    popupParam_ = -1;
    popupItems_.clear();
    popupValues_.clear();
    for (size_t i = 0; i < p_->presets.size(); ++i)
    {
        char num[12];
        std::snprintf (num, sizeof (num), "%02zu. ", i + 1);
        popupItems_.push_back (std::string (num) + p_->presets[i].name);
        popupValues_.push_back (double (i));
    }
    popupSelected_ = p_->currentPresetIndex;
    layoutPopup (items_[(size_t) Item::PresetBox]);
}

void PanelUI::openChoices (int param, const Rect& anchor)
{
    if (param < 0 || param >= int (p_->params.size())) return;
    const auto& pr = p_->params[(size_t) param];
    const int count = int (std::round (pr.range())) + 1;
    if (count < 2 || count > 64) return;

    popup_ = PopupKind::Choices;
    popupParam_ = param;
    popupItems_.clear();
    popupValues_.clear();
    const int cur = int (std::round (p_->displayValue (param)));
    popupSelected_ = -1;
    for (int i = 0; i < count; ++i)
    {
        const double v = pr.minVal + double (i);
        popupItems_.push_back (p_->formatParamValue (pr.id, v));
        popupValues_.push_back (v);
        if (int (std::round (v)) == cur)
            popupSelected_ = i;
    }
    layoutPopup (anchor);
}

void PanelUI::openMenu (int param, float x, float y)
{
    if (param < 0 || param >= int (p_->params.size())) return;
    const auto& pr = p_->params[(size_t) param];
    popup_ = PopupKind::Menu;
    popupParam_ = param;
    popupItems_.clear();
    popupValues_.clear();

    popupItems_.push_back ("RESET TO DEFAULT (" + p_->formatParamValue (pr.id, pr.defVal) + ")");
    popupValues_.push_back (0);

    const int cc = p_->findCcForParam (pr.id);
    if (cc >= 0)
    {
        char buf[48];
        std::snprintf (buf, sizeof (buf), "CLEAR MIDI CC %d", cc);
        popupItems_.push_back (buf);
        popupValues_.push_back (2);
    }
    popupItems_.push_back ("MIDI LEARN CC FOR " + upper (pr.name));
    popupValues_.push_back (1);

    if (pr.isStepped() && pr.range() >= 1.0 && pr.range() <= 48.0)
    {
        popupItems_.push_back ("CHOOSE VALUE FROM LIST...");
        popupValues_.push_back (3);
    }
    popupSelected_ = -1;
    layoutPopup ({ int (x), int (y), 12, 12 });
}

void PanelUI::closePopup()
{
    popup_ = PopupKind::None;
    popupParam_ = -1;
    popupItems_.clear();
    popupValues_.clear();
    popupRects_.clear();
    popupHover_ = -1;
}

void PanelUI::layoutPopup (const Rect& anchor)
{
    const int n = int (popupItems_.size());
    if (n == 0) { closePopup(); return; }
    const int maxRows = 14;
    const int cols = (n + maxRows - 1) / maxRows;
    const int rows = (n + cols - 1) / cols;
    int colW = 170;
    for (const auto& s : popupItems_)
        colW = std::max (colW, Canvas::textWidth (s) + 20);
    colW = std::min (colW, 250);
    const int rowH = 22;
    const int pw = cols * colW + 12;
    const int ph = rows * rowH + 12;
    int px = std::clamp (anchor.x, 8, std::max (8, w_ - pw - 8));
    int py = anchor.y + anchor.h + 4;
    if (py + ph > h_ - 8)
        py = std::max (8, anchor.y - ph - 4);

    popupRect_ = { px, py, pw, ph };
    popupRects_.resize ((size_t) n);
    for (int i = 0; i < n; ++i)
    {
        const int c = i / rows;
        const int r = i % rows;
        popupRects_[(size_t) i] = { px + 6 + c * colW, py + 6 + r * rowH, colW - 2, rowH - 2 };
    }
}

void PanelUI::popupPick (int i)
{
    if (i < 0 || i >= int (popupValues_.size())) { closePopup(); return; }
    const PopupKind k = popup_;
    const int param = popupParam_;
    const double val = popupValues_[(size_t) i];
    const Rect anchor = popupRect_;
    closePopup();

    if (k == PopupKind::Presets)
    {
        p_->loadPresetByIndex (int (val));
    }
    else if (k == PopupKind::Choices && param >= 0)
    {
        p_->setValueOnce (param, val);
        lastTouched_ = param;
    }
    else if (k == PopupKind::Menu && param >= 0)
    {
        const auto& pr = p_->params[(size_t) param];
        if (int (val) == 0) resetParam (param);
        else if (int (val) == 1) p_->midiLearnTarget.store (pr.id, std::memory_order_release);
        else if (int (val) == 2) p_->clearCcForParam (pr.id);
        else if (int (val) == 3) openChoices (param, { anchor.x, anchor.y, 20, 20 });
    }
}

void PanelUI::drawPopup()
{
    const auto& th = p_->theme;
    c_.fill (popupRect_.inset (-2), 0xFF000000u, 150);
    c_.fillChamfer (popupRect_, 4, 0xFF0A0D14u);
    c_.frameChamfer (popupRect_, 4, th.accent);

    for (size_t i = 0; i < popupItems_.size(); ++i)
    {
        const Rect& r = popupRects_[i];
        const bool sel = (int (i) == popupSelected_);
        const bool hot = (int (i) == popupHover_);
        if (sel || hot)
            c_.fillChamfer (r, 2, hot ? th.accent : mix (th.panel, th.accent, 0.35f));
        const uint32_t fg = hot ? 0xFF06080Cu : (sel ? 0xFFFFFFFFu : th.text);
        c_.textAligned (r.inset (4), popupItems_[i], fg, 1, 0);
    }
}

} // namespace hdslave_desktop
