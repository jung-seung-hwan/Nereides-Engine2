#include "CameraComponent.h"
#include <cmath>
#include <stdexcept>

CameraComponent::CameraComponent(GameObject& owner, float verticalFovDegrees, float nearPlane, float farPlane)
    : Component(owner), m_verticalFovDegrees(verticalFovDegrees), m_nearPlane(nearPlane), m_farPlane(farPlane)
{
    // NaN은 어떤 비교도 거짓이라 범위 검사를 그냥 통과한다. 무한대도 투영에 쓸 수 없다. 그래서 std::isfinite로 먼저 거른다.
    if (!std::isfinite(verticalFovDegrees) || verticalFovDegrees <= 0.0f || verticalFovDegrees >= 180.0f)
        throw std::invalid_argument("Camera field of view must be between 0 and 180 degrees.");
    if (!std::isfinite(nearPlane) || !std::isfinite(farPlane) || nearPlane <= 0.0f || farPlane <= nearPlane)
        throw std::invalid_argument("Camera planes need 0 < near < far.");
}

DirectX::XMMATRIX CameraComponent::Projection(float aspect) const
{
    // 왼손 좌표계의 원근 투영이다. 시야각은 라디안으로 바꿔 넣는다.
    return DirectX::XMMatrixPerspectiveFovLH(DirectX::XMConvertToRadians(m_verticalFovDegrees), aspect, m_nearPlane,
                                             m_farPlane);
}

DirectX::XMMATRIX CameraView(const Transform& transform)
{
    // 카메라의 회전으로 앞(+Z)과 위(+Y) 방향을 돌린 뒤, 그 위치에서 그 방향을 보는 행렬을 만든다.
    const DirectX::XMMATRIX rotation =
        DirectX::XMMatrixRotationRollPitchYaw(transform.rotation.x, transform.rotation.y, transform.rotation.z);
    const DirectX::XMVECTOR forward =
        DirectX::XMVector3TransformNormal(DirectX::XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f), rotation);
    const DirectX::XMVECTOR up = DirectX::XMVector3TransformNormal(DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f), rotation);
    const DirectX::XMVECTOR position =
        DirectX::XMVectorSet(transform.position.x, transform.position.y, transform.position.z, 1.0f);
    return DirectX::XMMatrixLookToLH(position, forward, up);
}
