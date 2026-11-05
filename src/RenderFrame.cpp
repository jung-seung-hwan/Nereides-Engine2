#include "RenderFrame.h"

namespace
{
// 사각형 면 하나를 메시에 더한다. center는 면의 중심, right와 up은 바깥에서 그 면을 볼 때의 오른쪽과 위쪽 방향이다.
// 왼쪽 아래 → 왼쪽 위 → 오른쪽 위 → 오른쪽 아래 순서로 정점을 놓고 삼각형 두 개로 나눈다.
// 바깥에서 보면 시계 방향이므로 앞면이 된다.
void AddFace(MeshData& mesh, const DirectX::XMFLOAT3& center, const DirectX::XMFLOAT3& right,
             const DirectX::XMFLOAT3& up, const DirectX::XMFLOAT3& normal)
{
    const std::uint32_t first = static_cast<std::uint32_t>(mesh.vertices.size());

    // 한 변이 1이므로 중심에서 오른쪽·위쪽으로 0.5씩 간다. {오른쪽 배율, 위쪽 배율}이다.
    const float corners[4][2] = {{-0.5f, -0.5f}, {-0.5f, 0.5f}, {0.5f, 0.5f}, {0.5f, -0.5f}};
    for (const auto& corner : corners)
    {
        MeshVertex vertex;
        vertex.position.x = center.x + right.x * corner[0] + up.x * corner[1];
        vertex.position.y = center.y + right.y * corner[0] + up.y * corner[1];
        vertex.position.z = center.z + right.z * corner[0] + up.z * corner[1];
        vertex.normal = normal;
        mesh.vertices.push_back(vertex);
    }

    // 삼각형 두 개: (왼쪽 아래, 왼쪽 위, 오른쪽 위)와 (왼쪽 아래, 오른쪽 위, 오른쪽 아래)
    const std::uint32_t order[6] = {0, 1, 2, 0, 2, 3};
    for (std::uint32_t index : order)
        mesh.indices.push_back(first + index);
}
}

MeshData MakeBox()
{
    MeshData mesh;
    // 면마다 중심, 바깥에서 본 오른쪽, 위쪽, 법선이다.
    AddFace(mesh, {0, 0, -0.5f}, {1, 0, 0}, {0, 1, 0}, {0, 0, -1});  // -Z 쪽 면. 카메라를 보는 면이다
    AddFace(mesh, {0, 0, 0.5f}, {-1, 0, 0}, {0, 1, 0}, {0, 0, 1});   // +Z 쪽 면
    AddFace(mesh, {0.5f, 0, 0}, {0, 0, 1}, {0, 1, 0}, {1, 0, 0});    // +X 쪽 면
    AddFace(mesh, {-0.5f, 0, 0}, {0, 0, -1}, {0, 1, 0}, {-1, 0, 0}); // -X 쪽 면
    AddFace(mesh, {0, 0.5f, 0}, {1, 0, 0}, {0, 0, 1}, {0, 1, 0});    // 윗면. 위에서 보면 +Z가 위쪽이다
    AddFace(mesh, {0, -0.5f, 0}, {1, 0, 0}, {0, 0, -1}, {0, -1, 0}); // 아랫면
    return mesh;
}

MeshData MakeFloor()
{
    MeshData mesh;
    AddFace(mesh, {0, 0, 0}, {1, 0, 0}, {0, 0, 1}, {0, 1, 0});
    return mesh;
}
