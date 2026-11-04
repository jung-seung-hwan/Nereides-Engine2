#pragma once
#include "Input.h"
#include "Scene.h"
#include "Time.h"

// 프로그램의 시작, 프레임 진행, 종료 순서를 담당한다.
// 프레임마다 입력을 모으고, Time이 정한 횟수만큼 tick을 실행하고, 한 번 그린다.
class Application
{
public:
    Application();
    void Run();

private:
    void Initialize();
    void ReadInput(int frame);
    int AdvanceTime(int frame);
    void RunTick(int tick, int tickCount);
    void UpdateAnimation();
    void CheckCollision();
    void Render();
    void Shutdown();

    // 멤버는 선언 순서대로 생성되고 반대 순서로 정리된다.
    // Scene이 Input과 Time을 참조하므로 Scene을 마지막에 선언해 가장 먼저 정리한다.
    // 서비스 참조를 보관하는 곳은 Scene 한 곳뿐이다. 컴포넌트는 소유자와 Scene을 거쳐 읽는다.
    Input m_input;
    Time m_time;
    Scene m_scene;
};
