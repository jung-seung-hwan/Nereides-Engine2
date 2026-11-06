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

    // 객체를 만든다. 돌려주는 참조는 바로 쓸 수 있다.
    // tick 안이나 갱신 도중에 만든 객체는 "대기 목록"에 두었다가 반영 시점에 객체 목록에 넣는다.
    // 그래서 그때까지는 ObjectCount·ObjectAt에 보이지 않고, FixedUpdate·Update도 받지 않는다.
    GameObject& CreateObject(const std::string& name);

    // tick의 시작과 끝. Application이 tick마다 BeginTick, (FixedUpdate·애니메이션·충돌 단계), EndTick 순서로 부른다.
    // BeginTick부터 EndTick까지 요청된 생성·삭제·Clear는 EndTick에서 한꺼번에 반영한다.
    // 그래서 tick 안의 어느 단계가 객체를 차례로 도는 동안에도 목록이 바뀌지 않는다.
    // 삭제를 기다리는 객체는 EndTick까지 목록에 남는다. 각 단계는 IsDestroyRequested()인 객체를 건너뛴다.
    void BeginTick();
    void EndTick();
    bool IsInTick() const { return m_inTick; }

    // tick마다 부른다. 모든 객체의 FixedUpdate를 부른다.
    // tick 안(BeginTick 뒤)이면 반영은 EndTick이 한다. tick 밖에서 바로 부르면(시험 코드) 끝날 때 바로 반영한다.
    void FixedUpdate();
    // 프레임마다 한 번, 모든 tick이 끝난 뒤 부른다. 모든 객체의 Update를 부르고, 끝나면 생성·삭제 예약을 반영한다.
    // Update에서는 생성·삭제·Clear를 하지 않기로 했다. 끝의 반영은 그 규칙을 어겼을 때 목록이 꼬이지 않게 하는 안전장치다.
    void Update();
    // 모든 객체(대기 목록 포함)의 삭제를 요청한다. tick 안이나 갱신 도중이면 정리는 반영 시점으로 미룬다.
    void Clear();
    std::size_t ObjectCount() const { return m_objects.size(); }

    // tick을 시작할 때 FixedUpdate 앞에서 부른다. 모든 객체(비활성 포함)의 Transform을 직전 값으로 복사한다.
    void SavePreviousTransforms();
    // index번째 객체를 읽기 전용으로 꺼낸다. 범위를 벗어나면 예외를 던진다.
    const GameObject& ObjectAt(std::size_t index) const;

    // 창구 규칙: 컴포넌트에 알려 주는 서비스는 Input과 Time 두 개뿐이다.
    // 그 밖에 컴포넌트가 쓰는 것은 CreateObject와 Clear뿐이다.
    // BeginTick, EndTick, FixedUpdate, Update, SavePreviousTransforms는 Application만, ObjectAt은 수집 함수만 부른다.
    // 컴포넌트 소멸자에서는 CreateObject·Clear·FixedUpdate·Update를 부르지 않는다.
    // 소멸자는 이 Scene이 객체 목록을 지우는 도중에 실행되므로, 그때 목록을 바꾸면 정리가 꼬인다.
    // 소멸자에서 GetInput·GetTime으로 읽는 것은 괜찮다.
    const Input& GetInput() const { return m_input; }
    const Time& GetTime() const { return m_time; }

private:
    // FixedUpdate와 Update가 함께 쓰는 순회. fixedStep이 true면 FixedUpdate를, false면 Update를 부른다.
    void RunObjects(bool fixedStep);
    // 미뤄 둔 생성과 삭제를 반영한다. 대기 목록의 객체를 객체 목록에 넣고, 삭제 요청된 객체를 지운다.
    void ApplyPendingChanges();

    const Input& m_input;
    const Time& m_time;
    bool m_updating = false; // FixedUpdate나 Update가 도는 중이면 true
    bool m_inTick = false;   // BeginTick과 EndTick 사이면 true
    std::vector<std::unique_ptr<GameObject>> m_objects;
    std::vector<std::unique_ptr<GameObject>> m_pendingObjects; // 만들었지만 아직 목록에 넣지 않은 객체
};
