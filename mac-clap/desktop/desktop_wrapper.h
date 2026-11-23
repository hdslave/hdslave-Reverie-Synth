#pragma once

#include <clap/clap.h>

#include <atomic>
#include <cstdint>
#include <string>
#include <vector>

#define HDSLAVE_DESKTOP_SHELL 1
#include "hdslave_tiles.h"
#include "plugin_theme.h"

namespace hdslave_desktop
{

constexpr uint32_t kDefaultGuiWidth = 1352;
constexpr uint32_t kDefaultGuiHeight = 708;
constexpr uint32_t kMinGuiWidth = 960;
constexpr uint32_t kMinGuiHeight = 540;
constexpr uint32_t kMaxGuiWidth = 3200;
constexpr uint32_t kMaxGuiHeight = 2000;

uint64_t nowMs();

enum class UiEventKind : uint8_t
{
    GestureBegin = 0,
    Value = 1,
    GestureEnd = 2,
    NoteOn = 3,
    NoteOff = 4,
};

struct UiParamEvent
{
    UiEventKind kind;
    clap_id paramId;
    double value;
};

// Single producer (main thread) / single consumer (process or flush, never both at once) queue.
template <size_t Capacity>
class SpscQueue
{
public:
    bool push (const UiParamEvent& item)
    {
        const size_t w = writeIdx_.load (std::memory_order_relaxed);
        const size_t next = (w + 1) % Capacity;
        if (next == readIdx_.load (std::memory_order_acquire)) return false;
        buf_[w] = item;
        writeIdx_.store (next, std::memory_order_release);
        return true;
    }
    bool pop (UiParamEvent& out)
    {
        const size_t r = readIdx_.load (std::memory_order_relaxed);
        if (r == writeIdx_.load (std::memory_order_acquire)) return false;
        out = buf_[r];
        readIdx_.store ((r + 1) % Capacity, std::memory_order_release);
        return true;
    }

private:
    UiParamEvent buf_[Capacity] {};
    std::atomic<size_t> readIdx_ { 0 };
    std::atomic<size_t> writeIdx_ { 0 };
};

struct CachedParamInfo
{
    clap_id id = CLAP_INVALID_ID;
    uint32_t index = 0;
    uint32_t flags = 0;
    std::string name;
    std::string module;
    double minVal = 0.0;
    double maxVal = 1.0;
    double defVal = 0.0;

    bool isStepped() const { return (flags & CLAP_PARAM_IS_STEPPED) != 0; }
    bool isEnum() const { return (flags & CLAP_PARAM_IS_ENUM) != 0; }
    bool isBool() const { return isStepped() && minVal == 0.0 && maxVal == 1.0; }
    bool isBipolar() const { return minVal < 0.0 && maxVal > 0.0 && (minVal + maxVal) == 0.0; }
    double range() const { return maxVal - minVal; }
};

struct PresetEntry
{
    std::string name;
    std::string loadKey;
};

class PanelUI;
struct PlatformGui;
struct MergedInputEvents;

struct DesktopPlugin
{
    clap_plugin_t clap {};
    const clap_host_t* realHost = nullptr;
    clap_host_t proxyHost {}; // what the inner plug-in sees; host_data points back here
    MergedInputEvents* mergedProcess = nullptr;
    MergedInputEvents* mergedFlush = nullptr;

    // Lifecycle / thread guards for safe FL Studio interop
    std::atomic<bool> active { false };
    std::atomic<bool> processing { false };
    std::atomic<bool> inAudioCall { false };
    std::atomic<uint64_t> lastProcessMs { 0 };
    std::atomic<uint32_t> pendingRescanFlags { 0 };
    std::atomic<bool> pendingStateDirty { false };
    bool inInternalPresetLoad = false;
    bool inScreenInput = false;
    clap_id openGestureParamId = CLAP_INVALID_ID;

    const clap_plugin_t* inner = nullptr;
    const clap_plugin_params_t* innerParams = nullptr;
    const clap_plugin_state_t* innerState = nullptr;
    const clap_plugin_preset_load_t* innerPresetLoad = nullptr;
    const clap_plugin_melodee_screen_t* innerScreen = nullptr;
    const clap_plugin_hdslave_tiles_t* innerTiles = nullptr;

    // The real host's extensions, always called with realHost.
    const clap_host_params_t* hostParams = nullptr;
    const clap_host_preset_load_t* hostPresetLoad = nullptr;
    const clap_host_state_t* hostState = nullptr;
    const clap_host_log_t* hostLog = nullptr;
    const clap_host_thread_check_t* hostThreadCheck = nullptr;
    bool hostExtsResolved = false;
    void resolveHostExtensions();

    PluginTheme theme {};
    std::string pluginId;
    std::string pluginName;
    std::string pluginVersion;

    std::vector<CachedParamInfo> params;
    int findParamIndex (clap_id id) const;

    std::vector<PresetEntry> presets;
    int currentPresetIndex = -1;

    // MIDI CC map: CC number -> param id (CLAP_INVALID_ID when unmapped).
    std::atomic<uint32_t> ccMap[128];
    static constexpr uint32_t kLearnArmedNoParam = 0xFFFFFFFEu;
    std::atomic<uint32_t> midiLearnTarget { CLAP_INVALID_ID };
    std::atomic<int> lastMidiCc { -1 };

    SpscQueue<2048> uiToAudio;

    // What the panel shows while the user drags: the value it sent, until the plug-in has caught up. Main thread.
    std::vector<double> uiValue;
    std::vector<uint64_t> uiHoldUntil;

    // The live MPC pages (single active page + all multi-screen tiles).
    std::vector<uint32_t> screenPixels;
    uint32_t tileCount = 0;
    int tileCols = 3;
    int tileRows = 2;
    int tileScreenH = MELODEE_SCREEN_HEIGHT;
    std::vector<std::vector<uint32_t>> tilePixels;
    clap_id tileKnobIds[16][3] {};
    bool screenWholeNeeded = true;
    std::atomic<bool> screenRequested { true };
    std::atomic<bool> transportPlaying { false };
    clap_id screenKnobIds[3] { CLAP_INVALID_ID, CLAP_INVALID_ID, CLAP_INVALID_ID };
    bool screenShift = false;

    PanelUI* ui = nullptr;
    PlatformGui* platformGui = nullptr;
    uint32_t guiWidth = kDefaultGuiWidth;
    uint32_t guiHeight = kDefaultGuiHeight;

    DesktopPlugin();
    ~DesktopPlugin();

    // Main-thread helpers for the panel.
    double getParamValue (clap_id id) const;
    double displayValue (int index) const; // the drag value while held, else the plug-in's
    std::string formatParamValue (clap_id id, double value) const;
    void beginEdit (int index);
    void setEdit (int index, double value);
    void endEdit (int index);
    void setValueOnce (int index, double value); // begin + value + end
    bool loadPresetByIndex (int idx);
    int findCcForParam (clap_id id) const;
    void clearCcForParam (clap_id id);
    void clearAllCc();
    int countMappedCc() const;
    void sendScreenInput (uint32_t kind, int32_t value);
    void sendNote (int midiNote, bool on, double velocity = 0.85);
    bool paintScreen (uint64_t now); // true when the page changed
    bool paintAllTiles (uint64_t now); // paints all MPC screen tiles
    void refreshScreenKnobs();
    void markAllDirty();

private:
    void enqueue (UiEventKind kind, clap_id id, double value);
};

// Platform GUI (gui_macos.mm / gui_win32.cpp).
PlatformGui* createPlatformGui (DesktopPlugin* plugin);
void destroyPlatformGui (PlatformGui* gui);
bool platformGuiSetParent (PlatformGui* gui, const clap_window_t* window);
bool platformGuiShow (PlatformGui* gui);
bool platformGuiHide (PlatformGui* gui);
bool platformGuiSetSize (PlatformGui* gui, uint32_t width, uint32_t height);
const char* platformGuiApiName();

} // namespace hdslave_desktop

// Test hook (tests/host_test.cpp): drives the panel off-screen to render snapshots and simulate input.
extern "C"
{
typedef struct hdslave_desktop_debug_api
{
    uint32_t version;
    bool (*ui_open) (const clap_plugin_t* plugin);
    void (*ui_close) (const clap_plugin_t* plugin);
    // Renders the whole panel at logical w x h and device scale; returns the physical pixels (owned by the panel).
    const uint32_t* (*ui_render) (const clap_plugin_t* plugin, int w, int h, int scale, int* physW, int* physH);
    // kind: 0 down, 1 move/drag, 2 up, 3 wheel. mods: bit0 shift, bit1 alt/cmd (fine / reset).
    void (*ui_mouse) (const clap_plugin_t* plugin, int kind, float x, float y, int button, uint32_t mods, float delta, int clicks);
    int (*preset_count) (const clap_plugin_t* plugin);
    bool (*preset_load) (const clap_plugin_t* plugin, int index);
    double (*ms_per_full_render) (const clap_plugin_t* plugin, int w, int h, int scale);
} hdslave_desktop_debug_api_t;
}
