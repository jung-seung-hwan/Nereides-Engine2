#pragma once
#include "Input.h"
#include <windows.h>

// Window는 Win32 창 하나를 만들고 OS 메시지를 처리한다.
// 키보드·마우스·포커스 메시지는 Input의 수집 함수 호출로 바꾼다.
// 창 크기·최소화·닫기 요청은 기록만 한다. 그것으로 무엇을 할지는 Application이 정한다.
// Renderer와 게임 행동은 모른다.
class Window
{
public:
    // 숨긴 창을 만든다. 보여 주는 것은 Show에서 한다.
    // width와 height는 그림이 그려지는 안쪽 영역의 크기다. 실패하면 예외를 던진다.
    Window(Input& input, const wchar_t* title, int width, int height);
    // 창을 없앤다. 이때 OS가 보내는 포커스 상실 메시지도 처리하므로 Input이 아직 살아 있어야 한다.
    ~Window();

    // OS가 이 객체의 주소를 기억한다. 복사하면 주소가 어긋난다.
    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;

    void Show();

    // 쌓인 메시지를 모두 처리한다. 닫기 요청을 받았으면 false를 돌려준다.
    bool ProcessMessages();

    HWND GetHandle() const { return m_handle; }
    int GetWidth() const { return m_width; }
    int GetHeight() const { return m_height; }
    bool IsMinimized() const { return m_minimized; }

    // 지난번 확인 뒤 크기가 바뀌었으면 true를 돌려주고 기록을 지운다.
    bool TakeSizeChanged();

private:
    // OS가 부르는 함수. 객체 없이 불리므로 static이다. 창에 붙여 둔 주소로 객체를 찾아 넘긴다.
    static LRESULT CALLBACK WindowProc(HWND handle, UINT message, WPARAM wParam, LPARAM lParam);
    LRESULT HandleMessage(HWND handle, UINT message, WPARAM wParam, LPARAM lParam);
    void OnMouseMove(HWND handle, int x, int y);

    Input& m_input;
    HWND m_handle = nullptr;
    int m_width;
    int m_height;
    bool m_minimized = false;
    bool m_sizeChanged = false;
    bool m_closeRequested = false;

    // 마우스 이동량을 구하는 기준점. 포커스를 잃거나 커서가 창 밖으로 나가면 지운다.
    bool m_hasMousePosition = false;
    bool m_trackingMouseLeave = false;
    int m_lastMouseX = 0;
    int m_lastMouseY = 0;
};
