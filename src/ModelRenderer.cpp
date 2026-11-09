#include "ModelRenderer.h"
#include "Animator.h"
#include <stdexcept>

ModelRenderer::ModelRenderer(GameObject& owner, const MeshData& mesh, const DirectX::XMFLOAT4& color,
                             const Animator* animator)
    : Component(owner), m_mesh(&mesh), m_color(color), m_animator(animator)
{
    if (mesh.vertices.empty() || mesh.indices.empty())
        throw std::invalid_argument("ModelRenderer needs a mesh with vertices and indices.");
    if (animator != nullptr && &animator->GetOwner() != &owner)
        throw std::invalid_argument("ModelRenderer needs an Animator from the same GameObject.");
}

void ModelRenderer::AddDrawItems(RenderFrame& frame, const DirectX::XMFLOAT4X4& world) const
{
    // 메시는 복사하지 않고 주소만 담는다. 여러 객체가 같은 원본을 가리켜도 된다.
    // 뼈 행렬은 복사한다. Animator가 tick 안에서 계산해 둔 값을 읽기만 한다.
    DrawItem item;
    item.mesh = m_mesh;
    item.world = world;
    item.color = m_color;
    if (m_animator != nullptr)
        item.bones = m_animator->SkinMatrices();
    frame.items.push_back(item);
}
