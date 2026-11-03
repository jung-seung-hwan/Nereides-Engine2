#pragma once

class GameObject;
class Input;
class Time;

// Component는 GameObject를 참조한다. 소유하거나 삭제하지 않는다.
class Component
{
public:
    explicit Component(GameObject& owner) : m_owner(owner) {}
    // 소멸자는 Scene이 객체 목록을 정리하는 도중에 실행된다.
    // 파생 클래스의 소멸자에서는 Scene을 바꾸는 CreateObject·Clear·Update를 부르지 않는다.
    virtual ~Component() = default;
    virtual void Update() {}

    GameObject& GetOwner() const { return m_owner; }
    bool IsEnabled() const { return m_enabled; }
    void SetEnabled(bool enabled) { m_enabled = enabled; }

protected:
    // 이번 tick의 입력과 시간. 소유자 GameObject와 그 Scene을 거쳐 읽는다. 읽기 전용이다.
    // 컴포넌트가 자기 Update 안에서 쓰는 도우미라서 protected로 둔다.
    const Input& GetInput() const;
    const Time& GetTime() const;

private:
    GameObject& m_owner;
    bool m_enabled = true;
};
