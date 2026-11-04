#include "Application.h"
#include "LogComponent.h"
#include "PlayerControl.h"
#include <chrono>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <memory>
#include <thread>

namespace
{
// 스크립트 데모는 단계 2와 같은 값을 쓴다. 같은 입력이면 같은 결과가 나와야 회귀 검사가 된다.
// 데모 프레임마다 흐른 실제 시간(초).
const double DemoFrameSeconds[] = {0.10, 0.05, 0.05, 0.10, 0.20, 1.00, 0.10};
// 로그를 읽기 쉽게 tick을 크게 잡는다.
const double DemoTickSeconds = 0.1;
const int DemoMaxTicksPerFrame = 3;

// 창 모드는 실제 게임에서 흔히 쓰는 값을 쓴다.
const double WindowTickSeconds = 1.0 / 60.0;
const int WindowMaxTicksPerFrame = 5;
const int WindowWidth = 960;
const int WindowHeight = 540;
// 창 모드의 위치 로그 간격. 30 tick은 0.5초다.
const int WindowLogEveryTicks = 30;

const float PlayerSpeed = 1.0f; // 초당 이동 거리

double TickSecondsFor(RunMode mode)
{
    if (mode == RunMode::Demo)
        return DemoTickSeconds;
    return WindowTickSeconds;
}

int MaxTicksFor(RunMode mode)
{
    if (mode == RunMode::Demo)
        return DemoMaxTicksPerFrame;
    return WindowMaxTicksPerFrame;
}
}

// Time과 Scene은 기본 생성자가 없으므로 초기화 목록에서 만든다.
// Scene에는 앞에서 만든 Input과 Time을 넘긴다. Window와 Renderer는 Initialize에서 창 모드일 때만 만든다.
Application::Application(RunMode mode)
    : m_mode(mode), m_time(TickSecondsFor(mode), MaxTicksFor(mode)), m_scene(m_input, m_time)
{
}

void Application::Run()
{
    std::cout << std::fixed << std::setprecision(2);
    Initialize();
    if (m_mode == RunMode::Window)
        RunWindow();
    else
        RunDemo();
    Shutdown();
}

void Application::Initialize()
{
    int logEveryTicks = 1;
    if (m_mode == RunMode::Demo)
    {
        std::cout << "Nereides Engine2 - Stage 2: input and time demo\n";
        std::cout << "Scripted input events and frame lengths. Run without arguments for the window mode.\n";
    }
    else
    {
        std::cout << "Nereides Engine2 - Stage 3-1: window and real-time loop\n";
        std::cout << "W/A/S/D: move, Space: attack, mouse: turn. Close the window to quit.\n";
        // 창을 먼저 만들어야 Renderer가 그 창에 스왑 체인을 붙일 수 있다.
        // Renderer까지 만든 뒤에 창을 보여 준다.
        m_window = std::make_unique<Window>(m_input, L"Nereides Engine2", WindowWidth, WindowHeight);
        m_renderer = std::make_unique<Renderer>(m_window->GetHandle(), m_window->GetWidth(), m_window->GetHeight());
        m_window->Show();
        logEveryTicks = WindowLogEveryTicks;
    }

    std::cout << "Initialize\n";
    GameObject& player = m_scene.CreateObject("Player");
    player.transform.position.x = -2;
    player.AddComponent(std::make_unique<PlayerControl>(player, PlayerSpeed, logEveryTicks));
    GameObject& kraken = m_scene.CreateObject("Kraken");
    kraken.transform.position.x = 2;
    kraken.AddComponent(std::make_unique<LogComponent>(kraken));
    kraken.SetActive(false);
}

void Application::RunWindow()
{
    bool wasMinimized = false;
    std::chrono::steady_clock::time_point last = std::chrono::steady_clock::now();

    // 1. 쌓인 창 메시지를 처리한다. 키·마우스는 이 안에서 Input에 수집된다.
    while (m_window->ProcessMessages())
    {
        // 2. 창 상태를 반영한다. Window는 기록만 했고, 크기 변경과 정지 요청은 메시지 처리가 끝난 여기서 한다.
        const bool minimized = m_window->IsMinimized();
        // 최소화 중에는 스왑 체인 크기를 바꾸지 않는다. 바뀐 크기는 기록에 남겨 두었다가 복원한 뒤 반영한다.
        if (!minimized && m_window->TakeSizeChanged())
            m_renderer->Resize(m_window->GetWidth(), m_window->GetHeight());
        if (minimized != wasMinimized)
        {
            // 최소화되면 정지를, 복원되면 재개를 요청한다. 다음 BeginFrame 시작에 적용된다.
            m_time.RequestPause(minimized);
            wasMinimized = minimized;
            if (minimized)
            {
                std::cout << "Window minimized: game time paused\n";
            }
            else
            {
                std::cout << "Window restored: game time resumed\n";
                // 복원한 프레임은 시계 기준을 다시 잡는다. 최소화 동안 흐른 시간이 이동으로 나타나지 않게 한다.
                // 크기 변경을 마친 뒤에 잡아서, 복원하며 걸린 시간도 tick에 넣지 않는다.
                last = std::chrono::steady_clock::now();
            }
        }

        // 3. 지난 프레임부터 흐른 실제 시간을 재서 Time에 넣고, 정해진 횟수만큼 tick을 실행한다.
        const std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
        const double realSeconds = std::chrono::duration<double>(now - last).count();
        last = now;

        const int tickCount = m_time.BeginFrame(realSeconds);
        for (int tick = 1; tick <= tickCount; ++tick)
            RunTick(tick, tickCount);

        // 4. 화면을 지우고 창에 보낸다. 최소화 중에는 보이는 화면이 없어 그리지 않는다.
        if (minimized)
        {
            // Present의 기다림이 없으면 루프가 CPU를 다 쓰므로 잠깐 쉰다.
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
        }
        m_renderer->Clear(0.08f, 0.12f, 0.20f);
        m_renderer->Present();
    }
}

void Application::RunDemo()
{
    const int frameCount = static_cast<int>(std::size(DemoFrameSeconds));
    for (int frame = 1; frame <= frameCount; ++frame)
    {
        std::cout << "\nFrame " << frame << '\n';
        ReadDemoInput(frame);
        const int tickCount = AdvanceDemoTime(frame);
        for (int tick = 1; tick <= tickCount; ++tick)
            RunTick(tick, tickCount);
        if (tickCount == 0)
            std::cout << "  (no tick: collected input waits for the next tick)\n";
        PrintDemoRender();
    }
}

void Application::ReadDemoInput(int frame)
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

int Application::AdvanceDemoTime(int frame)
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
    // tick 단계 로그는 스크립트 데모에서만 쓴다. 창 모드에서는 초당 60번이라 너무 많다.
    const bool showSteps = m_mode == RunMode::Demo;

    // tick 안의 시뮬레이션 시간은 이 tick이 시작한 시각이다.
    if (showSteps)
        std::cout << "  Tick " << tick << '/' << tickCount << " sim=" << m_time.SimulationTime() << '\n';
    // tick마다 한 번 소비한다. 행동 Component는 이 결과만 읽는다.
    m_input.ConsumeForTick();
    if (showSteps)
        std::cout << "    3. Objects\n";
    m_scene.Update();
    UpdateAnimation();
    CheckCollision();
    // tick 하나를 마쳤다. 시뮬레이션 시간이 tick 길이만큼 늘어난다.
    m_time.FinishTick();
}

void Application::UpdateAnimation()
{
    if (m_mode == RunMode::Demo)
        std::cout << "    4. Animation [placeholder]\n";
}

void Application::CheckCollision()
{
    if (m_mode == RunMode::Demo)
        std::cout << "    5. Collision [placeholder]\n";
}

void Application::PrintDemoRender()
{
    std::cout << "  6. Render    [placeholder] once per frame | sim=" << m_time.SimulationTime() << '\n';
}

void Application::Shutdown()
{
    std::cout << "\nShutdown\n";
    m_scene.Clear();
}
