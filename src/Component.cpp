#include "Component.h"
#include "GameObject.h"
#include "Scene.h"

// 소유자 GameObject는 자신을 소유한 Scene을 알고, Scene이 이번 tick의 입력과 시간을 알려 준다.
const Input& Component::GetInput() const
{
    return m_owner.GetScene().GetInput();
}

const Time& Component::GetTime() const
{
    return m_owner.GetScene().GetTime();
}
