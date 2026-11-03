#include "LifecycleTests.h"
#include "Scene.h"
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

class ClearScene : public Component
{
public:
    ClearScene(GameObject& owner, Scene& scene, Counts& counts)
        : Component(owner), m_scene(scene), m_counts(counts) {}
    ~ClearScene() override { ++m_counts.destroyed; }
    void Update() override
    {
        m_scene.Clear();
        Check(m_counts.destroyed == 0, "Clear destroyed an executing component.");
    }
private:
    Scene& m_scene;
    Counts& m_counts;
};

class SpawnOnce : public Component
{
public:
    SpawnOnce(GameObject& owner, Scene& scene, Counts& counts)
        : Component(owner), m_scene(scene), m_counts(counts) {}
    void Update() override
    {
        if (m_spawned)
            return;
        m_spawned = true;
        // vector 재할당 이후에도 실행 중인 객체의 주소는 유효해야 한다.
        for (int i = 0; i < 20; ++i)
        {
            GameObject& child = m_scene.CreateObject("spawned");
            child.AddComponent(std::make_unique<Counter>(child, m_counts));
        }
        GetOwner().AddComponent(std::make_unique<Counter>(GetOwner(), m_counts));
    }
private:
    Scene& m_scene;
    Counts& m_counts;
    bool m_spawned = false;
};

void TestActive()
{
    Counts counts;
    Scene scene;
    GameObject& object = scene.CreateObject("active");
    auto counter = std::make_unique<Counter>(object, counts);
    Counter* borrowed = counter.get();
    object.AddComponent(std::move(counter));
    scene.Update(1);
    object.SetActive(false);
    scene.Update(1);
    Check(counts.updates == 1, "Inactive object was updated.");
    object.SetActive(true);
    borrowed->SetEnabled(false);
    scene.Update(1);
    Check(counts.updates == 1, "Disabled component was updated.");
    borrowed->SetEnabled(true);
    scene.Update(1);
    Check(counts.updates == 2, "Reactivated component did not update.");
    scene.Clear();
    scene.Clear();
    Check(counts.destroyed == 1 && scene.ObjectCount() == 0, "Clear must be safe to repeat.");
}

void TestDeferredDelete()
{
    Counts self, later, other;
    Scene scene;
    GameObject& first = scene.CreateObject("self");
    first.AddComponent(std::make_unique<DestroySelf>(first, self));
    first.AddComponent(std::make_unique<Counter>(first, later));
    GameObject& second = scene.CreateObject("other");
    second.AddComponent(std::make_unique<Counter>(second, other));
    scene.Update(1);
    Check(self.destroyed == 1 && later.destroyed == 1, "Deletion cleanup count differs.");
    Check(later.updates == 0 && other.updates == 1, "Deletion broke update order.");
    Check(scene.ObjectCount() == 1, "Deleted object remains in the scene.");
}

void TestClearDuringUpdate()
{
    Counts clear, other;
    Scene scene;
    GameObject& first = scene.CreateObject("clear");
    first.AddComponent(std::make_unique<ClearScene>(first, scene, clear));
    GameObject& second = scene.CreateObject("other");
    second.AddComponent(std::make_unique<Counter>(second, other));
    scene.Update(1);
    Check(clear.destroyed == 1 && other.destroyed == 1, "Clear cleanup count differs.");
    Check(other.updates == 0 && scene.ObjectCount() == 0, "Clear must skip remaining updates.");
}

void TestCreationBoundary()
{
    Counts counts;
    Scene scene;
    GameObject& parent = scene.CreateObject("parent");
    parent.AddComponent(std::make_unique<SpawnOnce>(parent, scene, counts));
    scene.Update(1);
    Check(counts.updates == 0, "New objects/components ran in their creation update.");
    scene.Update(1);
    Check(counts.updates == 21, "New objects/components did not run next update.");
    scene.Clear();
    Check(counts.destroyed == 21, "Spawned components were not cleaned up.");
}

void TestRepeatedLifetime()
{
    Counts counts;
    for (int i = 0; i < 20; ++i)
    {
        Scene scene;
        GameObject& object = scene.CreateObject("repeat");
        object.AddComponent(std::make_unique<Counter>(object, counts));
        scene.Update(1);
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
