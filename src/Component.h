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
    // 파생 클래스의 소멸자에서는 Scene을 바꾸는 CreateObject·Clear·FixedUpdate·Update를 부르지 않는다.
    // GetInput·GetTime으로 읽는 것은 괜찮다.
    virtual ~Component() = default;

    // tick마다 부른다. 한 프레임에 0번일 수도, 여러 번일 수도 있다. 유니티의 FixedUpdate와 같다.
    // 게임 상태를 바꾸는 일(이동, 공격, 나중의 애니메이션 진행과 판정)은 여기서 한다.
    // 시간은 GetTime().FixedDeltaTime()을, 입력은 tick마다 한 번 소비한 GetInput()을 읽는다.
    virtual void FixedUpdate() {}

    // 프레임마다 한 번, 모든 tick이 끝난 뒤 그리기 전에 부른다. 정지 중에도 부른다. 유니티의 Update와 같다.
    // 게임 결과에 영향을 주지 않는 화면 연출만 한다. Transform은 바꾸지 않고, 누름·뗌 입력은 읽지 않는다.
    // 시간은 GetTime().DeltaTime()을 읽는다. 정지 중에는 0이다.
    virtual void Update() {}

    // 그릴 것이 있는 컴포넌트만 재정의해서 frame.items에 항목을 넣는다. 기본은 아무것도 넣지 않는다.
    // 수집 함수가 모든 tick과 Update가 끝난 뒤 프레임마다 한 번 부르고, 보간한 월드 행렬을 넘긴다.
    // 읽기만 한다. 여기서 객체나 Scene을 바꾸지 않는다.
    // 기본 구현은 매개변수를 쓰지 않으므로 이름을 비운다. 이름이 있으면 "쓰지 않는 매개변수" 경고가 난다.
    virtual void AddDrawItems(RenderFrame&, const DirectX::XMFLOAT4X4&) const {}

    GameObject& GetOwner() const { return m_owner; }
    bool IsEnabled() const { return m_enabled; }
    void SetEnabled(bool enabled) { m_enabled = enabled; }

protected:
    // 입력과 시간. 소유자 GameObject와 그 Scene을 거쳐 읽는다. 읽기 전용이다.
    // 컴포넌트가 자기 FixedUpdate·Update 안에서 쓰는 도우미라서 protected로 둔다.
    const Input& GetInput() const;
    const Time& GetTime() const;

private:
    GameObject& m_owner;
    bool m_enabled = true;
};
