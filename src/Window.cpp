#include "Window.h"
#include <stdexcept>
#include <string>
#include <windowsx.h>

namespace
{
const wchar_t* const ClassName = L"NereidesEngine2Window";

// Win32 함수가 실패하면 OS가 남긴 오류 번호를 함께 담아 예외를 던진다.
// 오류 번호는 다른 일을 하기 전에 실패 직후 읽어서 넘긴다. 문자열을 만드는 동안 값이 바뀔 수 있기 때문이다.
void ThrowWin32Error(const char* action, DWORD error)
{
    throw std::runtime_error(std::string(action) + " failed. GetLastError " + std::to_string(error));
}

struct KeyBinding
{
    WPARAM virtualKey;
    Key key;
};

// OS 키 코드와 엔진 Key의 연결 표. 표에 없는 키는 무시한다.
const KeyBinding KeyTable[] = {
    {'W', Key::W},
    {'A', Key::A},
    {'S', Key::S},
    {'D', Key::D},
    {VK_SPACE, Key::Space},
};

bool ToKey(WPARAM virtualKey, Key& key)
{
    for (const KeyBinding& binding : KeyTable)
    {
        if (binding.virtualKey == virtualKey)
        {
            key = binding.key;
            return true;
        }
    }
    return false;
}

// WM_KEYDOWN의 lParam 30번 비트는 이 메시지 전에 키가 이미 눌려 있었으면 1이다.
// 키를 오래 누를 때 OS가 반복해서 보내는 신호다.
// 0x40000000은 2의 30제곱, 곧 30번 비트만 1인 값이다. &로 그 비트만 남겨 0인지 본다.
bool IsRepeatSignal(LPARAM lParam)
{
    return (lParam & 0x40000000) != 0;
}
}

Window::Window(Input& input, const wchar_t* title, int width, int height)
    : m_input(input), m_width(width), m_height(height)
{
    const HINSTANCE instance = GetModuleHandleW(nullptr);

    WNDCLASSEXW windowClass = {};
    windowClass.cbSize = sizeof(windowClass);
    windowClass.lpfnWndProc = &Window::WindowProc;
    windowClass.hInstance = instance;
    windowClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    windowClass.lpszClassName = ClassName;
    // 시험처럼 창을 여러 번 만들면 두 번째부터는 이미 등록돼 있다. 그것은 실패가 아니다.
    if (RegisterClassExW(&windowClass) == 0)
    {
        const DWORD error = GetLastError();
        if (error != ERROR_CLASS_ALREADY_EXISTS)
            ThrowWin32Error("RegisterClassExW", error);
    }

    // 제목 줄과 테두리를 더해, 안쪽 영역이 width x height가 되는 창 크기를 구한다.
    const DWORD style = WS_OVERLAPPEDWINDOW;
    RECT bounds = {0, 0, width, height};
    AdjustWindowRect(&bounds, style, FALSE);

    // 마지막 인자로 이 객체의 주소를 넘긴다. OS가 WM_NCCREATE에 실어 WindowProc로 돌려준다.
    m_handle = CreateWindowExW(0, ClassName, title, style, CW_USEDEFAULT, CW_USEDEFAULT,
                               bounds.right - bounds.left, bounds.bottom - bounds.top,
                               nullptr, nullptr, instance, this);
    if (m_handle == nullptr)
        ThrowWin32Error("CreateWindowExW", GetLastError());
}

Window::~Window()
{
    // 창을 없애는 동안 OS가 WM_KILLFOCUS 같은 메시지를 보낸다. 그 처리는 아직 살아 있는 이 객체가 한다.
    if (m_handle != nullptr)
        DestroyWindow(m_handle);
    UnregisterClassW(ClassName, GetModuleHandleW(nullptr));
}

void Window::Show()
{
    ShowWindow(m_handle, SW_SHOW);
}

bool Window::ProcessMessages()
{
    MSG message = {};
    // PeekMessageW는 메시지가 없으면 기다리지 않고 바로 돌아온다. 그래서 메시지가 없어도 게임 루프가 계속 돈다.
    while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE))
    {
        if (message.message == WM_QUIT)
            m_closeRequested = true;
        // 키 메시지에서 문자 메시지를 만드는 표준 단계다. Alt+Space 같은 시스템 메뉴도 이것으로 동작한다.
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
    return !m_closeRequested;
}

bool Window::TakeSizeChanged()
{
    const bool changed = m_sizeChanged;
    m_sizeChanged = false;
    return changed;
}

LRESULT CALLBACK Window::WindowProc(HWND handle, UINT message, WPARAM wParam, LPARAM lParam)
{
    if (message == WM_NCCREATE)
    {
        // CreateWindowExW에 넘긴 Window 주소를 꺼내 창에 붙여 둔다.
        const CREATESTRUCTW* create = reinterpret_cast<const CREATESTRUCTW*>(lParam);
        SetWindowLongPtrW(handle, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(create->lpCreateParams));
    }

    Window* window = reinterpret_cast<Window*>(GetWindowLongPtrW(handle, GWLP_USERDATA));
    // WM_NCCREATE보다 먼저 오는 메시지에는 아직 주소가 없다. OS의 기본 처리에 맡긴다.
    if (window == nullptr)
        return DefWindowProcW(handle, message, wParam, lParam);
    return window->HandleMessage(handle, message, wParam, lParam);
}

LRESULT Window::HandleMessage(HWND handle, UINT message, WPARAM wParam, LPARAM lParam)
{
    // 이 함수 안에서는 예외를 던지지 않는다. 예외가 OS 코드를 거쳐 빠져나가면 안전하지 않다.
    // Input 함수는 잘못된 Key를 받을 때만 예외를 던진다. 여기서는 KeyTable에 있는 Key만 넘기므로 던지지 않는다.
    Key key = Key::W;
    switch (message)
    {
    case WM_KEYDOWN:
        if (!IsRepeatSignal(lParam) && ToKey(wParam, key))
            m_input.OnKeyDown(key);
        return 0;

    case WM_KEYUP:
        if (ToKey(wParam, key))
            m_input.OnKeyUp(key);
        return 0;

    case WM_MOUSEMOVE:
        OnMouseMove(handle, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
        return 0;

    case WM_MOUSELEAVE:
        // 커서가 창 밖으로 나갔다. 돌아온 뒤 첫 이동은 기준점만 잡는다.
        m_trackingMouseLeave = false;
        m_hasMousePosition = false;
        return 0;

    case WM_KILLFOCUS:
        // 창 밖에서 뗀 키는 알 수 없으므로 눌린 상태를 모두 비운다.
        m_input.OnFocusLost();
        m_hasMousePosition = false;
        return 0;

    case WM_SIZE:
        if (wParam == SIZE_MINIMIZED)
        {
            // 최소화되면 크기가 0으로 온다. 0은 기록하지 않는다.
            m_minimized = true;
        }
        else
        {
            m_minimized = false;
            const int width = LOWORD(lParam);
            const int height = HIWORD(lParam);
            if (width > 0 && height > 0 && (width != m_width || height != m_height))
            {
                m_width = width;
                m_height = height;
                m_sizeChanged = true;
            }
        }
        return 0;

    case WM_CLOSE:
        // 닫기 요청만 기록한다. 창은 Renderer가 정리된 뒤 Window 소멸자에서 없앤다.
        m_closeRequested = true;
        return 0;

    case WM_NCDESTROY:
        // 창이 완전히 사라진다. 붙여 둔 주소를 지워서 이후 메시지가 이 객체로 오지 않게 한다.
        SetWindowLongPtrW(handle, GWLP_USERDATA, 0);
        m_handle = nullptr;
        m_closeRequested = true;
        return DefWindowProcW(handle, message, wParam, lParam);

    default:
        return DefWindowProcW(handle, message, wParam, lParam);
    }
}

void Window::OnMouseMove(HWND handle, int x, int y)
{
    // 커서가 창 밖으로 나가면 WM_MOUSELEAVE가 오도록 한 번 요청해 둔다.
    if (!m_trackingMouseLeave)
    {
        TRACKMOUSEEVENT track = {};
        track.cbSize = sizeof(track);
        track.dwFlags = TME_LEAVE;
        track.hwndTrack = handle;
        m_trackingMouseLeave = TrackMouseEvent(&track) != FALSE;
    }

    // Input은 이동량을 받는다. 기준점이 없으면 이번 좌표를 기준점으로만 잡는다.
    if (m_hasMousePosition)
        m_input.OnMouseMove(x - m_lastMouseX, y - m_lastMouseY);
    m_lastMouseX = x;
    m_lastMouseY = y;
    m_hasMousePosition = true;
}
