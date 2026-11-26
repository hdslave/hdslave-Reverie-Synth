// Private input records shared only by the VST3 adapter and its embedded instrument.
// The public CLAP entry consumes the usual CLAP event structures.
#pragma once

#include <clap/clap.h>

namespace reverie
{
inline constexpr uint16_t vst3InputSpace = UINT16_MAX;

struct Vst3GlobalValue
{
    clap_event_header_t header;
    clap_id id;
    double value;
};

struct Vst3Note
{
    clap_event_header_t header;
    int32_t noteId;
    int16_t port, channel, key;
    float velocity; // The VST3 SDK supplies a float.
};
// The private records remain valid throughout process/flush. The core decodes one
// into caller-owned scratch immediately before its ordinary CLAP event handler.
union Vst3ExpandedEvent
{
    clap_event_note_t note;
    clap_event_param_value_t parameter;

    const clap_event_header_t* decode (const clap_event_header_t* input)
    {
        if (input->type == CLAP_EVENT_PARAM_VALUE && input->size == sizeof (Vst3GlobalValue))
        {
            const auto& v = *reinterpret_cast<const Vst3GlobalValue*> (input);
            parameter = {};
            parameter.header = { sizeof (parameter), input->time, CLAP_CORE_EVENT_SPACE_ID, input->type, input->flags };
            parameter.param_id = v.id;
            parameter.note_id = parameter.port_index = parameter.channel = parameter.key = -1;
            parameter.value = v.value;
            return &parameter.header;
        }
        if ((input->type == CLAP_EVENT_NOTE_ON || input->type == CLAP_EVENT_NOTE_OFF) && input->size == sizeof (Vst3Note))
        {
            const auto& n = *reinterpret_cast<const Vst3Note*> (input);
            note = {};
            note.header = { sizeof (note), input->time, CLAP_CORE_EVENT_SPACE_ID, input->type, input->flags };
            note.note_id = n.noteId;
            note.port_index = n.port;
            note.channel = n.channel;
            note.key = n.key;
            note.velocity = n.velocity;
            return &note.header;
        }
        return input;
    }
};

}
