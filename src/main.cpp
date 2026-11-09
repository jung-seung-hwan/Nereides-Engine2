#include "AnimationTests.h"
#include "Application.h"
#include "AssetManager.h"
#include "AssetTests.h"
#include "InputTimeTests.h"
#include "LifecycleTests.h"
#include "RenderTests.h"
#include "WindowTests.h"
#include <exception>
#include <iostream>
#include <string>

namespace
{
void PrintUsage()
{
    std::cerr << "Usage: NereidesEngine2.exe [--demo | --lifecycle-tests | --input-time-tests | --window-tests"
                 " | --render-tests | --asset-tests | --animation-tests]\n";
    std::cerr << "Run without arguments to open the window.\n";
}
}

int main(int argc, char* argv[])
{
    // 실행 중에 던져진 예외는 여기서 받는다. 원인을 출력하고 종료 코드 1을 돌려준다.
    try
    {
        RunMode mode = RunMode::Window;
        if (argc > 2)
        {
            PrintUsage();
            return 2;
        }
        if (argc == 2)
        {
            const std::string option = argv[1];
            if (option == "--lifecycle-tests")
                return RunLifecycleTests();
            if (option == "--input-time-tests")
                return RunInputTimeTests();
            if (option == "--window-tests")
                return RunWindowTests();
            if (option == "--render-tests")
                return RunRenderTests();
            if (option == "--asset-tests")
                return RunAssetTests(AssetManager::FindRootFolder());
            if (option == "--animation-tests")
                return RunAnimationTests(AssetManager::FindRootFolder());
            if (option != "--demo")
            {
                // 알 수 없는 인자로 창이 열리면 CI가 멈춘다. 사용법을 알리고 끝낸다.
                PrintUsage();
                return 2;
            }
            mode = RunMode::Demo;
        }

        Application application(mode);
        application.Run();
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "Error: " << error.what() << '\n';
        return 1;
    }
}
