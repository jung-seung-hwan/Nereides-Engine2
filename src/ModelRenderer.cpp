#include "ModelRenderer.h"
#include <stdexcept>

ModelRenderer::ModelRenderer(GameObject& owner, const MeshData& mesh, const DirectX::XMFLOAT4& color)
    : Component(owner), m_mesh(&mesh), m_color(color)
{
    if (mesh.vertices.empty() || mesh.indices.empty())
        throw std::invalid_argument("ModelRenderer needs a mesh with vertices and indices.");
}

void ModelRenderer::AddDrawItems(RenderFrame& frame, const DirectX::XMFLOAT4X4& world) const
{
    // 메시는 복사하지 않고 주소만 담는다. 여러 객체가 같은 원본을 가리켜도 된다.
    DrawItem item;
    item.mesh = m_mesh;
    item.world = world;
    item.color = m_color;
    frame.items.push_back(item);
}
