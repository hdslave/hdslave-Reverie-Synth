// Internal event interface shared by the native panel and Reverie's own processor.
#pragma once

#include <clap/clap.h>

namespace reverie
{
struct PanelParameter
{
    clap_event_param_value_t event;
    const clap_plugin_t* owner;
    uint64_t sequence;
};

void publishPanelValue (const clap_plugin_t*, clap_id, double value, uint64_t sequence);
bool currentPanelEdit (const clap_plugin_t*, uint64_t sequence);

struct MidiMapper
{
    void* context;
    bool (*map) (void*, const clap_event_midi_t&, clap_event_param_value_t&);
};
void flushMapped (const clap_plugin_t*, const clap_input_events_t*, const clap_output_events_t*, const MidiMapper&);
clap_process_status processMapped (const clap_plugin_t*, const clap_process_t*, const MidiMapper&);
}
