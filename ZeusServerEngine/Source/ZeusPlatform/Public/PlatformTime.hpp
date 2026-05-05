#pragma once

#include <cstdint>

namespace Zeus::Platform
{
/** Seconds from a monotonic clock (not wall clock); suitable for frame timing. */
[[nodiscard]] double NowMonotonicSeconds();

/** Millis since Unix epoch (UTC), para payloads de rede (`serverTimeMs`, etc.). */
[[nodiscard]] std::uint64_t NowUnixEpochMilliseconds();
} // namespace Zeus::Platform
