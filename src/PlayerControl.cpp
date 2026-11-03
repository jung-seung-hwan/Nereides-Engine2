#include "PlayerControl.h"
#include "GameObject.h"
#include "Input.h"
#include "Time.h"
#include <cmath>
#include <iostream>

namespace
{
// 마우스 1칸당 회전량(라디안).
const float MouseTurnPerCount = 0.01f;
}

PlayerControl::PlayerControl(GameObject& owner, const Input& input, const Time& time, float speed)
    : Component(owner), m_input(input), m_time(time), m_speed(speed)
{
}

void PlayerControl::Update()
{
    Transform& transform = GetOwner().transform;

    // 눌린 키를 방향으로 모은다. 앞과 오른쪽은 +1, 뒤와 왼쪽은 -1이다.
    float moveX = 0.0f;
    float moveZ = 0.0f;
    if (m_input.IsHeld(Action::MoveForward))
        moveZ += 1.0f;
    if (m_input.IsHeld(Action::MoveBack))
        moveZ -= 1.0f;
    if (m_input.IsHeld(Action::MoveRight))
        moveX += 1.0f;
    if (m_input.IsHeld(Action::MoveLeft))
        moveX -= 1.0f;

    // 대각선 방향의 길이는 1보다 길다(약 1.41). 길이로 나눠 어느 방향이든 같은 거리를 움직인다.
    const float length = std::sqrt(moveX * moveX + moveZ * moveZ);
    if (length > 0.0f)
    {
        // tick 길이는 배속과 관계없이 항상 같다. 배속은 tick 수를 바꾼다.
        const float step = m_speed * static_cast<float>(m_time.TickSeconds());
        transform.position.x += moveX / length * step;
        transform.position.z += moveZ / length * step;
    }

    transform.rotation.y += m_input.MouseDeltaX() * MouseTurnPerCount;

    std::cout << "      " << GetOwner().GetName()
              << " x=" << transform.position.x
              << " z=" << transform.position.z
              << " turnY=" << transform.rotation.y;

    // 누름은 tick 하나에서만 참이므로 공격은 한 번만 실행된다.
    if (m_input.IsPressed(Action::Attack))
    {
        ++m_attackCount;
        std::cout << " Attack #" << m_attackCount;
    }
    std::cout << '\n';
}
