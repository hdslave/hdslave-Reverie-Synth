// Private note address for mouse auditions between the desktop panel and its own synth.
#pragma once

#include <clap/clap.h>

namespace hdslave_desktop
{
struct PanelNote
{
    static constexpr int32_t id = -2;
    clap_event_note_t note;
    const clap_plugin_t* owner;

    static bool validId (const clap_event_note_t& n, const clap_plugin_t* instance)
    {
        if (n.note_id >= -1) return true;
        return n.note_id == id && n.header.size == sizeof (PanelNote) && n.header.flags == CLAP_EVENT_IS_LIVE &&
               (n.header.type == CLAP_EVENT_NOTE_ON || n.header.type == CLAP_EVENT_NOTE_OFF) &&
               n.port_index == 0 && n.channel == 0 && n.key >= 0 && n.key < 128 &&
               reinterpret_cast<const PanelNote*> (&n)->owner == instance;
    }
};
}
