#pragma once
#include "Component.h"
#include "Transform.h"
#include <memory>
#include <string>
#include <vector>

// Scene.h가 이 파일을 포함하므로, 여기서는 Scene이라는 이름만 알린다.
class Scene;

class GameObject
{
public:
    // Scene::CreateObject가 자기 자신을 넘겨 만든다.
    GameObject(Scene& scene, const std::string& name);
    ~GameObject();

    // 소유 객체는 복사하지 않는다. Component의 owner 참조가 유지되어야 한다.
    GameObject(const GameObject&) = delete;
    GameObject& operator=(const GameObject&) = delete;

    const std::string& GetName() const { return m_name; }
    // 나를 소유한 Scene. Scene이 나보다 항상 오래 살므로 참조가 유효하다.
    Scene& GetScene() const { return m_scene; }
    bool IsActive() const { return m_active; }
    void SetActive(bool active);
    void AddComponent(std::unique_ptr<Component> component);
    void Update();

    // 즉시 delete하지 않고 Scene에 정리를 요청한다.
    void RequestDestroy();
    bool IsDestroyRequested() const { return m_destroyRequested; }

    Transform transform;

private:
    Scene& m_scene;
    std::string m_name;
    bool m_active = true;
    bool m_destroyRequested = false;
    std::vector<std::unique_ptr<Component>> m_components;
};
