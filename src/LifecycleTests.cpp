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
    int updates = 0;
    int destroyed = 0;
};

// 수명 검증은 입력과 시간을 쓰지 않지만, Scene을 만들려면 Input과 Time이 필요하다.
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
    void Update() override { ++m_counts.updates; }
private:
    Counts& m_counts;
};

class DestroySelf : public Component
{
public:
    DestroySelf(GameObject& owner, Counts& counts) : Component(owner), m_counts(counts) {}
    ~DestroySelf() override { ++m_counts.destroyed; }
    void Update() override
    {
        GetOwner().RequestDestroy();
        GetOwner().RequestDestroy();
        Check(m_counts.destroyed == 0, "Destroyed while Update was running.");
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
    void Update() override
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
    void Update() override
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

void TestActive()
{
    Counts counts;
    TestWorld world;
    Scene& scene = world.scene;
    GameObject& object = scene.CreateObject("active");
    auto counter = std::make_unique<Counter>(object, counts);
    Counter* borrowed = counter.get();
    object.AddComponent(std::move(counter));
    scene.Update();
    object.SetActive(false);
    scene.Update();
    Check(counts.updates == 1, "Inactive object was updated.");
    object.SetActive(true);
    borrowed->SetEnabled(false);
    scene.Update();
    Check(counts.updates == 1, "Disabled component was updated.");
    borrowed->SetEnabled(true);
    scene.Update();
    Check(counts.updates == 2, "Reactivated component did not update.");
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
    scene.Update();
    Check(self.destroyed == 1 && later.destroyed == 1, "Deletion cleanup count differs.");
    Check(later.updates == 0 && other.updates == 1, "Deletion broke update order.");
    Check(scene.ObjectCount() == 1, "Deleted object remains in the scene.");
}

void TestClearDuringUpdate()
{
    Counts clear, other;
    TestWorld world;
    Scene& scene = world.scene;
    GameObject& first = scene.CreateObject("clear");
    first.AddComponent(std::make_unique<ClearScene>(first, clear));
    GameObject& second = scene.CreateObject("other");
    second.AddComponent(std::make_unique<Counter>(second, other));
    scene.Update();
    Check(clear.destroyed == 1 && other.destroyed == 1, "Clear cleanup count differs.");
    Check(other.updates == 0 && scene.ObjectCount() == 0, "Clear must skip remaining updates.");
}

void TestCreationBoundary()
{
    Counts counts;
    TestWorld world;
    Scene& scene = world.scene;
    GameObject& parent = scene.CreateObject("parent");
    parent.AddComponent(std::make_unique<SpawnOnce>(parent, counts));
    scene.Update();
    Check(counts.updates == 0, "New objects/components ran in their creation update.");
    scene.Update();
    Check(counts.updates == 21, "New objects/components did not run next update.");
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
        scene.Update();
    }
    Check(counts.updates == 20 && counts.destroyed == 20, "Scene destruction leaked or doubled cleanup.");
}
}

int RunLifecycleTests()
{
    try
    {
        TestActive();
        TestDeferredDelete();
        TestClearDuringUpdate();
        TestCreationBoundary();
        TestRepeatedLifetime();
        std::cout << "Lifecycle tests PASS (5 cases)\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "Lifecycle tests FAIL: " << error.what() << '\n';
        return 1;
    }
}
