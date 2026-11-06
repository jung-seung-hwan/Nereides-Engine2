#pragma once
#include "AssetManager.h"
#include "Input.h"
#include "RenderFrame.h"
#include "Renderer.h"
#include "Scene.h"
#include "Time.h"
#include "Window.h"
#include <memory>

class CameraComponent;

// 어떻게 실행할지. 창 모드가 기본이고, 스크립트 데모는 같은 결과를 다시 확인하는 검증 모드다.
enum class RunMode
{
    Window,
    Demo
};

// 프로그램의 시작, 프레임 진행, 종료 순서를 담당한다.
// 창 모드: 창 메시지를 처리하고, 실제로 흐른 시간만큼 tick(FixedUpdate)을 실행하고, Update를 한 번 부르고, 장면을 한 번 그린다.
// 스크립트 데모: 정해 둔 입력 사건과 프레임 길이로 단계 2와 같은 결과를 낸다.
class Application
{
public:
    explicit Application(RunMode mode);
    void Run();

private:
    void Initialize();
    void BuildWindowScene(GameObject& player, GameObject& kraken);
    void RunWindow();
    void RunDemo();
    void ReadDemoInput(int frame);
    // 게임을 한 프레임만큼 진행한다: tick 수를 정하고, tick마다 RunTick을, 모든 tick 뒤에 Update를 한 번 부른다.
    // 창 모드와 스크립트 데모가 같이 쓴다. 그래서 데모 출력으로 창 모드와 같은 순서를 확인할 수 있다.
    // RunWindow 주석의 3번이 이 함수다. 데모 출력의 2번(Time), tick 안의 3~5번, 6번(Update)이 이 안에서 찍힌다.
    void AdvanceGame(double unscaledDeltaTime);
    void PrintDemoTime(double unscaledDeltaTime, int tickCount);
    void RunTick(int tick, int tickCount);
    void UpdateAnimation();
    void CheckCollision();
    void PrintDemoRender();
    void Shutdown();

    // 멤버는 선언 순서대로 생성되고 반대 순서로 정리된다. 참조되는 쪽을 먼저 선언한다.
    // Window는 Input을 참조한다. Renderer의 스왑 체인은 창에 그림을 보낸다. Scene은 Input과 Time을 참조한다.
    // Scene 안의 ModelRenderer는 메시 원본(AssetManager의 모델이나 아래 MeshData)을 가리키고,
    // Renderer는 원본 주소로 GPU 버퍼를 기억한다. 그래서 정리는 Scene, Renderer, Window, 원본들, Time, Input 순서다.
    RunMode m_mode;
    Input m_input;
    Time m_time;
    AssetManager m_assets;                // 불러온 모델 원본의 주인. 창 모드에서 BuildWindowScene이 채운다
    MeshData m_floorMesh;                 // 창 모드에서 채운다. 코드로 만드는 바닥은 그대로 둔다
    std::unique_ptr<Window> m_window;     // 창 모드에서만 만든다
    std::unique_ptr<Renderer> m_renderer; // 창 모드에서만 만든다
    Scene m_scene;
    // 창 모드의 카메라. 가리키기만 하고, 소유자는 카메라 GameObject다. 카메라 객체는 Shutdown 전까지 지우지 않는다.
    const CameraComponent* m_camera = nullptr;
};
