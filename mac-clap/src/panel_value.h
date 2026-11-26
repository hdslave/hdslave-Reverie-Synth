// Publish a panel edit to project state immediately. Its queued audio event retires
// the matching publication without overwriting subsequent edits or a restored state.
#pragma once

#include <atomic>
#include <cstdint>
#include <cstring>

namespace reverie
{
class ParameterValue
{
    static_assert (std::atomic<uint64_t>::is_always_lock_free);
    std::atomic<uint64_t> bits { 0 };

    static uint32_t valueBits (float value)
    {
        uint32_t raw;
        std::memcpy (&raw, &value, sizeof raw);
        return raw;
    }
    static uint64_t panelBits (float value, uint64_t sequence)
    {
        const uint64_t ticket = 1 + sequence % UINT32_MAX;
        return (ticket << 32) | valueBits (value);
    }

public:
    float load (std::memory_order order) const
    {
        const uint32_t raw = uint32_t (bits.load (order));
        float value;
        std::memcpy (&value, &raw, sizeof value);
        return value;
    }
    void store (float value, std::memory_order order) { bits.store (valueBits (value), order); }
    void publish (float value, uint64_t sequence) { bits.store (panelBits (value, sequence), std::memory_order_release); }
    void acknowledge (float value, uint64_t sequence)
    {
        auto expected = panelBits (value, sequence);
        bits.compare_exchange_strong (expected, valueBits (value), std::memory_order_relaxed);
    }
    void fromHost (float value)
    {
        auto previous = bits.load (std::memory_order_relaxed);
        if ((previous >> 32) == 0)
            bits.compare_exchange_strong (previous, valueBits (value), std::memory_order_relaxed);
    }
};
}
