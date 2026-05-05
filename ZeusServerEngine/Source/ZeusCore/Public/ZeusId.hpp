#pragma once

#include "ZeusTypes.hpp"

namespace Zeus
{
/** Opaque numeric id for future entities; no world module in Part 1. */
struct EntityId
{
    u64 Value = 0;

    [[nodiscard]] bool IsValid() const { return Value != 0; }

    friend bool operator==(EntityId a, EntityId b) { return a.Value == b.Value; }
    friend bool operator!=(EntityId a, EntityId b) { return !(a == b); }
};
} // namespace Zeus
