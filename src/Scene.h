#pragma once
#include "GameObject.h"
#include <memory>
#include <string>
#include <vector>

class Input;
class Time;

// Scene은 GameObject를 소유하고 갱신·정리한다.
// 컴포넌트가 소유자를 거쳐 읽는 Input과 Time도 읽기 전용으로 알려 준다.
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
    // tick마다 부른다. 모든 객체의 FixedUpdate를 부르고, 끝나면 삭제 예약을 반영한다.
    void FixedUpdate();
    // 프레임마다 한 번, 모든 tick이 끝난 뒤 부른다. 모든 객체의 Update를 부르고, 끝나면 삭제 예약을 반영한다.
    // Update에서는 생성·삭제·Clear를 하지 않기로 했다. 끝의 삭제 반영은 그 규칙을 어겼을 때 목록이 꼬이지 않게 하는 안전장치다.
    void Update();
    void Clear();
    std::size_t ObjectCount() const { return m_objects.size(); }

    // tick을 시작할 때 FixedUpdate 앞에서 부른다. 모든 객체(비활성 포함)의 Transform을 직전 값으로 복사한다.
    void SavePreviousTransforms();
    // index번째 객체를 읽기 전용으로 꺼낸다. 범위를 벗어나면 예외를 던진다.
    const GameObject& ObjectAt(std::size_t index) const;

    // 창구 규칙: 컴포넌트에 알려 주는 서비스는 Input과 Time 두 개뿐이다.
    // 그 밖에 컴포넌트가 쓰는 것은 CreateObject와 Clear뿐이다.
    // FixedUpdate, Update, SavePreviousTransforms는 Application만, ObjectAt은 수집 함수만 부른다.
    // 컴포넌트 소멸자에서는 CreateObject·Clear·FixedUpdate·Update를 부르지 않는다.
    // 소멸자는 이 Scene이 객체 목록을 지우는 도중에 실행되므로, 그때 목록을 바꾸면 정리가 꼬인다.
    // 소멸자에서 GetInput·GetTime으로 읽는 것은 괜찮다.
    const Input& GetInput() const { return m_input; }
    const Time& GetTime() const { return m_time; }

private:
    // FixedUpdate와 Update가 함께 쓰는 순회. fixedStep이 true면 FixedUpdate를, false면 Update를 부른다.
    void RunObjects(bool fixedStep);
    void RemoveDestroyedObjects();

    const Input& m_input;
    const Time& m_time;
    bool m_updating = false; // FixedUpdate나 Update가 도는 중이면 true
    std::vector<std::unique_ptr<GameObject>> m_objects;
};
