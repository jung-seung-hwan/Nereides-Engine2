#pragma once
#include "RenderFrame.h"

class GameObject;
class Input;
class Time;

// Component는 GameObject를 참조한다. 소유하거나 삭제하지 않는다.
class Component
{
public:
    explicit Component(GameObject& owner) : m_owner(owner) {}
    // 소멸자는 Scene이 객체 목록을 지우는 도중에 실행된다.
    // 파생 클래스의 소멸자에서는 Scene을 바꾸는 CreateObject·Clear·Update를 부르지 않는다.
    // GetInput·GetTime으로 읽는 것은 괜찮다.
    virtual ~Component() = default;
    virtual void Update() {}

    // 그릴 것이 있는 컴포넌트만 재정의해서 frame.items에 항목을 넣는다. 기본은 아무것도 넣지 않는다.
    // 수집 함수가 모든 tick이 끝난 뒤 프레임마다 한 번 부르고, 보간한 월드 행렬을 넘긴다.
    // 읽기만 한다. 여기서 객체나 Scene을 바꾸지 않는다.
    // 기본 구현은 매개변수를 쓰지 않으므로 이름을 비운다. 이름이 있으면 "쓰지 않는 매개변수" 경고가 난다.
    virtual void AddDrawItems(RenderFrame&, const DirectX::XMFLOAT4X4&) const {}

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
