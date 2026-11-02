#pragma once
#include "Component.h"

// 학습용 동작: 자신의 객체 이름과 위치를 출력한다.
class LogComponent : public Component
{
public:
    explicit LogComponent(GameObject& owner);
    ~LogComponent() override;
    void Update() override;
};
