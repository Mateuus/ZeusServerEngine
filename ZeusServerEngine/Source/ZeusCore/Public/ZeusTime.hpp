#pragma once

namespace Zeus
{
/** Logical/simulation time step in seconds (no platform calls here). */
struct Seconds
{
    double Value = 0.0;
};

struct Milliseconds
{
    double Value = 0.0;
};

[[nodiscard]] constexpr Seconds ToSeconds(Milliseconds ms)
{
    return Seconds{ms.Value / 1000.0};
}
} // namespace Zeus
