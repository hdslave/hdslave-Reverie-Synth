#pragma once

namespace reverie
{
constexpr bool supportsSampleRate(double rate)
{
    return rate >= 8000.0 && rate <= 192000.0;
}
}
