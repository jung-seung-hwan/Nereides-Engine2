#pragma once

// 프로그램의 시작, 프레임 진행, 종료 순서를 담당한다.
// 이번 단계의 모듈 함수들은 로그만 출력하는 자리 표시자다.
class Application
{
public:
    void Run();

private:
    void Initialize();
    void ReadInput();
    void AdvanceTime();
    void UpdateObjects();
    void UpdateAnimation();
    void CheckCollision();
    void Render();
    void Shutdown();
};
