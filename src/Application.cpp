#include "Application.h"
#include "CameraComponent.h"
#include "CollectRenderFrame.h"
#include "LogComponent.h"
#include "ModelRenderer.h"
#include "PlayerControl.h"
#include <chrono>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <memory>
#include <thread>
#include <utility>

namespace
{
// 스크립트 데모는 단계 2와 같은 값을 쓴다. 같은 입력이면 같은 결과가 나와야 회귀 검사가 된다.
// 데모 프레임마다 흐른 실제 시간(초).
const double DemoFrameSeconds[] = {0.10, 0.05, 0.05, 0.10, 0.20, 1.00, 0.10};
// 로그를 읽기 쉽게 tick을 크게 잡는다.
const double DemoFixedDeltaTime = 0.1;
const int DemoMaxTicksPerFrame = 3;

// 창 모드의 tick 길이는 유니티의 기본 고정 간격과 같은 0.02초다. 1초에 50번이다.
const double WindowFixedDeltaTime = 0.02;
const int WindowMaxTicksPerFrame = 5;
const int WindowWidth = 960;
const int WindowHeight = 540;
// 창 모드의 위치 로그 간격. 0.02초 × 25 tick = 0.5초다.
const int WindowLogEveryTicks = 25;

const float PlayerSpeed = 1.0f; // 초당 이동 거리

// 창 모드 장면의 값. 바닥 위에 상자 두 개를 놓고, 카메라가 앞 위에서 내려다본다.
const DirectX::XMFLOAT4 FloorColor = {0.45f, 0.47f, 0.50f, 1.0f};
const DirectX::XMFLOAT4 PlayerColor = {0.25f, 0.50f, 0.95f, 1.0f};
const DirectX::XMFLOAT4 KrakenColor = {0.85f, 0.30f, 0.30f, 1.0f};
const float CameraFovDegrees = 60.0f;
const float CameraNearPlane = 0.1f;
const float CameraFarPlane = 100.0f;
const float CameraPitchRadians = 0.44f; // 약 25도. 양수면 아래를 본다

double FixedDeltaTimeFor(RunMode mode)
{
    if (mode == RunMode::Demo)
        return DemoFixedDeltaTime;
    return WindowFixedDeltaTime;
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
    : m_mode(mode), m_time(FixedDeltaTimeFor(mode), MaxTicksFor(mode)), m_scene(m_input, m_time)
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
        std::cout << "Nereides Engine2 - Stage 3-3: Update and FixedUpdate\n";
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
    if (m_mode == RunMode::Demo)
    {
        // 스크립트 데모는 단계 2 그대로다. 단계 3-3에서 더한 Update 줄과 Render 번호 말고는 출력이 같아야 회귀 검사가 된다.
        kraken.AddComponent(std::make_unique<LogComponent>(kraken));
        kraken.SetActive(false);
        return;
    }
    BuildWindowScene(player, kraken);
}

void Application::BuildWindowScene(GameObject& player, GameObject& kraken)
{
    // 메시 원본을 먼저 채운다. ModelRenderer는 이 원본을 가리키기만 한다.
    m_boxMesh = MakeBox();
    m_floorMesh = MakeFloor();

    // 상자는 중심이 원점이라 높이의 절반만큼 올려 바닥 위에 세운다.
    player.transform.position.y = 0.5f;
    player.AddComponent(std::make_unique<ModelRenderer>(player, m_boxMesh, PlayerColor));

    // 비활성 객체는 그리지 않으므로 창 모드의 Kraken은 켜 둔다.
    // LogComponent는 달지 않는다. 켜 두면 tick마다, 곧 초당 50줄을 찍기 때문이다.
    kraken.transform.position.y = 1.0f;
    kraken.transform.scale = {2.0f, 2.0f, 2.0f};
    kraken.AddComponent(std::make_unique<ModelRenderer>(kraken, m_boxMesh, KrakenColor));

    GameObject& floor = m_scene.CreateObject("Floor");
    floor.transform.scale = {20.0f, 1.0f, 20.0f};
    floor.AddComponent(std::make_unique<ModelRenderer>(floor, m_floorMesh, FloorColor));

    GameObject& cameraObject = m_scene.CreateObject("Camera");
    cameraObject.transform.position = {0.0f, 4.0f, -8.0f};
    cameraObject.transform.rotation.x = CameraPitchRadians;
    auto camera = std::make_unique<CameraComponent>(cameraObject, CameraFovDegrees, CameraNearPlane, CameraFarPlane);
    // GameObject에 넘기기 전에 주소를 기억해 둔다. unique_ptr을 옮겨도 가리키는 객체의 주소는 그대로다.
    m_camera = camera.get();
    cameraObject.AddComponent(std::move(camera));
}

void Application::RunWindow()
{
    bool wasMinimized = false;
    std::chrono::steady_clock::time_point last = std::chrono::steady_clock::now();

    // 종료 조건은 루프 안의 if에서 확인한다. 닫기 요청을 받으면 break로 루프를 빠져나가 Shutdown으로 간다.
    while (true)
    {
        // 1. 쌓인 창 메시지를 처리한다. 키·마우스는 이 안에서 Input에 수집된다.
        m_window->ProcessMessages();
        // 닫기 요청을 받았으면 이번 프레임은 tick도 그리기도 하지 않고 끝낸다.
        if (m_window->IsCloseRequested())
            break;

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

        // 3. 지난 프레임부터 흐른 실제 시간을 재서 게임을 한 프레임만큼 진행한다. tick들과 Update가 이 안에서 돈다.
        // 이 시간은 정지·배속을 적용하기 전 값이라 unscaledDeltaTime이라 부른다.
        // 게임 코드는 이 값 대신 FixedUpdate에서 FixedDeltaTime()을, Update에서 DeltaTime()을 읽는다.
        const std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
        const double unscaledDeltaTime = std::chrono::duration<double>(now - last).count();
        last = now;
        // 최소화(정지) 중에도 부른다. 그때 tick은 0번이고, Update는 DeltaTime() 0으로 한 번 불린다.
        AdvanceGame(unscaledDeltaTime);

        // 최소화 중에는 보이는 화면이 없어 수집도 그리기도 하지 않는다.
        if (minimized)
        {
            // Present의 기다림이 없으면 루프가 CPU를 다 쓰므로 잠깐 쉰다.
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
        }

        // 4. 모든 tick과 Update가 끝난 뒤 Scene을 읽어 이번 프레임에 그릴 값을 만든다.
        // 보간 계수만큼 직전 tick과 현재 tick 사이를 섞는다. 화면 비율은 창 안쪽의 너비 ÷ 높이다.
        const float aspect = static_cast<float>(m_window->GetWidth()) / static_cast<float>(m_window->GetHeight());
        const float alpha = static_cast<float>(m_time.InterpolationAlpha());
        const RenderFrame frame = CollectRenderFrame(m_scene, *m_camera, aspect, alpha);

        // 5. 지우기, 그리기, 창에 보내기를 한 번에 한다.
        m_renderer->Render(frame);
    }
}

void Application::RunDemo()
{
    const int frameCount = static_cast<int>(std::size(DemoFrameSeconds));
    for (int frame = 1; frame <= frameCount; ++frame)
    {
        std::cout << "\nFrame " << frame << '\n';
        ReadDemoInput(frame);
        // 창 모드와 같은 함수로 게임을 진행한다. 실제로 잰 시간 대신 정해 둔 프레임 길이를 넣는다.
        AdvanceGame(DemoFrameSeconds[frame - 1]);
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

void Application::AdvanceGame(double unscaledDeltaTime)
{
    // 프레임 경계: 정지·배속을 적용하고, 이번 프레임의 tick 수와 DeltaTime()을 정한다.
    const int tickCount = m_time.BeginFrame(unscaledDeltaTime);
    if (m_mode == RunMode::Demo)
        PrintDemoTime(unscaledDeltaTime, tickCount);

    for (int tick = 1; tick <= tickCount; ++tick)
        RunTick(tick, tickCount);

    // 모든 tick이 끝난 뒤 프레임마다 한 번 Update를 부른다. tick이 0번인 프레임과 정지 중에도 부른다.
    if (m_mode == RunMode::Demo)
    {
        if (tickCount == 0)
            std::cout << "  (no tick: collected input waits for the next tick)\n";
        std::cout << "  6. Update    once per frame, after all ticks\n";
    }
    m_scene.Update();
}

void Application::PrintDemoTime(double unscaledDeltaTime, int tickCount)
{
    std::cout << "  2. Time      real+" << unscaledDeltaTime
              << " ticks=" << tickCount
              << " scale=" << m_time.Scale()
              << " paused=" << (m_time.IsPaused() ? "yes" : "no")
              << " | total: real=" << m_time.RealTime();
    if (m_time.DroppedTime() > 0.0)
        std::cout << " dropped=" << m_time.DroppedTime();
    std::cout << '\n';
}

void Application::RunTick(int tick, int tickCount)
{
    // tick 단계 로그는 스크립트 데모에서만 쓴다. 창 모드에서는 초당 50번이라 너무 많다.
    const bool showSteps = m_mode == RunMode::Demo;

    // tick 안의 시뮬레이션 시간은 이 tick이 시작한 시각이다.
    if (showSteps)
        std::cout << "  Tick " << tick << '/' << tickCount << " sim=" << m_time.SimulationTime() << '\n';
    // tick마다 한 번 소비한다. 행동 Component는 이 결과만 읽는다.
    m_input.ConsumeForTick();
    // 보간에 쓸 직전 Transform을, 이번 tick이 객체를 움직이기 전에 복사한다.
    m_scene.SavePreviousTransforms();
    if (showSteps)
        std::cout << "    3. Objects\n";
    // 게임 상태를 바꾸는 갱신. 컴포넌트는 FixedDeltaTime()과 이번 tick의 입력을 읽는다.
    m_scene.FixedUpdate();
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
    std::cout << "  7. Render    [placeholder] once per frame | sim=" << m_time.SimulationTime() << '\n';
}

void Application::Shutdown()
{
    std::cout << "\nShutdown\n";
    m_scene.Clear();
}
