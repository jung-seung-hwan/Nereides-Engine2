#pragma once
#include "Scene.h"

// 프로그램의 시작, 프레임 진행, 종료 순서를 담당한다.
// 객체 갱신은 Scene에 맡긴다. 나머지 단계는 아직 자리 표시자다.
class Application
{
public:
    void Run();

private:
    Scene m_scene;
    void Initialize();
    void ReadInput();
    void AdvanceTime();
    void UpdateObjects(int frame);
    void UpdateAnimation();
    void CheckCollision();
    void Render();
    void Shutdown();
};
