#include "Application.h"
#include <iostream>
#include "LogComponent.h"
#include <memory>

void Application::Run()
{
    Initialize();

    const int frameCount = 5;
    for (int frame = 1; frame <= frameCount; ++frame)
    {
        std::cout << "\nFrame " << frame << '\n';
        ReadInput();
        AdvanceTime();
        UpdateObjects(frame);
        UpdateAnimation();
        CheckCollision();
        Render();
    }

    Shutdown();
}

void Application::Initialize()
{
    std::cout << "Nereides Engine2 - Stage 1: object lifetime demo\n";
    std::cout << "No real input, clock, animation, collision, or DX11 yet.\n";
    std::cout << "Initialize\n";
    GameObject& player = m_scene.CreateObject("Player");
    player.transform.position.x = -2;
    player.AddComponent(std::make_unique<LogComponent>(player));
    GameObject& kraken = m_scene.CreateObject("Kraken");
    kraken.transform.position.x = 2;
    kraken.AddComponent(std::make_unique<LogComponent>(kraken));
    kraken.SetActive(false);
}

void Application::ReadInput()
{
    std::cout << "  1. Input     [placeholder]\n";
}

void Application::AdvanceTime()
{
    std::cout << "  2. Time      [placeholder]\n";
}

void Application::UpdateObjects(int frame)
{
    std::cout << "  3. Objects\n";
    m_scene.Update(frame);
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
    m_scene.Clear();
}
