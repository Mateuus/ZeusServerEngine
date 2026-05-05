#pragma once

#include "ActorTick.hpp"

#include <string>

namespace Zeus::World
{
class Actor;

class ActorComponent
{
public:
    ActorComponent();
    virtual ~ActorComponent();

    Actor* GetOwner() const { return Owner; }
    void SetOwner(Actor* owner);

    const std::string& GetName() const { return Name; }
    void SetName(std::string name);

    virtual void OnRegister();
    virtual void BeginPlay();
    virtual void TickComponent(double deltaSeconds);
    virtual void EndPlay();
    virtual void OnUnregister();

    bool IsTickEnabled() const;
    void SetTickEnabled(bool enabled);

    ActorTickFunction& GetPrimaryComponentTick() { return PrimaryComponentTick; }
    const ActorTickFunction& GetPrimaryComponentTick() const { return PrimaryComponentTick; }

protected:
    Actor* Owner = nullptr;
    std::string Name;
    ActorTickFunction PrimaryComponentTick;
};
} // namespace Zeus::World
