#pragma once

namespace Zeus::Platform
{
/** Seconds from a monotonic clock (not wall clock); suitable for frame timing. */
[[nodiscard]] double NowMonotonicSeconds();
} // namespace Zeus::Platform
