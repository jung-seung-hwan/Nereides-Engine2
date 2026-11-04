#pragma once
#include "Component.h"
#include "Transform.h"
#include <memory>
#include <string>
#include <vector>

class GameObject
{
public:
    explicit GameObject(const std::string& name);
    ~GameObject();

    // 소유 객체는 복사하지 않는다. Component의 owner 참조가 유지되어야 한다.
    GameObject(const GameObject&) = delete;
    GameObject& operator=(const GameObject&) = delete;

    const std::string& GetName() const { return m_name; }
    bool IsActive() const { return m_active; }
    void SetActive(bool active);
    void AddComponent(std::unique_ptr<Component> component);
    void Update();

    // 즉시 delete하지 않고 Scene에 정리를 요청한다.
    void RequestDestroy();
    bool IsDestroyRequested() const { return m_destroyRequested; }

    Transform transform;

private:
    std::string m_name;
    bool m_active = true;
    bool m_destroyRequested = false;
    std::vector<std::unique_ptr<Component>> m_components;
};
