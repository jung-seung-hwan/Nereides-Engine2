#pragma once
#include "Component.h"

// 입력과 시간을 읽어 Player의 Transform을 바꾸는 행동 Component.
// 생성자에는 이동 속도처럼 만들 때 정해지는 값만 받는다.
// 읽는 것: Input의 이동·공격·마우스 가로 이동, Time의 tick 길이. 소유자와 Scene을 거쳐 읽는다.
class PlayerControl : public Component
{
public:
    PlayerControl(GameObject& owner, float speed);
    void Update() override;
    int AttackCount() const { return m_attackCount; }

private:
    float m_speed; // 초당 이동 거리
    int m_attackCount = 0;
};
