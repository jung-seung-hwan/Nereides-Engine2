#include "Input.h"
#include <stdexcept>

namespace
{
struct ActionBinding
{
    Action action;
    Key key;
};

// 행동과 키의 연결 표. 행동 하나에 키 하나만 둔다. 키 재설정은 이번 범위가 아니다.
const ActionBinding Bindings[] = {
    {Action::MoveForward, Key::W},
    {Action::MoveBack, Key::S},
    {Action::MoveLeft, Key::A},
    {Action::MoveRight, Key::D},
    {Action::Attack, Key::Space},
};

int ToIndex(Key key)
{
    const int index = static_cast<int>(key);
    if (index < 0 || index >= KeyCount)
        throw std::invalid_argument("Key is out of range.");
    return index;
}

int KeyIndexFor(Action action)
{
    for (const ActionBinding& binding : Bindings)
    {
        if (binding.action == action)
            return ToIndex(binding.key);
    }
    throw std::invalid_argument("Action has no key binding.");
}
}

void Input::OnKeyDown(Key key)
{
    const int i = ToIndex(key);
    // 이미 눌린 키의 반복 신호는 새 누름이 아니다.
    if (m_collecting.held[i])
        return;
    m_collecting.held[i] = true;
    m_collecting.pressed[i] = true;
}

void Input::OnKeyUp(Key key)
{
    const int i = ToIndex(key);
    // 포커스 상실로 이미 풀린 키의 뗌 신호는 무시한다.
    if (!m_collecting.held[i])
        return;
    m_collecting.held[i] = false;
    m_collecting.released[i] = true;
}

void Input::OnMouseMove(int dx, int dy)
{
    m_collecting.mouseX += dx;
    m_collecting.mouseY += dy;
}

void Input::OnFocusLost()
{
    // 창 밖에서 일어난 뗌은 받을 수 없으므로 눌린 상태와 모인 사건을 모두 버린다.
    m_collecting = InputSnapshot();
}

void Input::ConsumeForTick()
{
    m_tick = m_collecting;

    // 누름·뗌·마우스 변화량은 한 번만 전달한다. 눌린 상태(held)는 계속 유지한다.
    for (int i = 0; i < KeyCount; ++i)
    {
        m_collecting.pressed[i] = false;
        m_collecting.released[i] = false;
    }
    m_collecting.mouseX = 0;
    m_collecting.mouseY = 0;
}

bool Input::IsPressed(Action action) const
{
    return m_tick.pressed[KeyIndexFor(action)];
}

bool Input::IsHeld(Action action) const
{
    return m_tick.held[KeyIndexFor(action)];
}

bool Input::IsReleased(Action action) const
{
    return m_tick.released[KeyIndexFor(action)];
}
