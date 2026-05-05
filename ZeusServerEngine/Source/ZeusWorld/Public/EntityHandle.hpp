#pragma once

#include "EntityId.hpp"

namespace Zeus::World
{
struct EntityHandle
{
    EntityId Id = InvalidEntityId;

    bool IsValid() const { return Id != InvalidEntityId; }
    void Reset() { Id = InvalidEntityId; }
};
} // namespace Zeus::World
