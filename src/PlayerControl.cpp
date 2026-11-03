#include "PlayerControl.h"
#include "GameObject.h"
#include "Input.h"
#include "Time.h"
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

    // tick 길이는 배속과 관계없이 항상 같다. 배속은 tick 수를 바꾼다.
    const float step = m_speed * static_cast<float>(m_time.TickSeconds());
    if (m_input.IsHeld(Action::MoveForward))
        transform.position.z += step;
    if (m_input.IsHeld(Action::MoveBack))
        transform.position.z -= step;
    if (m_input.IsHeld(Action::MoveRight))
        transform.position.x += step;
    if (m_input.IsHeld(Action::MoveLeft))
        transform.position.x -= step;

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
