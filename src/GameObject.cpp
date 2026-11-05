#include "GameObject.h"
#include <iostream>
#include <stdexcept>
#include <utility>

GameObject::GameObject(Scene& scene, const std::string& name) : m_scene(scene), m_name(name)
{
    std::cout << "Create GameObject: " << m_name << '\n';
}

GameObject::~GameObject()
{
    // Component가 정리되는 동안에는 owner의 이름과 Transform이 유효하다.
    m_components.clear();
    std::cout << "Destroy GameObject: " << m_name << '\n';
}

void GameObject::SetActive(bool active)
{
    m_active = active;
    std::cout << m_name << " active=" << active << '\n';
}

void GameObject::AddComponent(std::unique_ptr<Component> component)
{
    if (!component || &component->GetOwner() != this)
        throw std::invalid_argument("Component owner must match this GameObject.");

    // 소유권을 함수 인자에서 GameObject의 보관함으로 옮긴다.
    m_components.push_back(std::move(component));
}

void GameObject::FixedUpdate()
{
    RunComponents(true);
}

void GameObject::Update()
{
    RunComponents(false);
}

void GameObject::RunComponents(bool fixedStep)
{
    // 이번 갱신 중 추가한 Component는 다음 갱신부터 실행한다.
    const std::size_t count = m_components.size();
    for (std::size_t i = 0; i < count; ++i)
    {
        if (!m_active || m_destroyRequested)
            break;

        Component* component = m_components[i].get();
        if (!component->IsEnabled())
            continue;
        if (fixedStep)
            component->FixedUpdate();
        else
            component->Update();
    }
}

void GameObject::RequestDestroy()
{
    if (!m_destroyRequested)
    {
        m_destroyRequested = true;
        std::cout << "Request destroy: " << m_name << '\n';
    }
}

void GameObject::SavePreviousTransform()
{
    m_previousTransform = transform;
    m_hasPreviousTransform = true;
}

Transform GameObject::InterpolatedTransform(float alpha) const
{
    // 직전 값이 없으면 섞을 출발점이 없다. 원점에서 끌려오지 않게 현재 값을 그대로 쓴다.
    if (!m_hasPreviousTransform)
        return transform;
    return Lerp(m_previousTransform, transform, alpha);
}

void GameObject::AddDrawItems(RenderFrame& frame, const DirectX::XMFLOAT4X4& world) const
{
    // FixedUpdate·Update와 같은 규칙: 꺼진 컴포넌트는 부르지 않는다. 객체의 활성 여부는 수집 함수가 먼저 본다.
    for (const auto& component : m_components)
    {
        if (component->IsEnabled())
            component->AddDrawItems(frame, world);
    }
}
