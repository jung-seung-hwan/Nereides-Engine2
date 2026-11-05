#include "LifecycleTests.h"
#include "Input.h"
#include "Scene.h"
#include "Time.h"
#include <iostream>
#include <memory>
#include <stdexcept>

namespace
{
struct Counts
{
    int fixedUpdates = 0; // FixedUpdate가 불린 횟수
    int frameUpdates = 0; // Update가 불린 횟수
    int destroyed = 0;
};

// 수명 검증은 입력과 시간을 쓰지 않지만, Scene을 만들려면 Input과 Time이 필요하다.
// FixedUpdate·Update를 매개변수 없는 모양으로 두려고 Scene이 Input과 Time을 갖게 한 설계 선택의 대가다.
// Application과 같은 선언 순서라서 Scene이 가장 먼저 정리된다.
struct TestWorld
{
    Input input;
    Time time{0.1, 1};
    Scene scene{input, time};
};

void Check(bool condition, const char* message)
{
    if (!condition)
        throw std::runtime_error(message);
}

class Counter : public Component
{
public:
    Counter(GameObject& owner, Counts& counts) : Component(owner), m_counts(counts) {}
    ~Counter() override { ++m_counts.destroyed; }
    void FixedUpdate() override { ++m_counts.fixedUpdates; }
private:
    Counts& m_counts;
};

class DestroySelf : public Component
{
public:
    DestroySelf(GameObject& owner, Counts& counts) : Component(owner), m_counts(counts) {}
    ~DestroySelf() override { ++m_counts.destroyed; }
    void FixedUpdate() override
    {
        GetOwner().RequestDestroy();
        GetOwner().RequestDestroy();
        Check(m_counts.destroyed == 0, "Destroyed while FixedUpdate was running.");
        Check(GetOwner().GetName() == "self", "Owner disappeared before callback returned.");
    }
private:
    Counts& m_counts;
};

// Scene은 소유자를 거쳐 찾는다. 생성자로 따로 받지 않는다.
class ClearScene : public Component
{
public:
    ClearScene(GameObject& owner, Counts& counts) : Component(owner), m_counts(counts) {}
    ~ClearScene() override { ++m_counts.destroyed; }
    void FixedUpdate() override
    {
        GetOwner().GetScene().Clear();
        Check(m_counts.destroyed == 0, "Clear destroyed an executing component.");
    }
private:
    Counts& m_counts;
};

class SpawnOnce : public Component
{
public:
    SpawnOnce(GameObject& owner, Counts& counts) : Component(owner), m_counts(counts) {}
    void FixedUpdate() override
    {
        if (m_spawned)
            return;
        m_spawned = true;
        // vector 재할당 이후에도 실행 중인 객체의 주소는 유효해야 한다.
        Scene& scene = GetOwner().GetScene();
        for (int i = 0; i < 20; ++i)
        {
            GameObject& child = scene.CreateObject("spawned");
            child.AddComponent(std::make_unique<Counter>(child, m_counts));
        }
        GetOwner().AddComponent(std::make_unique<Counter>(GetOwner(), m_counts));
    }
private:
    Counts& m_counts;
    bool m_spawned = false;
};

// FixedUpdate와 Update를 둘 다 재정의해 따로 센다.
class BothCounter : public Component
{
public:
    BothCounter(GameObject& owner, Counts& counts) : Component(owner), m_counts(counts) {}
    void FixedUpdate() override { ++m_counts.fixedUpdates; }
    void Update() override { ++m_counts.frameUpdates; }
private:
    Counts& m_counts;
};

// Update 안에서 자기 객체의 삭제를 요청한다.
class DestroyInUpdate : public Component
{
public:
    DestroyInUpdate(GameObject& owner, Counts& counts) : Component(owner), m_counts(counts) {}
    ~DestroyInUpdate() override { ++m_counts.destroyed; }
    void Update() override
    {
        GetOwner().RequestDestroy();
        Check(m_counts.destroyed == 0, "Destroyed while Update was running.");
    }
private:
    Counts& m_counts;
};

// 갱신 도중에 다른 갱신을 시작하려 한다. Scene이 거부한 횟수를 센다.
class NestedUpdate : public Component
{
public:
    NestedUpdate(GameObject& owner, int& rejected) : Component(owner), m_rejected(rejected) {}
    void FixedUpdate() override
    {
        try { GetOwner().GetScene().Update(); } catch (const std::logic_error&) { ++m_rejected; }
    }
    void Update() override
    {
        try { GetOwner().GetScene().FixedUpdate(); } catch (const std::logic_error&) { ++m_rejected; }
    }
private:
    int& m_rejected;
};

void TestActive()
{
    Counts counts;
    TestWorld world;
    Scene& scene = world.scene;
    GameObject& object = scene.CreateObject("active");
    auto counter = std::make_unique<Counter>(object, counts);
    Counter* borrowed = counter.get();
    object.AddComponent(std::move(counter));
    scene.FixedUpdate();
    object.SetActive(false);
    scene.FixedUpdate();
    Check(counts.fixedUpdates == 1, "Inactive object was updated.");
    object.SetActive(true);
    borrowed->SetEnabled(false);
    scene.FixedUpdate();
    Check(counts.fixedUpdates == 1, "Disabled component was updated.");
    borrowed->SetEnabled(true);
    scene.FixedUpdate();
    Check(counts.fixedUpdates == 2, "Reactivated component did not update.");
    scene.Clear();
    scene.Clear();
    Check(counts.destroyed == 1 && scene.ObjectCount() == 0, "Clear must be safe to repeat.");
}

void TestDeferredDelete()
{
    Counts self, later, other;
    TestWorld world;
    Scene& scene = world.scene;
    GameObject& first = scene.CreateObject("self");
    first.AddComponent(std::make_unique<DestroySelf>(first, self));
    first.AddComponent(std::make_unique<Counter>(first, later));
    GameObject& second = scene.CreateObject("other");
    second.AddComponent(std::make_unique<Counter>(second, other));
    scene.FixedUpdate();
    Check(self.destroyed == 1 && later.destroyed == 1, "Deletion cleanup count differs.");
    Check(later.fixedUpdates == 0 && other.fixedUpdates == 1, "Deletion broke update order.");
    Check(scene.ObjectCount() == 1, "Deleted object remains in the scene.");
}

void TestClearDuringFixedUpdate()
{
    Counts clear, other;
    TestWorld world;
    Scene& scene = world.scene;
    GameObject& first = scene.CreateObject("clear");
    first.AddComponent(std::make_unique<ClearScene>(first, clear));
    GameObject& second = scene.CreateObject("other");
    second.AddComponent(std::make_unique<Counter>(second, other));
    scene.FixedUpdate();
    Check(clear.destroyed == 1 && other.destroyed == 1, "Clear cleanup count differs.");
    Check(other.fixedUpdates == 0 && scene.ObjectCount() == 0, "Clear must skip remaining updates.");
}

void TestCreationBoundary()
{
    Counts counts;
    TestWorld world;
    Scene& scene = world.scene;
    GameObject& parent = scene.CreateObject("parent");
    parent.AddComponent(std::make_unique<SpawnOnce>(parent, counts));
    scene.FixedUpdate();
    Check(counts.fixedUpdates == 0, "New objects/components ran in their creation update.");
    scene.FixedUpdate();
    Check(counts.fixedUpdates == 21, "New objects/components did not run next update.");
    scene.Clear();
    Check(counts.destroyed == 21, "Spawned components were not cleaned up.");
}

void TestRepeatedLifetime()
{
    Counts counts;
    for (int i = 0; i < 20; ++i)
    {
        TestWorld world;
        Scene& scene = world.scene;
        GameObject& object = scene.CreateObject("repeat");
        object.AddComponent(std::make_unique<Counter>(object, counts));
        scene.FixedUpdate();
    }
    Check(counts.fixedUpdates == 20 && counts.destroyed == 20, "Scene destruction leaked or doubled cleanup.");
}

void TestFixedUpdateAndUpdateSplit()
{
    Counts counts;
    TestWorld world;
    Scene& scene = world.scene;
    GameObject& object = scene.CreateObject("both");
    auto counter = std::make_unique<BothCounter>(object, counts);
    BothCounter* borrowed = counter.get();
    object.AddComponent(std::move(counter));

    // 두 갱신은 각자 자기 이름의 함수만 부른다.
    scene.FixedUpdate();
    scene.FixedUpdate();
    Check(counts.fixedUpdates == 2 && counts.frameUpdates == 0, "Scene.FixedUpdate must call only FixedUpdate.");
    scene.Update();
    Check(counts.fixedUpdates == 2 && counts.frameUpdates == 1, "Scene.Update must call only Update.");

    // 비활성 객체와 꺼진 컴포넌트는 두 갱신 모두에서 빠진다.
    object.SetActive(false);
    scene.FixedUpdate();
    scene.Update();
    Check(counts.fixedUpdates == 2 && counts.frameUpdates == 1, "An inactive object must skip both updates.");
    object.SetActive(true);
    borrowed->SetEnabled(false);
    scene.FixedUpdate();
    scene.Update();
    Check(counts.fixedUpdates == 2 && counts.frameUpdates == 1, "A disabled component must skip both updates.");
}

void TestUpdateBoundary()
{
    TestWorld world;
    Scene& scene = world.scene;

    // Update 안의 삭제 요청도 FixedUpdate와 같이 Update가 끝난 뒤 반영된다.
    Counts leaving;
    GameObject& doomed = scene.CreateObject("doomed");
    doomed.AddComponent(std::make_unique<DestroyInUpdate>(doomed, leaving));
    scene.FixedUpdate();
    Check(leaving.destroyed == 0 && scene.ObjectCount() == 1, "FixedUpdate must not run the Update-only delete request.");
    scene.Update();
    Check(leaving.destroyed == 1 && scene.ObjectCount() == 0, "A delete request in Update must apply when Update ends.");

    // 갱신 도중에는 FixedUpdate·Update 어느 것도 다시 시작할 수 없다.
    int rejected = 0;
    GameObject& nested = scene.CreateObject("nested");
    nested.AddComponent(std::make_unique<NestedUpdate>(nested, rejected));
    scene.FixedUpdate();
    scene.Update();
    Check(rejected == 2, "Starting an update inside another update must be rejected.");
    // 거부된 뒤에도 Scene은 계속 쓸 수 있다.
    scene.Update();
    Check(rejected == 3, "The scene must keep working after a rejected nested update.");
}
}

int RunLifecycleTests()
{
    try
    {
        TestActive();
        TestDeferredDelete();
        TestClearDuringFixedUpdate();
        TestCreationBoundary();
        TestRepeatedLifetime();
        TestFixedUpdateAndUpdateSplit();
        TestUpdateBoundary();
        std::cout << "Lifecycle tests PASS (7 cases)\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "Lifecycle tests FAIL: " << error.what() << '\n';
        return 1;
    }
}
