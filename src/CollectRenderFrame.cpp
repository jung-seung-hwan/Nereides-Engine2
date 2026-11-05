#include "CollectRenderFrame.h"
#include "CameraComponent.h"
#include "Scene.h"
#include <stdexcept>

RenderFrame CollectRenderFrame(const Scene& scene, const CameraComponent& camera, float aspect, float alpha)
{
    if (aspect <= 0.0f)
        throw std::invalid_argument("Aspect ratio must be positive.");
    if (alpha < 0.0f || alpha > 1.0f)
        throw std::invalid_argument("Interpolation alpha must be between 0 and 1.");

    RenderFrame frame;

    // 카메라도 다른 객체처럼 보간한 Transform을 쓴다.
    const Transform cameraTransform = camera.GetOwner().InterpolatedTransform(alpha);
    DirectX::XMStoreFloat4x4(&frame.view, CameraView(cameraTransform));
    DirectX::XMStoreFloat4x4(&frame.projection, camera.Projection(aspect));

    for (std::size_t i = 0; i < scene.ObjectCount(); ++i)
    {
        const GameObject& object = scene.ObjectAt(i);
        // 비활성 객체와 삭제를 기다리는 객체는 그리지 않는다.
        if (!object.IsActive() || object.IsDestroyRequested())
            continue;

        // 월드 행렬은 객체마다 한 번 계산해서 컴포넌트에 넘긴다. 컴포넌트는 보간 계수를 몰라도 된다.
        DirectX::XMFLOAT4X4 world;
        DirectX::XMStoreFloat4x4(&world, WorldMatrix(object.InterpolatedTransform(alpha)));
        object.AddDrawItems(frame, world);
    }
    return frame;
}
