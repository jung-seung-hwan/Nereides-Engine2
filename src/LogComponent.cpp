#include "LogComponent.h"
#include "GameObject.h"
#include <iostream>

LogComponent::LogComponent(GameObject& owner) : Component(owner)
{
    std::cout << "  Create LogComponent: " << GetOwner().GetName() << '\n';
}

LogComponent::~LogComponent()
{
    std::cout << "  Destroy LogComponent: " << GetOwner().GetName() << '\n';
}

void LogComponent::Update()
{
    std::cout << "    Update " << GetOwner().GetName()
              << " position.x=" << GetOwner().transform.position.x << '\n';
}
