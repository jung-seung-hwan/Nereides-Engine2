#include "WindowTests.h"
#include "Input.h"
#include "Window.h"
#include <iostream>
#include <stdexcept>
#include <string>

// 보이지 않는 창에 메시지를 직접 보내서 Window의 메시지 규칙을 확인한다.
// SendMessageW는 같은 스레드가 만든 창이면 WindowProc를 바로 부른다. 그래서 메시지 처리 루프 없이 확인할 수 있다.
namespace
{
void Check(bool condition, const std::string& message)
{
    if (!condition)
        throw std::runtime_error(message);
}

struct KeyCase
{
    WPARAM virtualKey;
    Action action;
    const char* name;
};

const KeyCase KeyCases[] = {
    {'W', Action::MoveForward, "W"},
    {'A', Action::MoveLeft, "A"},
    {'S', Action::MoveBack, "S"},
    {'D', Action::MoveRight, "D"},
    {VK_SPACE, Action::Attack, "Space"},
};

// repeat가 true면 lParam의 30번 비트를 켠다. OS가 보내는 자동 반복 신호와 같다.
void SendKeyDown(Window& window, WPARAM virtualKey, bool repeat)
{
    LPARAM flags = 0;
    if (repeat)
        flags = 0x40000000;
    SendMessageW(window.GetHandle(), WM_KEYDOWN, virtualKey, flags);
}

void SendKeyUp(Window& window, WPARAM virtualKey)
{
    SendMessageW(window.GetHandle(), WM_KEYUP, virtualKey, 0);
}

void SendMouseMove(Window& window, int x, int y)
{
    SendMessageW(window.GetHandle(), WM_MOUSEMOVE, 0, MAKELPARAM(x, y));
}

void SendFocusLost(Window& window)
{
    SendMessageW(window.GetHandle(), WM_KILLFOCUS, 0, 0);
}

void SendSize(Window& window, WPARAM kind, int width, int height)
{
    SendMessageW(window.GetHandle(), WM_SIZE, kind, MAKELPARAM(width, height));
}

void TestKeyMessagesMapToActions()
{
    Input input;
    Window window(input, L"window test", 320, 240);

    for (const KeyCase& keyCase : KeyCases)
        SendKeyDown(window, keyCase.virtualKey, false);
    input.ConsumeForTick();
    for (const KeyCase& keyCase : KeyCases)
    {
        Check(input.IsPressed(keyCase.action) && input.IsHeld(keyCase.action),
              std::string(keyCase.name) + " key down must press its action.");
    }

    for (const KeyCase& keyCase : KeyCases)
        SendKeyUp(window, keyCase.virtualKey);
    input.ConsumeForTick();
    for (const KeyCase& keyCase : KeyCases)
    {
        Check(input.IsReleased(keyCase.action) && !input.IsHeld(keyCase.action),
              std::string(keyCase.name) + " key up must release its action.");
    }
}

void TestRepeatSignalIgnoredAfterFocusLoss()
{
    Input input;
    Window window(input, L"window test", 320, 240);

    SendKeyDown(window, 'W', false);
    input.ConsumeForTick();
    Check(input.IsHeld(Action::MoveForward), "W must be held before the focus is lost.");

    // 키를 누르고 있는 동안 오는 자동 반복 신호는 새 누름이 아니다.
    SendKeyDown(window, 'W', true);
    input.ConsumeForTick();
    Check(!input.IsPressed(Action::MoveForward) && input.IsHeld(Action::MoveForward),
          "A repeat signal while W is held must not press W again.");

    // 포커스를 잃은 뒤 키를 누른 채 돌아오면 OS는 자동 반복 신호를 보낸다.
    SendFocusLost(window);
    SendKeyDown(window, 'W', true);
    input.ConsumeForTick();
    Check(!input.IsPressed(Action::MoveForward) && !input.IsHeld(Action::MoveForward),
          "A repeat signal after the focus loss must not press W.");

    SendKeyDown(window, 'W', false);
    input.ConsumeForTick();
    Check(input.IsPressed(Action::MoveForward) && input.IsHeld(Action::MoveForward),
          "A new press after the focus loss must press W.");
}

void TestUnmappedKeysIgnored()
{
    Input input;
    Window window(input, L"window test", 320, 240);

    SendKeyDown(window, 'Q', false);
    SendKeyDown(window, VK_SHIFT, false);
    SendKeyDown(window, VK_RETURN, false);
    // Alt를 누른 채 누른 키는 WM_SYSKEYDOWN으로 온다. OS에 맡기므로 W라도 Input에 들어가지 않는다.
    SendMessageW(window.GetHandle(), WM_SYSKEYDOWN, 'W', 0);
    input.ConsumeForTick();
    for (const KeyCase& keyCase : KeyCases)
    {
        Check(!input.IsPressed(keyCase.action) && !input.IsHeld(keyCase.action),
              std::string("Unmapped keys and Alt keys must not press ") + keyCase.name + ".");
    }
}

void TestMouseMoveBecomesDelta()
{
    Input input;
    Window window(input, L"window test", 320, 240);

    SendMouseMove(window, 100, 100);
    input.ConsumeForTick();
    Check(input.MouseDeltaX() == 0 && input.MouseDeltaY() == 0, "The first move must only set the reference point.");

    // 오른쪽·위로 갔다가 왼쪽·위로 간다. 차이는 음수까지 그대로 더해진다.
    SendMouseMove(window, 110, 95);
    SendMouseMove(window, 95, 90);
    input.ConsumeForTick();
    Check(input.MouseDeltaX() == -5 && input.MouseDeltaY() == -10,
          "Mouse moves must add up as differences, negative ones included.");
}

void TestMouseReferenceResetAfterFocusLossAndLeave()
{
    Input input;
    Window window(input, L"window test", 320, 240);

    SendMouseMove(window, 100, 100);
    SendMouseMove(window, 120, 100);
    input.ConsumeForTick();
    Check(input.MouseDeltaX() == 20, "The move before the focus loss must count.");

    SendFocusLost(window);
    SendMouseMove(window, 300, 300);
    input.ConsumeForTick();
    Check(input.MouseDeltaX() == 0 && input.MouseDeltaY() == 0,
          "After the focus loss the first move must only set the reference point.");

    SendMouseMove(window, 310, 300);
    input.ConsumeForTick();
    Check(input.MouseDeltaX() == 10, "Moves after the new reference point must count.");

    SendMessageW(window.GetHandle(), WM_MOUSELEAVE, 0, 0);
    SendMouseMove(window, 50, 50);
    input.ConsumeForTick();
    Check(input.MouseDeltaX() == 0 && input.MouseDeltaY() == 0,
          "After the cursor leaves the window the first move must only set the reference point.");
}

void TestSizeAndMinimizeRecorded()
{
    Input input;
    Window window(input, L"window test", 320, 240);
    // 창을 만드는 동안 들어온 기록은 지우고 시작한다.
    window.TakeSizeChanged();

    SendSize(window, SIZE_RESTORED, 800, 600);
    Check(window.TakeSizeChanged() && window.GetWidth() == 800 && window.GetHeight() == 600,
          "A new size must be recorded.");
    Check(!window.TakeSizeChanged(), "A size change must be reported once.");

    SendSize(window, SIZE_MINIMIZED, 0, 0);
    Check(window.IsMinimized(), "Minimizing must be recorded.");
    Check(!window.TakeSizeChanged() && window.GetWidth() == 800 && window.GetHeight() == 600,
          "The zero size of a minimized window must not be recorded.");

    SendSize(window, SIZE_RESTORED, 800, 600);
    Check(!window.IsMinimized(), "Restoring must clear the minimized state.");
    Check(!window.TakeSizeChanged(), "Restoring to the same size is not a size change.");
}

void TestCloseRequestKeepsWindowUntilDestroyed()
{
    Input input;
    HWND handle = nullptr;
    {
        Window window(input, L"window test", 320, 240);
        handle = window.GetHandle();
        window.ProcessMessages();
        Check(!window.IsCloseRequested(), "A new window must not report a close request.");

        SendMessageW(handle, WM_CLOSE, 0, 0);
        window.ProcessMessages();
        Check(window.IsCloseRequested(), "A close request must be recorded so the loop can end.");
        window.ProcessMessages();
        Check(window.IsCloseRequested(), "A close request must stay recorded.");
        Check(IsWindow(handle) != FALSE, "The window must stay until the Window object is destroyed.");
    }
    Check(IsWindow(handle) == FALSE, "Destroying the Window object must destroy the window.");
}

void TestQueuedMessagesGoThroughProcessMessages()
{
    Input input;
    Window window(input, L"window test", 320, 240);

    // 실제 키 메시지는 메시지 큐에 쌓였다가 ProcessMessages가 꺼내 WindowProc에 넘긴다.
    PostMessageW(window.GetHandle(), WM_KEYDOWN, 'D', 0);
    input.ConsumeForTick();
    Check(!input.IsPressed(Action::MoveRight), "A queued message must wait until ProcessMessages runs.");

    window.ProcessMessages();
    Check(!window.IsCloseRequested(), "Processing key messages must not request a close.");
    input.ConsumeForTick();
    Check(input.IsPressed(Action::MoveRight) && input.IsHeld(Action::MoveRight),
          "ProcessMessages must deliver the queued key to Input.");
}
}

int RunWindowTests()
{
    try
    {
        TestKeyMessagesMapToActions();
        TestRepeatSignalIgnoredAfterFocusLoss();
        TestUnmappedKeysIgnored();
        TestMouseMoveBecomesDelta();
        TestMouseReferenceResetAfterFocusLossAndLeave();
        TestSizeAndMinimizeRecorded();
        TestCloseRequestKeepsWindowUntilDestroyed();
        TestQueuedMessagesGoThroughProcessMessages();
        std::cout << "Window message tests PASS (8 cases)\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "Window message tests FAIL: " << error.what() << '\n';
        return 1;
    }
}
