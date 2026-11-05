#include "PlayerControl.h"
#include "GameObject.h"
#include "Input.h"
#include "Time.h"
#include <cmath>
#include <iostream>
#include <stdexcept>

namespace
{
// 마우스 1칸당 회전량(라디안).
const float MouseTurnPerCount = 0.01f;
}

PlayerControl::PlayerControl(GameObject& owner, float speed, int logEveryTicks)
    : Component(owner), m_speed(speed), m_logEveryTicks(logEveryTicks)
{
    if (logEveryTicks < 1)
        throw std::invalid_argument("Log interval must be at least 1 tick.");
}

void PlayerControl::FixedUpdate()
{
    Transform& transform = GetOwner().transform;
    const Input& input = GetInput();
    const Time& time = GetTime();

    // 눌린 키를 방향으로 모은다. 앞과 오른쪽은 +1, 뒤와 왼쪽은 -1이다.
    float moveX = 0.0f;
    float moveZ = 0.0f;
    if (input.IsHeld(Action::MoveForward))
        moveZ += 1.0f;
    if (input.IsHeld(Action::MoveBack))
        moveZ -= 1.0f;
    if (input.IsHeld(Action::MoveRight))
        moveX += 1.0f;
    if (input.IsHeld(Action::MoveLeft))
        moveX -= 1.0f;

    // 대각선 방향의 길이는 1보다 길다(약 1.41). 길이로 나눠 어느 방향이든 같은 거리를 움직인다.
    const float length = std::sqrt(moveX * moveX + moveZ * moveZ);
    if (length > 0.0f)
    {
        // tick 길이는 배속과 관계없이 항상 같다. 배속은 tick 수를 바꾼다.
        const float step = m_speed * static_cast<float>(time.FixedDeltaTime());
        transform.position.x += moveX / length * step;
        transform.position.z += moveZ / length * step;
    }

    transform.rotation.y += input.MouseDeltaX() * MouseTurnPerCount;

    // 누름은 tick 하나에서만 참이므로 공격은 한 번만 실행된다.
    const bool attacked = input.IsPressed(Action::Attack);
    if (attacked)
        ++m_attackCount;

    // 위치 로그는 정한 간격마다 쓴다. 공격한 tick은 간격과 상관없이 바로 쓰지만, 정기 로그의 간격은 밀지 않는다.
    ++m_ticksSinceLog;
    const bool regularLog = m_ticksSinceLog >= m_logEveryTicks;
    if (regularLog)
        m_ticksSinceLog = 0;
    if (!regularLog && !attacked)
        return;

    std::cout << "      " << GetOwner().GetName()
              << " x=" << transform.position.x
              << " z=" << transform.position.z
              << " turnY=" << transform.rotation.y;
    if (attacked)
        std::cout << " Attack #" << m_attackCount;
    std::cout << '\n';
}
