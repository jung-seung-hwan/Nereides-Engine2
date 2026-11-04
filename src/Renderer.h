#pragma once
#include <d3d11.h>
#include <windows.h>
#include <wrl/client.h>

// Renderer는 DX11 장치와 화면에 보낼 버퍼를 소유한다.
// 단계 3-1에서는 화면을 한 가지 색으로 지우고 창에 보내는 일만 한다. 도형과 카메라는 단계 3-2에서 더한다.
// 창은 소유하지 않는다. 스왑 체인이 그림을 보내는 창은 Renderer보다 오래 살아야 한다.
class Renderer
{
public:
    // 창 핸들과 안쪽 크기로 장치와 스왑 체인을 만든다. 실패하면 예외를 던진다.
    Renderer(HWND window, int width, int height);

    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;

    // 창 크기가 바뀐 뒤 Application이 부른다. 크기는 0보다 커야 한다.
    void Resize(int width, int height);
    // 화면 대상을 한 가지 색으로 지운다. 색은 0부터 1 사이의 빨강·초록·파랑이다.
    void Clear(float red, float green, float blue);
    // 다 그린 화면을 창에 보낸다. 화면 갱신 주기 하나를 기다린다.
    void Present();

    bool IsUsingWarp() const { return m_usingWarp; }

private:
    void CreateBackBufferView();

    // ComPtr은 DX11 객체의 참조 수를 대신 관리한다. 멤버가 정리될 때 자동으로 놓아 준다.
    // 선언 순서대로 만들고 반대 순서로 정리한다. 화면 대상이 가장 먼저, 장치가 가장 나중에 정리된다.
    Microsoft::WRL::ComPtr<ID3D11Device> m_device;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> m_context;
    Microsoft::WRL::ComPtr<IDXGISwapChain> m_swapChain;
    Microsoft::WRL::ComPtr<ID3D11RenderTargetView> m_backBufferView;
    bool m_usingWarp = false;
};
