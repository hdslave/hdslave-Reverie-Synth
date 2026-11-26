#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <clap/clap.h>

bool checkEditorFocus (const clap_plugin_t* plugin, const clap_plugin_gui_t* gui)
{
    HWND host = CreateWindowW (L"STATIC", L"Reverie focus test", WS_POPUP,
                              0, 0, 960, 648, nullptr, nullptr, GetModuleHandleW (nullptr), nullptr);
    if (!host) return false;
    if (!gui->create (plugin, CLAP_WINDOW_API_WIN32, false)) { DestroyWindow (host); return false; }
    clap_window_t parent {};
    parent.api = CLAP_WINDOW_API_WIN32;
    parent.win32 = host;
    bool ok = gui->set_parent (plugin, &parent) && gui->show (plugin);
    HWND panel = GetWindow (host, GW_CHILD);
    SetFocus (host);
    ok = ok && panel && GetFocus() == host;
    const UINT clicks[] = { WM_LBUTTONDOWN, WM_LBUTTONUP, WM_RBUTTONDOWN, WM_RBUTTONUP };
    for (UINT message : clicks)
    {
        if (!ok) break;
        SendMessageW (panel, message, 0, MAKELPARAM (880, 18));
        ok = GetFocus() == host;
    }
    ok = ok && GetNextDlgTabItem (host, nullptr, FALSE) == panel;
    gui->destroy (plugin);
    DestroyWindow (host);
    return ok;
}
