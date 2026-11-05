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
    // Scene::CreateObject만 이 생성자를 부른다. 직접 만들면 Scene이 소유하지 않아
    // 갱신·정리되지 않고, Scene보다 오래 살면 GetScene이 끊어진 참조를 돌려준다.
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

    // 보간에 쓰는 직전 tick의 Transform. Scene이 tick을 시작할 때 복사한다.
    // 새로 만든 객체는 아직 직전 값이 없다.
    void SavePreviousTransform();
    bool HasPreviousTransform() const { return m_hasPreviousTransform; }
    // 그릴 때 쓰는 Transform. 직전 값이 있으면 직전 값과 현재 값을 alpha만큼 섞고, 없으면 현재 값이다.
    // 보간은 그리기에만 쓴다. 게임 코드는 transform만 읽고 쓴다.
    Transform InterpolatedTransform(float alpha) const;
    // 켜진 컴포넌트에게 그릴 항목을 넣게 한다. 수집 함수만 부른다.
    void AddDrawItems(RenderFrame& frame, const DirectX::XMFLOAT4X4& world) const;

    Transform transform;

private:
    Scene& m_scene;
    std::string m_name;
    bool m_active = true;
    bool m_destroyRequested = false;
    Transform m_previousTransform;
    bool m_hasPreviousTransform = false;
    std::vector<std::unique_ptr<Component>> m_components;
};
