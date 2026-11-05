#include "InputTimeTests.h"
#include "Input.h"
#include "PlayerControl.h"
#include "Scene.h"
#include "Time.h"
#include <cmath>
#include <iostream>
#include <memory>
#include <stdexcept>

namespace
{
void Check(bool condition, const char* message)
{
    if (!condition)
        throw std::runtime_error(message);
}

bool Near(double a, double b)
{
    return std::fabs(a - b) < 1e-4;
}

// Time만 검사할 때 쓴다. 프레임의 tick을 정하고 모두 마친다.
int AdvanceFrame(Time& time, double unscaledDeltaTime)
{
    const int ticks = time.BeginFrame(unscaledDeltaTime);
    for (int i = 0; i < ticks; ++i)
        time.FinishTick();
    return ticks;
}

// Application과 같은 순서: 프레임마다 tick 수를 정하고, tick마다 소비·FixedUpdate를 한 뒤 tick을 마친다.
// 모든 tick이 끝나면 프레임마다 한 번 Update를 부른다.
int RunFrame(Time& time, Input& input, Scene& scene, double unscaledDeltaTime)
{
    const int ticks = time.BeginFrame(unscaledDeltaTime);
    for (int i = 0; i < ticks; ++i)
    {
        input.ConsumeForTick();
        scene.FixedUpdate();
        time.FinishTick();
    }
    scene.Update();
    return ticks;
}

void TestPressHoldRelease()
{
    Input input;
    input.OnKeyDown(Key::W);
    input.ConsumeForTick();
    Check(input.IsPressed(Action::MoveForward) && input.IsHeld(Action::MoveForward),
          "First tick must see press and hold.");
    input.ConsumeForTick();
    Check(!input.IsPressed(Action::MoveForward) && input.IsHeld(Action::MoveForward),
          "Press must be delivered once while hold stays.");
    input.OnKeyUp(Key::W);
    input.ConsumeForTick();
    Check(input.IsReleased(Action::MoveForward) && !input.IsHeld(Action::MoveForward),
          "Release tick must see release without hold.");
    input.ConsumeForTick();
    Check(!input.IsReleased(Action::MoveForward), "Release must be delivered once.");
}

void TestTickResultStaysFixed()
{
    Input input;
    input.OnKeyDown(Key::W);
    input.ConsumeForTick();
    // 다음 소비 전에 들어온 사건은 이번 tick의 결과를 바꾸지 않는다.
    input.OnKeyUp(Key::W);
    input.OnMouseMove(4, 0);
    Check(input.IsHeld(Action::MoveForward) && !input.IsReleased(Action::MoveForward)
              && input.MouseDeltaX() == 0,
          "Tick result changed before the next consume.");
    input.ConsumeForTick();
    Check(!input.IsHeld(Action::MoveForward) && input.IsReleased(Action::MoveForward)
              && input.MouseDeltaX() == 4,
          "Events after a consume were not delivered on the next tick.");
}

void TestShortTapKeptOnce()
{
    Input input;
    // tick이 없는 프레임이 지나가도 소비 전까지 사건은 보관된다.
    input.OnKeyDown(Key::Space);
    input.OnKeyUp(Key::Space);
    input.ConsumeForTick();
    Check(input.IsPressed(Action::Attack) && input.IsReleased(Action::Attack), "Short tap was lost.");
    Check(!input.IsHeld(Action::Attack), "Short tap must not count as hold.");
    input.ConsumeForTick();
    Check(!input.IsPressed(Action::Attack), "Short tap repeated on the next tick.");
}

void TestKeyRepeatIgnored()
{
    Input input;
    input.OnKeyDown(Key::W);
    input.ConsumeForTick();
    input.OnKeyDown(Key::W); // OS 자동 반복 신호
    input.ConsumeForTick();
    Check(!input.IsPressed(Action::MoveForward), "Key repeat became a new press.");
    input.OnKeyUp(Key::S); // 누르지 않은 키의 뗌 신호
    input.ConsumeForTick();
    Check(!input.IsReleased(Action::MoveBack), "Release without press was reported.");
}

void TestMouseDelta()
{
    Input input;
    input.OnMouseMove(3, 1);
    input.OnMouseMove(2, -1);
    input.ConsumeForTick();
    Check(input.MouseDeltaX() == 5 && input.MouseDeltaY() == 0, "Mouse delta was not summed.");
    input.ConsumeForTick();
    Check(input.MouseDeltaX() == 0 && input.MouseDeltaY() == 0, "Mouse delta repeated on the next tick.");
}

void TestFocusLost()
{
    Input input;
    input.OnKeyDown(Key::W);
    input.ConsumeForTick();
    input.OnKeyDown(Key::Space);
    input.OnMouseMove(7, 0);
    input.OnFocusLost();
    input.OnKeyUp(Key::W); // 포커스 상실 뒤 늦게 도착한 뗌 신호
    input.ConsumeForTick();
    Check(!input.IsHeld(Action::MoveForward) && !input.IsPressed(Action::Attack),
          "Input survived focus loss.");
    Check(!input.IsReleased(Action::MoveForward) && input.MouseDeltaX() == 0,
          "Focus loss left stale events.");
}

void TestTickAccumulation()
{
    Time time(0.1, 3);
    Check(AdvanceFrame(time, 0.05) == 0, "Half a tick must not run.");
    Check(AdvanceFrame(time, 0.05) == 1, "Leftover time must carry to the next frame.");
    Check(AdvanceFrame(time, 0.2) == 2, "Two ticks of time must run two ticks.");
    Check(Near(time.SimulationTime(), 0.3) && Near(time.RealTime(), 0.3), "Accumulated time differs.");
}

void TestSimulationTimePerTick()
{
    Time time(0.1, 3);
    Check(time.BeginFrame(0.3) == 3, "Three ticks of time must run three ticks.");
    // BeginFrame은 tick 수만 정한다. 시뮬레이션 시간은 tick을 마칠 때마다 늘어난다.
    Check(Near(time.SimulationTime(), 0.0), "Simulation time moved before any tick finished.");
    time.FinishTick();
    Check(Near(time.SimulationTime(), 0.1), "First tick must end at 0.1.");
    time.FinishTick();
    time.FinishTick();
    Check(Near(time.SimulationTime(), 0.3), "Third tick must end at 0.3.");

    int rejected = 0;
    try { time.FinishTick(); } catch (const std::logic_error&) { ++rejected; }
    time.BeginFrame(0.1);
    try { time.BeginFrame(0.1); } catch (const std::logic_error&) { ++rejected; }
    Check(rejected == 2, "Broken tick order was not reported.");
}

void TestPauseAtBoundary()
{
    Time time(0.1, 3);
    AdvanceFrame(time, 0.05);
    time.RequestPause(true);
    Check(!time.IsPaused(), "Pause must wait for the frame boundary.");
    Check(AdvanceFrame(time, 0.3) == 0 && time.IsPaused(), "Paused frame ran ticks.");
    Check(Near(time.RealTime(), 0.35) && Near(time.SimulationTime(), 0.0),
          "Pause must stop only simulation time.");
    time.RequestPause(false);
    Check(AdvanceFrame(time, 0.05) == 1, "Leftover time before pause was lost.");
}

void TestScaleAtBoundary()
{
    Time time(0.1, 100);
    time.RequestScale(2.0);
    Check(Near(time.Scale(), 1.0), "Scale must wait for the frame boundary.");
    Check(AdvanceFrame(time, 1.0) == 20, "Double speed must double the tick count.");
    Check(Near(time.SimulationTime(), 2.0) && Near(time.RealTime(), 1.0), "Scaled time differs.");
}

void TestCatchUpLimit()
{
    Time time(0.1, 3);
    Check(AdvanceFrame(time, 1.05) == 3, "Catch-up limit was not applied.");
    // 남은 0.75초는 tick 조각 0.05까지 모두 버린다.
    Check(Near(time.DroppedTime(), 0.75), "Excess time was not recorded as dropped.");
    Check(AdvanceFrame(time, 0.05) == 0, "Part of the dropped time was carried to the next frame.");
    Check(Near(time.SimulationTime(), 0.3) && Near(time.RealTime(), 1.1), "Low FPS time differs.");
}

void TestInvalidValues()
{
    int rejected = 0;
    try { Time bad(0.0, 3); bad.BeginFrame(0.0); } catch (const std::invalid_argument&) { ++rejected; }
    try { Time bad(0.1, 0); bad.BeginFrame(0.0); } catch (const std::invalid_argument&) { ++rejected; }
    Time time(0.1, 3);
    try { time.RequestScale(-1.0); } catch (const std::invalid_argument&) { ++rejected; }
    try { time.BeginFrame(-0.1); } catch (const std::invalid_argument&) { ++rejected; }
    Check(rejected == 4, "Invalid time values were accepted.");
}

void TestInvalidKey()
{
    // Key::Count는 배열 크기용 값이다. 실제 키처럼 넣으면 거부해야 한다.
    Input input;
    int rejected = 0;
    try { input.OnKeyDown(Key::Count); } catch (const std::invalid_argument&) { ++rejected; }
    try { input.OnKeyUp(Key::Count); } catch (const std::invalid_argument&) { ++rejected; }
    Check(rejected == 2, "Key::Count was accepted as a real key.");
}

// 컴포넌트가 자기 FixedUpdate 안에서 읽은 Input·Time의 주소를 기록한다.
struct SeenServices
{
    const Input* input = nullptr;
    const Time* time = nullptr;
};

class ServiceReader : public Component
{
public:
    ServiceReader(GameObject& owner, SeenServices& seen) : Component(owner), m_seen(seen) {}
    void FixedUpdate() override
    {
        m_seen.input = &GetInput();
        m_seen.time = &GetTime();
    }
private:
    SeenServices& m_seen;
};

// 첫 FixedUpdate에서 새 객체를 만들고 ServiceReader를 붙인다.
class SpawnReader : public Component
{
public:
    SpawnReader(GameObject& owner, SeenServices& seen) : Component(owner), m_seen(seen) {}
    void FixedUpdate() override
    {
        if (m_spawned)
            return;
        m_spawned = true;
        GameObject& child = GetOwner().GetScene().CreateObject("spawned reader");
        child.AddComponent(std::make_unique<ServiceReader>(child, m_seen));
    }
private:
    SeenServices& m_seen;
    bool m_spawned = false;
};

void TestServiceAccessThroughOwner()
{
    Input input;
    Time time(0.1, 3);
    Scene scene(input, time);

    // 처음부터 있던 객체와, 갱신 중에 만들어질 객체의 컴포넌트를 함께 본다.
    SeenServices direct;
    GameObject& probe = scene.CreateObject("probe");
    probe.AddComponent(std::make_unique<ServiceReader>(probe, direct));
    SeenServices spawned;
    GameObject& spawner = scene.CreateObject("spawner");
    spawner.AddComponent(std::make_unique<SpawnReader>(spawner, spawned));
    Check(&probe.GetScene() == &scene, "GameObject does not know the Scene that owns it.");

    scene.FixedUpdate();
    Check(direct.input == &input && direct.time == &time,
          "Component did not reach the Scene's Input and Time.");
    Check(spawned.input == nullptr, "Spawned object ran in its creation update.");

    scene.FixedUpdate();
    Check(spawned.input == &input && spawned.time == &time,
          "Spawned object's component did not reach the same Input and Time.");
}

void TestMovementFollowsTime()
{
    // Scene을 마지막에 선언해 먼저 정리한다. Scene이 Input과 Time을 참조하기 때문이다.
    Input input;
    Time time(0.1, 3);
    Scene scene(input, time);
    GameObject& player = scene.CreateObject("mover");
    player.AddComponent(std::make_unique<PlayerControl>(player, 1.0f, 1));

    input.OnKeyDown(Key::W);
    for (int i = 0; i < 10; ++i)
        RunFrame(time, input, scene, 0.1);
    Check(Near(player.transform.position.z, 1.0), "Normal speed: 1 second must move 1 unit.");

    time.RequestPause(true);
    for (int i = 0; i < 5; ++i)
        RunFrame(time, input, scene, 0.1);
    Check(Near(player.transform.position.z, 1.0), "Paused time moved the player.");

    time.RequestPause(false);
    time.RequestScale(2.0);
    for (int i = 0; i < 5; ++i)
        RunFrame(time, input, scene, 0.1);
    Check(Near(player.transform.position.z, 2.0), "Double speed: 0.5 seconds must move 1 unit.");
}

void TestDiagonalSpeed()
{
    Input input;
    Time time(0.1, 3);
    Scene scene(input, time);
    GameObject& player = scene.CreateObject("diagonal");
    player.AddComponent(std::make_unique<PlayerControl>(player, 1.0f, 1));

    input.OnKeyDown(Key::W);
    input.OnKeyDown(Key::D);
    for (int i = 0; i < 10; ++i)
        RunFrame(time, input, scene, 0.1);
    const double x = player.transform.position.x;
    const double z = player.transform.position.z;
    Check(Near(std::sqrt(x * x + z * z), 1.0) && Near(x, z),
          "Diagonal movement must keep the same speed as straight movement.");
}

void TestTapInMultiTickFrame()
{
    Input input;
    Time time(0.1, 3);
    Scene scene(input, time);
    GameObject& player = scene.CreateObject("attacker");
    auto control = std::make_unique<PlayerControl>(player, 1.0f, 1);
    PlayerControl* borrowed = control.get();
    player.AddComponent(std::move(control));

    input.OnKeyDown(Key::Space);
    input.OnKeyUp(Key::Space);
    Check(RunFrame(time, input, scene, 0.05) == 0, "Short frame must not run a tick.");
    Check(borrowed->AttackCount() == 0, "Attack ran without a tick.");
    Check(RunFrame(time, input, scene, 0.3) == 3, "Long frame must run three ticks.");
    Check(borrowed->AttackCount() == 1, "Short tap must attack exactly once.");
}

void TestDeltaTime()
{
    Time time(0.02, 5);
    Check(Near(time.FixedDeltaTime(), 0.02), "FixedDeltaTime must be the tick length.");

    AdvanceFrame(time, 0.016);
    Check(Near(time.DeltaTime(), 0.016), "DeltaTime must follow the real frame time.");

    time.RequestScale(2.0);
    AdvanceFrame(time, 0.016);
    Check(Near(time.DeltaTime(), 0.032), "DeltaTime must apply the time scale.");

    time.RequestPause(true);
    AdvanceFrame(time, 0.016);
    Check(Near(time.DeltaTime(), 0.0), "DeltaTime must be zero while paused.");

    // 배속 2로 1초 프레임이면 게임 시간 2초지만, 5 tick × 0.02초 = 0.1초에서 자른다.
    time.RequestPause(false);
    AdvanceFrame(time, 1.0);
    Check(Near(time.DeltaTime(), 0.1), "A long frame must be capped at maxTicksPerFrame x FixedDeltaTime.");
    Check(Near(time.FixedDeltaTime(), 0.02), "FixedDeltaTime must not change with scale, pause or long frames.");
}

// 프레임마다 FixedUpdate와 Update가 불린 횟수와, Update에서 읽은 DeltaTime을 기록한다.
struct FrameRecord
{
    int fixedUpdates = 0;
    int updates = 0;
    double lastDeltaTime = -1.0;
};

class FrameRecorder : public Component
{
public:
    FrameRecorder(GameObject& owner, FrameRecord& record) : Component(owner), m_record(record) {}
    void FixedUpdate() override { ++m_record.fixedUpdates; }
    void Update() override
    {
        ++m_record.updates;
        m_record.lastDeltaTime = GetTime().DeltaTime();
    }
private:
    FrameRecord& m_record;
};

void TestUpdateCallsPerFrame()
{
    Input input;
    Time time(0.1, 3);
    Scene scene(input, time);
    FrameRecord record;
    GameObject& object = scene.CreateObject("recorder");
    object.AddComponent(std::make_unique<FrameRecorder>(object, record));

    // tick이 0번인 프레임: FixedUpdate는 없고, Update는 한 번이다.
    Check(RunFrame(time, input, scene, 0.05) == 0, "Short frame must not run a tick.");
    Check(record.fixedUpdates == 0 && record.updates == 1, "A frame without ticks must still call Update once.");
    Check(Near(record.lastDeltaTime, 0.05), "Update must read this frame's DeltaTime.");

    // tick이 3번인 프레임: FixedUpdate는 3번, Update는 그래도 한 번이다.
    Check(RunFrame(time, input, scene, 0.25) == 3, "Long frame must run three ticks.");
    Check(record.fixedUpdates == 3 && record.updates == 2,
          "A three-tick frame must call FixedUpdate three times and Update once.");

    // 정지한 프레임: FixedUpdate는 없고, Update는 DeltaTime 0으로 한 번 불린다.
    time.RequestPause(true);
    Check(RunFrame(time, input, scene, 0.3) == 0, "Paused frame must not run a tick.");
    Check(record.fixedUpdates == 3 && record.updates == 3 && Near(record.lastDeltaTime, 0.0),
          "A paused frame must call Update once with zero DeltaTime.");
}
}

int RunInputTimeTests()
{
    try
    {
        TestPressHoldRelease();
        TestTickResultStaysFixed();
        TestShortTapKeptOnce();
        TestKeyRepeatIgnored();
        TestMouseDelta();
        TestFocusLost();
        TestTickAccumulation();
        TestSimulationTimePerTick();
        TestPauseAtBoundary();
        TestScaleAtBoundary();
        TestCatchUpLimit();
        TestInvalidValues();
        TestInvalidKey();
        TestServiceAccessThroughOwner();
        TestMovementFollowsTime();
        TestDiagonalSpeed();
        TestTapInMultiTickFrame();
        TestDeltaTime();
        TestUpdateCallsPerFrame();
        std::cout << "Input/time tests PASS (19 cases)\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "Input/time tests FAIL: " << error.what() << '\n';
        return 1;
    }
}
