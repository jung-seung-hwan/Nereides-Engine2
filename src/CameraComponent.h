#pragma once
#include "Component.h"
#include "Transform.h"

// 화면을 보는 시점. 위치와 회전은 붙은 GameObject의 Transform을 쓰고, 크기는 무시한다.
// 앞은 +Z, 위는 +Y다. 시야각·near·far는 생성자로 받는 이 컴포넌트의 고유 값이다.
// 카메라는 하나다. Application이 주소를 기억해서 수집 함수에 넘긴다.
class CameraComponent : public Component
{
public:
    // 세로 시야각은 도 단위로 0보다 크고 180보다 작아야 한다. 0 < near < far여야 한다. 아니면 예외를 던진다.
    CameraComponent(GameObject& owner, float verticalFovDegrees, float nearPlane, float farPlane);

    // 투영 행렬. 가까운 것은 크게, 먼 것은 작게 보이게 한다. aspect는 화면의 너비 ÷ 높이다.
    DirectX::XMMATRIX Projection(float aspect) const;

private:
    float m_verticalFovDegrees;
    float m_nearPlane;
    float m_farPlane;
};

// 카메라 객체의 Transform으로 view 행렬을 만든다. 세상을 카메라 기준 좌표로 옮기는 행렬이다.
DirectX::XMMATRIX CameraView(const Transform& transform);
