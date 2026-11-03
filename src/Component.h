#pragma once

class GameObject;
class Input;
class Time;

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

    // 이번 tick의 입력과 시간. 소유자 GameObject와 그 Scene을 거쳐 읽는다. 읽기 전용이다.
    const Input& GetInput() const;
    const Time& GetTime() const;

private:
    GameObject& m_owner;
    bool m_enabled = true;
};
