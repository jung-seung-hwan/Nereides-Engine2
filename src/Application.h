#pragma once
#include "Input.h"
#include "Renderer.h"
#include "Scene.h"
#include "Time.h"
#include "Window.h"
#include <memory>

// 어떻게 실행할지. 창 모드가 기본이고, 스크립트 데모는 같은 결과를 다시 확인하는 검증 모드다.
enum class RunMode
{
    Window,
    Demo
};

// 프로그램의 시작, 프레임 진행, 종료 순서를 담당한다.
// 창 모드: 창 메시지를 처리하고, 실제로 흐른 시간만큼 tick을 실행하고, 화면을 한 번 그린다.
// 스크립트 데모: 정해 둔 입력 사건과 프레임 길이로 단계 2와 같은 결과를 낸다.
class Application
{
public:
    explicit Application(RunMode mode);
    void Run();

private:
    void Initialize();
    void RunWindow();
    void RunDemo();
    void ReadDemoInput(int frame);
    int AdvanceDemoTime(int frame);
    void RunTick(int tick, int tickCount);
    void UpdateAnimation();
    void CheckCollision();
    void PrintDemoRender();
    void Shutdown();

    // 멤버는 선언 순서대로 생성되고 반대 순서로 정리된다. 참조되는 쪽을 먼저 선언한다.
    // Window는 Input을 참조한다. Renderer의 스왑 체인은 창에 그림을 보낸다. Scene은 Input과 Time을 참조한다.
    // 그래서 정리는 Scene, Renderer, Window, Time, Input 순서다.
    RunMode m_mode;
    Input m_input;
    Time m_time;
    std::unique_ptr<Window> m_window;     // 창 모드에서만 만든다
    std::unique_ptr<Renderer> m_renderer; // 창 모드에서만 만든다
    Scene m_scene;
};
