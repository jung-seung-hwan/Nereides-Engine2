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
// 이 시험들은 BeginFrame 없이 Scene.FixedUpdate를 바로 부른다. 그래서 여기서 DeltaTime()을 읽으면 tick 길이가 아니라 0이 나온다.
// 여기의 컴포넌트는 시간을 읽지 않는다. 시간이 필요한 시험은 입력·시간 시험의 RunFrame처럼 BeginFrame부터 진행한다.
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
    // Update에서도 같은 일을 한다. 두 갱신 모두 정리를 갱신이 끝난 뒤로 미루는지 본다.
    void Update() override
    {
        GetOwner().GetScene().Clear();
        Check(m_counts.destroyed == 0, "Clear destroyed an executing component during Update.");
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

// Update 안에서 자기 객체의 삭제를 요청한 뒤 예외를 던진다.
class ThrowInUpdate : public Component
{
public:
    ThrowInUpdate(GameObject& owner, Counts& counts) : Component(owner), m_counts(counts) {}
    ~ThrowInUpdate() override { ++m_counts.destroyed; }
    void Update() override
    {
        GetOwner().RequestDestroy();
        throw std::runtime_error("Update failed on purpose.");
    }
private:
    Counts& m_counts;
};

// 첫 Update에서 새 객체를 만들고, 새 객체와 자기 객체에 BothCounter를 붙인다.
class SpawnInUpdate : public Component
{
public:
    SpawnInUpdate(GameObject& owner, Counts& counts) : Component(owner), m_counts(counts) {}
    void Update() override
    {
        if (m_spawned)
            return;
        m_spawned = true;
        GameObject& child = GetOwner().GetScene().CreateObject("spawned in update");
        child.AddComponent(std::make_unique<BothCounter>(child, m_counts));
        GetOwner().AddComponent(std::make_unique<BothCounter>(GetOwner(), m_counts));
    }
private:
    Counts& m_counts;
    bool m_spawned = false;
};

// tick 안의 FixedUpdate에서 객체를 하나 만든다. 만든 객체의 주소를 기억해 두고, 밖에서 그 객체를 계속 쓸 수 있는지 본다.
class SpawnInTick : public Component
{
public:
    SpawnInTick(GameObject& owner, Counts& counts, GameObject*& spawned)
        : Component(owner), m_counts(counts), m_spawned(spawned)
    {
    }
    void FixedUpdate() override
    {
        if (m_spawned != nullptr)
            return;
        GameObject& child = GetOwner().GetScene().CreateObject("spawned in tick");
        child.AddComponent(std::make_unique<Counter>(child, m_counts));
        m_spawned = &child;
    }
private:
    Counts& m_counts;
    GameObject*& m_spawned;
};

// FixedUpdate에서 자기 객체의 삭제를 요청한 뒤 예외를 던진다.
class ThrowInFixedUpdate : public Component
{
public:
    ThrowInFixedUpdate(GameObject& owner, Counts& counts) : Component(owner), m_counts(counts) {}
    ~ThrowInFixedUpdate() override { ++m_counts.destroyed; }
    void FixedUpdate() override
    {
        GetOwner().RequestDestroy();
        throw std::runtime_error("FixedUpdate failed on purpose.");
    }
private:
    Counts& m_counts;
};

// tick 안의 FixedUpdate 뒤 단계(단계 4-2의 애니메이션, 단계 5의 충돌)가 객체를 차례로 도는 모습이다.
// 도중에 생성·Clear가 요청되어도 목록이 바뀌지 않아 끝까지 돌 수 있어야 한다.
std::size_t WalkObjects(const Scene& scene)
{
    std::size_t visited = 0;
    for (std::size_t i = 0; i < scene.ObjectCount(); ++i)
    {
        const GameObject& object = scene.ObjectAt(i);
        if (object.IsDestroyRequested())
            continue;
        ++visited;
    }
    return visited;
}

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

// Update에서는 생성·삭제·Clear를 하지 않기로 했다. 규칙을 어겨도 Scene이 FixedUpdate와 같은 경계 규칙으로
// 안전하게 처리하는지 본다. Scene.Update 끝의 삭제 반영은 이 안전장치다.
void TestUpdateBoundary()
{
    // 삭제 요청은 Update가 끝난 뒤 반영된다.
    {
        TestWorld world;
        Scene& scene = world.scene;
        Counts leaving;
        GameObject& doomed = scene.CreateObject("doomed");
        doomed.AddComponent(std::make_unique<DestroyInUpdate>(doomed, leaving));
        scene.FixedUpdate();
        Check(leaving.destroyed == 0 && scene.ObjectCount() == 1,
              "FixedUpdate must not run the Update-only delete request.");
        scene.Update();
        Check(leaving.destroyed == 1 && scene.ObjectCount() == 0, "A delete request in Update must apply when Update ends.");
    }

    // Update 도중 만든 객체와 붙인 컴포넌트는 다음 Update부터 불린다.
    {
        TestWorld world;
        Scene& scene = world.scene;
        Counts spawned;
        GameObject& parent = scene.CreateObject("parent");
        parent.AddComponent(std::make_unique<SpawnInUpdate>(parent, spawned));
        scene.Update();
        Check(spawned.frameUpdates == 0, "Objects/components added during Update ran in the same Update.");
        scene.Update();
        Check(spawned.frameUpdates == 2, "Objects/components added during Update did not run in the next Update.");
    }

    // Update 도중 Clear를 불러도 정리는 Update가 끝난 뒤에 한다. 남은 객체의 Update는 불리지 않는다.
    {
        TestWorld world;
        Scene& scene = world.scene;
        Counts clear, other;
        GameObject& first = scene.CreateObject("clear");
        first.AddComponent(std::make_unique<ClearScene>(first, clear));
        GameObject& second = scene.CreateObject("other");
        second.AddComponent(std::make_unique<BothCounter>(second, other));
        scene.Update();
        Check(clear.destroyed == 1 && other.frameUpdates == 0 && scene.ObjectCount() == 0,
              "Clear during Update must skip the remaining updates and clean up after Update.");
    }
}

// 갱신이 실패하거나 잘못 불려도 Scene은 정리하고 계속 쓸 수 있다.
void TestUpdateFailure()
{
    // Update가 예외를 던져도 그 전에 받은 삭제 요청은 반영되고, 예외는 부른 쪽까지 간다.
    {
        TestWorld world;
        Scene& scene = world.scene;
        Counts failing;
        GameObject& object = scene.CreateObject("failing");
        object.AddComponent(std::make_unique<ThrowInUpdate>(object, failing));
        bool thrown = false;
        try { scene.Update(); } catch (const std::runtime_error&) { thrown = true; }
        Check(thrown, "An exception from Update must reach the caller.");
        Check(failing.destroyed == 1 && scene.ObjectCount() == 0, "A delete request before an exception must still apply.");
        // 갱신 중 표시가 지워졌으므로 다음 갱신을 시작할 수 있다. 남아 있으면 logic_error가 난다.
        scene.FixedUpdate();
        scene.Update();
    }

    // 갱신 도중에는 FixedUpdate·Update 어느 것도 다시 시작할 수 없다.
    {
        TestWorld world;
        Scene& scene = world.scene;
        int rejected = 0;
        GameObject& nested = scene.CreateObject("nested");
        nested.AddComponent(std::make_unique<NestedUpdate>(nested, rejected));
        scene.FixedUpdate();
        scene.Update();
        Check(rejected == 2, "Starting an update inside another update must be rejected.");
        // 거부된 뒤에도 바깥 갱신은 끝까지 돌고, 다음 갱신을 시작할 수 있다.
        scene.Update();
        Check(rejected == 3, "The scene must keep working after a rejected nested update.");
    }
}

// tick 안(BeginTick~EndTick)에서 요청된 생성·삭제·Clear는 EndTick에서 한꺼번에 반영된다.
// 그 전까지는 목록이 바뀌지 않아, FixedUpdate 뒤의 단계가 객체를 차례로 돌 수 있다.
void TestTickBoundary()
{
    // 생성: tick 안에서 만든 객체는 EndTick까지 목록에 보이지 않고, EndTick 뒤의 다음 tick부터 불린다.
    // 삭제: tick 안에서 삭제를 요청한 객체는 EndTick까지 목록에 남고, 그동안의 단계는 건너뛴다.
    {
        TestWorld world;
        Scene& scene = world.scene;
        Counts spawned, leaving;
        GameObject* child = nullptr;
        GameObject& parent = scene.CreateObject("parent");
        parent.AddComponent(std::make_unique<SpawnInTick>(parent, spawned, child));
        // DestroySelf는 자기 이름이 "self"인지 확인한다.
        GameObject& doomed = scene.CreateObject("self");
        doomed.AddComponent(std::make_unique<DestroySelf>(doomed, leaving));

        scene.BeginTick();
        scene.FixedUpdate();
        Check(child != nullptr && child->GetName() == "spawned in tick", "The created object must be usable at once.");
        Check(scene.ObjectCount() == 2, "An object created inside a tick must wait until EndTick.");
        Check(leaving.destroyed == 0 && doomed.IsDestroyRequested(), "A delete request inside a tick must wait until EndTick.");
        // FixedUpdate 뒤의 단계: 삭제를 기다리는 객체는 건너뛰고, 새 객체는 아직 없다.
        Check(WalkObjects(scene) == 1, "A later tick stage must see only the surviving objects.");
        scene.EndTick();
        Check(scene.ObjectCount() == 2 && leaving.destroyed == 1, "EndTick must apply the creation and the deletion.");
        Check(spawned.fixedUpdates == 0, "The new object ran before the next tick.");

        scene.BeginTick();
        scene.FixedUpdate();
        scene.EndTick();
        Check(spawned.fixedUpdates == 1, "The new object did not run in the next tick.");
    }

    // FixedUpdate 뒤의 단계가 객체를 차례로 도는 도중에 생성과 Clear가 요청되어도, 목록은 EndTick까지 그대로다.
    {
        TestWorld world;
        Scene& scene = world.scene;
        Counts counts;
        for (int i = 0; i < 3; ++i)
        {
            GameObject& object = scene.CreateObject("walked");
            object.AddComponent(std::make_unique<Counter>(object, counts));
        }
        scene.BeginTick();
        scene.FixedUpdate();
        std::size_t visited = 0;
        for (std::size_t i = 0; i < scene.ObjectCount(); ++i)
        {
            ++visited;
            if (i == 0)
            {
                scene.CreateObject("made during the walk");
                scene.Clear();
            }
            // 생성과 Clear 뒤에도 같은 개수를 돈다. 전에는 여기서 out_of_range가 났다.
            Check(scene.ObjectCount() == 3, "The object list changed while a tick stage was walking it.");
        }
        Check(visited == 3 && counts.destroyed == 0, "The walk must finish before anything is destroyed.");
        scene.EndTick();
        Check(scene.ObjectCount() == 0 && counts.destroyed == 3, "EndTick must apply the Clear, including the object made during the walk.");
    }

    // tick을 겹쳐 시작하거나 시작하지 않은 tick을 끝내면 거부한다.
    {
        TestWorld world;
        Scene& scene = world.scene;
        bool rejected = false;
        try { scene.EndTick(); } catch (const std::logic_error&) { rejected = true; }
        Check(rejected, "EndTick without BeginTick must be rejected.");
        scene.BeginTick();
        rejected = false;
        try { scene.BeginTick(); } catch (const std::logic_error&) { rejected = true; }
        Check(rejected, "A nested BeginTick must be rejected.");
        scene.EndTick();
    }
}

// tick 안의 FixedUpdate가 예외를 던져도 반영 경계는 EndTick 하나다. 그 전의 요청은 EndTick까지 기다리고, Scene은 계속 쓸 수 있다.
void TestTickFailure()
{
    TestWorld world;
    Scene& scene = world.scene;
    Counts failing;
    GameObject& object = scene.CreateObject("failing in tick");
    object.AddComponent(std::make_unique<ThrowInFixedUpdate>(object, failing));
    scene.BeginTick();
    bool thrown = false;
    try { scene.FixedUpdate(); } catch (const std::runtime_error&) { thrown = true; }
    Check(thrown, "An exception from FixedUpdate must reach the caller.");
    Check(failing.destroyed == 0 && scene.ObjectCount() == 1 && object.IsDestroyRequested(),
          "Inside a tick, a delete request before an exception must still wait for EndTick.");
    scene.EndTick();
    Check(failing.destroyed == 1 && scene.ObjectCount() == 0, "EndTick after an exception must apply the request.");
    // 예외 뒤에도 다음 tick을 시작할 수 있다.
    scene.BeginTick();
    scene.FixedUpdate();
    scene.EndTick();
}

// tick 안에서 만든 객체를 같은 tick에 삭제 요청하면, 목록에 들어가지 않고 EndTick에서 바로 사라진다.
void TestCreateAndDestroyInTick()
{
    TestWorld world;
    Scene& scene = world.scene;
    Counts counts;
    scene.BeginTick();
    GameObject& shortLived = scene.CreateObject("short-lived");
    shortLived.AddComponent(std::make_unique<Counter>(shortLived, counts));
    shortLived.RequestDestroy();
    Check(scene.ObjectCount() == 0 && counts.destroyed == 0, "The object must wait in the pending list until EndTick.");
    scene.EndTick();
    Check(scene.ObjectCount() == 0 && counts.destroyed == 1 && counts.fixedUpdates == 0,
          "An object created and destroyed in the same tick must never enter the list or run.");
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
        TestUpdateFailure();
        TestTickBoundary();
        TestTickFailure();
        TestCreateAndDestroyInTick();
        std::cout << "Lifecycle tests PASS (11 cases)\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "Lifecycle tests FAIL: " << error.what() << '\n';
        return 1;
    }
}
