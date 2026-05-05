#pragma once

#include "ActorLifecycle.hpp"
#include "ActorTick.hpp"
#include "EntityId.hpp"

#include "Bounds.hpp"
#include "Transform.hpp"

#include <memory>
#include <string>
#include <vector>

namespace Zeus::World
{
class ActorComponent;
class World;

class Actor
{
public:
    Actor();
    virtual ~Actor();

    EntityId GetEntityId() const { return Id; }
    void SetEntityId(EntityId id);

    const std::string& GetName() const { return Name; }
    void SetName(std::string name);

    const Zeus::Math::Transform& GetTransform() const { return ActorTransform; }
    void SetTransform(const Zeus::Math::Transform& transform);

    Zeus::Math::Vector3 GetLocation() const;
    void SetLocation(const Zeus::Math::Vector3& location);

    Zeus::Math::Quaternion GetRotation() const;
    void SetRotation(const Zeus::Math::Quaternion& rotation);

    Zeus::Math::Vector3 GetScale3D() const;
    void SetScale3D(const Zeus::Math::Vector3& scale);

    const Zeus::Math::Aabb& GetBounds() const { return ActorBounds; }
    void SetBounds(const Zeus::Math::Aabb& bounds);

    bool IsPendingDestroy() const { return bPendingDestroy; }
    void Destroy();

    EActorLifecycleState GetLifecycleState() const { return LifecycleState; }

    virtual void OnSpawned();
    virtual void BeginPlay();
    virtual void Tick(double deltaSeconds);
    virtual void EndPlay();

    /** Tick do Actor e dos componentes com tick ativo (intervalos independentes). */
    void TickActorAndComponents(double deltaSeconds);

    void AddTag(std::string tag);
    bool HasTag(const std::string& tag) const;

    World* GetOwningWorld() const { return OwningWorld; }
    void SetOwningWorld(World* world);

    void AddComponent(std::unique_ptr<ActorComponent> component);

    template<typename TComponent>
    TComponent* FindComponent()
    {
        for (auto& c : Components)
        {
            if (c == nullptr)
            {
                continue;
            }
            if (auto* casted = dynamic_cast<TComponent*>(c.get()))
            {
                return casted;
            }
        }
        return nullptr;
    }

    template<typename TComponent>
    const TComponent* FindComponent() const
    {
        for (const auto& c : Components)
        {
            if (c == nullptr)
            {
                continue;
            }
            if (const auto* casted = dynamic_cast<const TComponent*>(c.get()))
            {
                return casted;
            }
        }
        return nullptr;
    }

    ActorTickFunction& GetPrimaryActorTick() { return PrimaryActorTick; }
    const ActorTickFunction& GetPrimaryActorTick() const { return PrimaryActorTick; }

protected:
    void SetLifecycleState(EActorLifecycleState state);

private:
    friend class EntityManager;

    void InternalBeginPlay();
    void InternalEndPlay();

    EntityId Id = InvalidEntityId;
    std::string Name;
    Zeus::Math::Transform ActorTransform = Zeus::Math::Transform::Identity;
    Zeus::Math::Aabb ActorBounds = Zeus::Math::Aabb::Empty();
    EActorLifecycleState LifecycleState = EActorLifecycleState::Uninitialized;
    ActorTickFunction PrimaryActorTick;
    std::vector<std::unique_ptr<ActorComponent>> Components;
    std::vector<std::string> Tags;
    bool bPendingDestroy = false;
    World* OwningWorld = nullptr;
};
} // namespace Zeus::World
