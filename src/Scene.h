#pragma once
#include "GameObject.h"
#include <memory>
#include <string>
#include <vector>

class Input;
class Time;

// Scene은 GameObject를 소유하고 갱신·정리한다.
// 컴포넌트가 소유자를 거쳐 읽는 이번 tick의 Input과 Time도 읽기 전용으로 알려 준다.
// Input과 Time은 Application이 소유한다. Scene은 참조만 하므로 둘이 Scene보다 오래 살아야 한다.
class Scene
{
public:
    Scene(const Input& input, const Time& time);
    ~Scene();

    // 복사하면 GameObject가 가진 Scene 참조가 원래 Scene을 가리킨 채 남는다.
    Scene(const Scene&) = delete;
    Scene& operator=(const Scene&) = delete;

    GameObject& CreateObject(const std::string& name);
    void Update();
    void Clear();
    std::size_t ObjectCount() const { return m_objects.size(); }

    // 창구 규칙: 컴포넌트에 알려 주는 서비스는 Input과 Time 두 개뿐이다.
    const Input& GetInput() const { return m_input; }
    const Time& GetTime() const { return m_time; }

private:
    void RemoveDestroyedObjects();

    const Input& m_input;
    const Time& m_time;
    bool m_updating = false;
    std::vector<std::unique_ptr<GameObject>> m_objects;
};
