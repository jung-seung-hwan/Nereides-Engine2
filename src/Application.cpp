#include "Application.h"
#include "LogComponent.h"
#include "PlayerControl.h"
#include <iomanip>
#include <iostream>
#include <iterator>
#include <memory>

namespace
{
// 데모 프레임마다 흐른 실제 시간(초). 창 루프가 생기면 실제로 잰 값으로 바뀐다.
const double DemoFrameSeconds[] = {0.10, 0.05, 0.05, 0.10, 0.20, 1.00, 0.10};

// 로그를 읽기 쉽게 tick을 크게 잡는다. 실제 게임은 보통 1/60초를 쓴다.
const double DemoTickSeconds = 0.1;
const int DemoMaxTicksPerFrame = 3;
const float PlayerSpeed = 1.0f; // 초당 이동 거리
}

// Time은 기본 생성자가 없으므로 초기화 목록에서 만든다.
Application::Application() : m_time(DemoTickSeconds, DemoMaxTicksPerFrame)
{
}

void Application::Run()
{
    std::cout << std::fixed << std::setprecision(2);
    Initialize();

    const int frameCount = static_cast<int>(std::size(DemoFrameSeconds));
    for (int frame = 1; frame <= frameCount; ++frame)
    {
        std::cout << "\nFrame " << frame << '\n';
        ReadInput(frame);
        const int tickCount = AdvanceTime(frame);
        for (int tick = 1; tick <= tickCount; ++tick)
            RunTick(tick, tickCount);
        if (tickCount == 0)
            std::cout << "  (no tick: collected input waits for the next tick)\n";
        Render();
    }

    Shutdown();
}

void Application::Initialize()
{
    std::cout << "Nereides Engine2 - Stage 2: input and time demo\n";
    std::cout << "Input events and frame lengths are scripted. No window, clock, animation, collision, or DX11 yet.\n";
    std::cout << "Initialize\n";
    GameObject& player = m_scene.CreateObject("Player");
    player.transform.position.x = -2;
    player.AddComponent(std::make_unique<PlayerControl>(player, m_input, m_time, PlayerSpeed));
    GameObject& kraken = m_scene.CreateObject("Kraken");
    kraken.transform.position.x = 2;
    kraken.AddComponent(std::make_unique<LogComponent>(kraken));
    kraken.SetActive(false);
}

void Application::ReadInput(int frame)
{
    std::cout << "  1. Input     [demo events]\n";

    // 창 메시지 대신 정해 둔 사건을 넣는다. 수집만 하고 아직 소비하지 않는다.
    if (frame == 1)
    {
        std::cout << "    W down, mouse x+30\n";
        m_input.OnKeyDown(Key::W);
        m_input.OnMouseMove(30, 0);
    }
    else if (frame == 2)
    {
        std::cout << "    Space down, Space up (short tap)\n";
        m_input.OnKeyDown(Key::Space);
        m_input.OnKeyUp(Key::Space);
    }
    else if (frame == 4)
    {
        std::cout << "    request time scale 2\n";
        m_time.RequestScale(2.0);
    }
    else if (frame == 5)
    {
        std::cout << "    request pause\n";
        m_time.RequestPause(true);
    }
    else if (frame == 6)
    {
        std::cout << "    request resume, time scale 1\n";
        m_time.RequestPause(false);
        m_time.RequestScale(1.0);
    }
    else if (frame == 7)
    {
        std::cout << "    focus lost\n";
        m_input.OnFocusLost();
    }
}

int Application::AdvanceTime(int frame)
{
    const double realSeconds = DemoFrameSeconds[frame - 1];
    const int tickCount = m_time.BeginFrame(realSeconds);

    std::cout << "  2. Time      real+" << realSeconds
              << " ticks=" << tickCount
              << " scale=" << m_time.Scale()
              << " paused=" << (m_time.IsPaused() ? "yes" : "no")
              << " | total: real=" << m_time.RealTime();
    if (m_time.DroppedTime() > 0.0)
        std::cout << " dropped=" << m_time.DroppedTime();
    std::cout << '\n';
    return tickCount;
}

void Application::RunTick(int tick, int tickCount)
{
    // tick 안의 시뮬레이션 시간은 이 tick이 시작한 시각이다.
    std::cout << "  Tick " << tick << '/' << tickCount << " sim=" << m_time.SimulationTime() << '\n';
    // tick마다 한 번 소비한다. 행동 Component는 이 결과만 읽는다.
    m_input.ConsumeForTick();
    std::cout << "    3. Objects\n";
    m_scene.Update();
    UpdateAnimation();
    CheckCollision();
    // tick 하나를 마쳤다. 시뮬레이션 시간이 tick 길이만큼 늘어난다.
    m_time.FinishTick();
}

void Application::UpdateAnimation()
{
    std::cout << "    4. Animation [placeholder]\n";
}

void Application::CheckCollision()
{
    std::cout << "    5. Collision [placeholder]\n";
}

void Application::Render()
{
    std::cout << "  6. Render    [placeholder] once per frame | sim=" << m_time.SimulationTime() << '\n';
}

void Application::Shutdown()
{
    std::cout << "\nShutdown\n";
    m_scene.Clear();
}
