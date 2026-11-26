// Stable, aligned input-event storage owned by one VST3 processor. All offsets are
// collected and sorted before the synth is given any pointers into the buffer.
#pragma once

#include "vst3_input.h"

#include <algorithm>
#include <cstddef>
#include <cstring>
#include <memory>

namespace reverie
{
template <typename FullEvent>
class Vst3EventBuffer
{
public:
    void prepare (size_t extraEvents, size_t automationPoints)
    {
        static_assert (sizeof (Vst3Note) <= sizeof (Vst3GlobalValue));
        used = 0;
        reserve (extraEvents * aligned (sizeof (FullEvent)) + automationPoints * aligned (sizeof (Vst3GlobalValue)));
    }

    void clear() { used = 0; }

    size_t append (const FullEvent& event)
    {
        if (event.header.type == CLAP_EVENT_PARAM_VALUE)
        {
            const auto& p = event.param;
            const Vst3GlobalValue value { { sizeof (Vst3GlobalValue), p.header.time, vst3InputSpace,
                                            CLAP_EVENT_PARAM_VALUE, p.header.flags }, p.param_id, p.value };
            return appendBytes (&value, sizeof value);
        }
        if (event.header.type == CLAP_EVENT_NOTE_ON || event.header.type == CLAP_EVENT_NOTE_OFF)
        {
            const auto& n = event.note;
            const Vst3Note note { { sizeof (Vst3Note), n.header.time, vst3InputSpace, n.header.type, n.header.flags },
                                  n.note_id, n.port_index, n.channel, n.key, static_cast<float> (n.velocity) };
            return appendBytes (&note, sizeof note);
        }
        return appendBytes (&event, event.header.size);
    }

    const clap_event_header_t* at (size_t offset) const
    {
        return reinterpret_cast<const clap_event_header_t*> (storage.get() + offset);
    }

private:
    static constexpr size_t aligned (size_t size) { return (size + alignof (FullEvent) - 1) & ~(alignof (FullEvent) - 1); }

    size_t appendBytes (const void* source, size_t size)
    {
        const size_t offset = used;
        const size_t end = offset + aligned (size);
        if (end > capacity) reserve (std::max (end, capacity * 2));
        std::memcpy (storage.get() + offset, source, size);
        used = end;
        return offset;
    }

    void reserve (size_t size)
    {
        if (size <= capacity) return;
        std::unique_ptr<std::byte[]> larger (new std::byte[size]);
        if (used) std::memcpy (larger.get(), storage.get(), used);
        storage = std::move (larger);
        capacity = size;
    }

    std::unique_ptr<std::byte[]> storage;
    size_t used = 0, capacity = 0;
};
}
