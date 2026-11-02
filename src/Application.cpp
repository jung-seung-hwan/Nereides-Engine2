#include "Application.h"
#include <iostream>

void Application::Run()
{
    Initialize();

    const int frameCount = 3;
    for (int frame = 1; frame <= frameCount; ++frame)
    {
        std::cout << "\nFrame " << frame << '\n';
        ReadInput();
        AdvanceTime();
        UpdateObjects();
        UpdateAnimation();
        CheckCollision();
        Render();
    }

    Shutdown();
}

void Application::Initialize()
{
    std::cout << "Stage 0: execution order demo\n";
    std::cout << "No real input, clock, animation, collision, or DX11 yet.\n";
    std::cout << "Initialize\n";
}

void Application::ReadInput()
{
    std::cout << "  1. Input     [placeholder]\n";
}

void Application::AdvanceTime()
{
    std::cout << "  2. Time      [placeholder]\n";
}

void Application::UpdateObjects()
{
    std::cout << "  3. Objects   [placeholder]\n";
}

void Application::UpdateAnimation()
{
    std::cout << "  4. Animation [placeholder]\n";
}

void Application::CheckCollision()
{
    std::cout << "  5. Collision [placeholder]\n";
}

void Application::Render()
{
    std::cout << "  6. Render    [placeholder]\n";
}

void Application::Shutdown()
{
    std::cout << "\nShutdown\n";
}
