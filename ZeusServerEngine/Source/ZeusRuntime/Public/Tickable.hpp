#pragma once

namespace Zeus::Runtime
{
/** Interface for future fixed-tick systems (Part 4+). */
class ITickable
{
public:
    virtual ~ITickable() = default;
    virtual void FixedTick(double fixedDeltaSeconds) = 0;
};
} // namespace Zeus::Runtime
