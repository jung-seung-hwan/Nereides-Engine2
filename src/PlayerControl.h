#pragma once
#include "Component.h"
#include <string>

class Animator;

// 입력과 시간을 읽어 Player의 Transform을 바꾸는 행동 Component.
// 생성자에는 이동 속도와 위치 로그 간격처럼 만들 때 정해지는 값만 받는다.
// 읽는 것: Input의 이동·공격·마우스 가로 이동, Time의 FixedDeltaTime(). 소유자와 Scene을 거쳐 읽는다.
// 게임 상태(Transform, 공격 횟수)를 바꾸므로 FixedUpdate에서 일한다.
// 단계 4-2부터 같은 객체의 Animator를 가리킬 수 있다. 공격 누름을 읽으면 그 Animator에 공격 클립 재생을 요청한다.
class PlayerControl : public Component
{
public:
    // logEveryTicks: 위치 로그를 몇 tick마다 쓸지 정한다. 1이면 tick마다 쓴다. 1보다 작으면 예외를 던진다.
    // 공격한 tick은 간격과 상관없이 바로 쓴다. 이때도 정기 로그의 간격은 그대로 유지된다.
    // animator와 attackClip: 없어도 된다(nullptr, 빈 이름). 있으면 공격마다 Play(attackClip, 반복 없음)을 요청한다.
    PlayerControl(GameObject& owner, float speed, int logEveryTicks, Animator* animator = nullptr,
                  const std::string& attackClip = "");
    void FixedUpdate() override;
    int AttackCount() const { return m_attackCount; }

private:
    float m_speed; // 초당 이동 거리
    int m_logEveryTicks;
    int m_ticksSinceLog = 0;
    int m_attackCount = 0;
    Animator* m_animator;
    std::string m_attackClip;
};
