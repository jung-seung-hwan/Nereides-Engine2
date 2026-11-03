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

void GameObject::Update()
{
    // 이번 갱신 중 추가한 Component는 다음 갱신부터 실행한다.
    const std::size_t count = m_components.size();
    for (std::size_t i = 0; i < count; ++i)
    {
        if (!m_active || m_destroyRequested)
            break;

        Component* component = m_components[i].get();
        if (component->IsEnabled())
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
