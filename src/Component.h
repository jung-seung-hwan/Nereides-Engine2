#pragma once

class GameObject;

// Component는 GameObject를 참조한다. 소유하거나 삭제하지 않는다.
class Component
{
public:
    explicit Component(GameObject& owner) : m_owner(owner) {}
    virtual ~Component() = default;
    virtual void Update() {}

    GameObject& GetOwner() const { return m_owner; }
    bool IsEnabled() const { return m_enabled; }
    void SetEnabled(bool enabled) { m_enabled = enabled; }

private:
    GameObject& m_owner;
    bool m_enabled = true;
};
