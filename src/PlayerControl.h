#pragma once
#include "Component.h"

class Input;
class Time;

// 입력과 시간을 읽어 Player의 Transform을 바꾸는 행동 Component.
// Input과 Time은 Application이 소유한다. 여기서는 읽기만 하는 참조다.
class PlayerControl : public Component
{
public:
    PlayerControl(GameObject& owner, const Input& input, const Time& time, float speed);
    void Update() override;
    int AttackCount() const { return m_attackCount; }

private:
    const Input& m_input;
    const Time& m_time;
    float m_speed; // 초당 이동 거리
    int m_attackCount = 0;
};
