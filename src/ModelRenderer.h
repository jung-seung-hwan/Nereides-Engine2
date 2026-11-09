#pragma once
#include "Component.h"

class Animator;

// 객체를 어떤 메시로, 무슨 색으로 그릴지 정한다.
// 메시 원본은 소유하지 않고 가리키기만 한다. 원본을 가진 쪽(Application)이 이 컴포넌트보다 오래 살아야 한다.
// 상위 Nereides 프로젝트의 MeshComponent 자리다. 단계 4에서는 불러온 모델 원본도 이 컴포넌트가 받는다.
// 단계 4-2부터 같은 객체의 Animator를 가리킬 수 있다. 그러면 그릴 항목에 그 Animator의 뼈 행렬을 담는다.
class ModelRenderer : public Component
{
public:
    // 정점이나 인덱스가 비어 있는 메시는 그릴 수 없으므로 예외를 던진다.
    // animator는 없어도 된다(nullptr). 그러면 뼈 행렬 없이 기본 자세로 그린다. Animator는 같은 GameObject의 것이어야 한다.
    ModelRenderer(GameObject& owner, const MeshData& mesh, const DirectX::XMFLOAT4& color,
                  const Animator* animator = nullptr);

    void AddDrawItems(RenderFrame& frame, const DirectX::XMFLOAT4X4& world) const override;

private:
    const MeshData* m_mesh;
    DirectX::XMFLOAT4 m_color;
    const Animator* m_animator;
};
