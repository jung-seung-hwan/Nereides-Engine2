#include "Transform.h"

namespace
{
Vector3 LerpVector(const Vector3& from, const Vector3& to, float t)
{
    Vector3 result;
    result.x = from.x + (to.x - from.x) * t;
    result.y = from.y + (to.y - from.y) * t;
    result.z = from.z + (to.z - from.z) * t;
    return result;
}
}

Transform Lerp(const Transform& from, const Transform& to, float t)
{
    Transform result;
    result.position = LerpVector(from.position, to.position, t);
    result.rotation = LerpVector(from.rotation, to.rotation, t);
    result.scale = LerpVector(from.scale, to.scale, t);
    return result;
}

DirectX::XMMATRIX WorldMatrix(const Transform& transform)
{
    const Vector3& scale = transform.scale;
    const Vector3& rotation = transform.rotation;
    const Vector3& position = transform.position;
    // DirectXMath는 행 벡터 규칙이라 왼쪽 행렬부터 적용된다. 크기를 바꾸고, 돌리고, 옮긴다.
    return DirectX::XMMatrixScaling(scale.x, scale.y, scale.z) *
           DirectX::XMMatrixRotationRollPitchYaw(rotation.x, rotation.y, rotation.z) *
           DirectX::XMMatrixTranslation(position.x, position.y, position.z);
}
