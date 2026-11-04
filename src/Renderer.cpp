#include "Renderer.h"
#include <iostream>
#include <sstream>
#include <stdexcept>

namespace
{
// DX11 함수는 성공과 실패를 HRESULT로 돌려준다.
// 실패하면 무엇이 실패했는지와 HRESULT 값을 담아 예외를 던진다.
void ThrowIfFailed(HRESULT result, const char* action)
{
    if (SUCCEEDED(result))
        return;
    std::ostringstream message;
    message << action << " failed. HRESULT 0x" << std::hex << std::uppercase
            << static_cast<unsigned long>(result);
    throw std::runtime_error(message.str());
}

HRESULT CreateDeviceAndSwapChain(D3D_DRIVER_TYPE driverType, UINT flags, const DXGI_SWAP_CHAIN_DESC& desc,
                                 IDXGISwapChain** swapChain, ID3D11Device** device, ID3D11DeviceContext** context)
{
    const D3D_FEATURE_LEVEL featureLevel = D3D_FEATURE_LEVEL_11_0;
    return D3D11CreateDeviceAndSwapChain(nullptr, driverType, nullptr, flags, &featureLevel, 1, D3D11_SDK_VERSION,
                                         &desc, swapChain, device, nullptr, context);
}
}

Renderer::Renderer(HWND window, int width, int height)
{
    DXGI_SWAP_CHAIN_DESC desc = {};
    desc.BufferDesc.Width = static_cast<UINT>(width);
    desc.BufferDesc.Height = static_cast<UINT>(height);
    desc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    desc.BufferCount = 2; // 버퍼 두 장을 번갈아 쓴다. 하나에 그리는 동안 다른 하나가 화면에 보인다.
    desc.OutputWindow = window;
    desc.Windowed = TRUE;
    desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;

    UINT flags = 0;
#ifdef _DEBUG
    // Debug 빌드는 잘못된 DX11 사용을 알려 주는 디버그 계층을 켠다.
    flags = D3D11_CREATE_DEVICE_DEBUG;
#endif

    // 그래픽 하드웨어로 먼저 만들고, 실패하면 CPU로 DX11을 흉내 내는 WARP로 만든다.
    const D3D_DRIVER_TYPE driverTypes[] = {D3D_DRIVER_TYPE_HARDWARE, D3D_DRIVER_TYPE_WARP};
    HRESULT result = E_FAIL;
    for (D3D_DRIVER_TYPE driverType : driverTypes)
    {
        result = CreateDeviceAndSwapChain(driverType, flags, desc, m_swapChain.GetAddressOf(),
                                          m_device.GetAddressOf(), m_context.GetAddressOf());
        // 디버그 계층이 설치되지 않은 PC다. 이 경우에만 디버그 계층을 끄고 다시 만든다.
        if (result == DXGI_ERROR_SDK_COMPONENT_MISSING && flags != 0)
        {
            std::cout << "DX11 debug layer is not installed. Creating the device without it.\n";
            flags = 0;
            result = CreateDeviceAndSwapChain(driverType, flags, desc, m_swapChain.GetAddressOf(),
                                              m_device.GetAddressOf(), m_context.GetAddressOf());
        }
        if (SUCCEEDED(result))
        {
            m_usingWarp = driverType == D3D_DRIVER_TYPE_WARP;
            break;
        }
    }
    ThrowIfFailed(result, "D3D11CreateDeviceAndSwapChain");

    if (m_usingWarp)
        std::cout << "DX11 device: WARP, the CPU version\n";
    else
        std::cout << "DX11 device: graphics hardware\n";

    // Alt+Enter 전체 화면 전환을 끈다. 전체 화면은 이번 범위가 아니다.
    Microsoft::WRL::ComPtr<IDXGIFactory> factory;
    ThrowIfFailed(m_swapChain->GetParent(IID_PPV_ARGS(factory.GetAddressOf())), "IDXGISwapChain::GetParent");
    ThrowIfFailed(factory->MakeWindowAssociation(window, DXGI_MWA_NO_ALT_ENTER), "MakeWindowAssociation");

    CreateBackBufferView();
}

void Renderer::CreateBackBufferView()
{
    // 스왑 체인이 소유한 버퍼를 잠깐 빌려 와서, 그 버퍼에 그리는 창구인 화면 대상을 만든다.
    // 빌려 온 backBuffer는 이 함수가 끝나면 놓는다. 화면 대상은 버퍼를 계속 가리킨다.
    Microsoft::WRL::ComPtr<ID3D11Texture2D> backBuffer;
    ThrowIfFailed(m_swapChain->GetBuffer(0, IID_PPV_ARGS(backBuffer.GetAddressOf())), "IDXGISwapChain::GetBuffer");
    ThrowIfFailed(m_device->CreateRenderTargetView(backBuffer.Get(), nullptr, m_backBufferView.GetAddressOf()),
                  "CreateRenderTargetView");
}

void Renderer::Resize(int width, int height)
{
    // 화면 대상이 버퍼를 붙잡고 있으면 버퍼 크기를 바꿀 수 없다.
    // 그리기 단계에서 풀고, 놓고, 놓은 것을 장치가 바로 정리하게 한다.
    m_context->OMSetRenderTargets(0, nullptr, nullptr);
    m_backBufferView.Reset();
    m_context->Flush();

    // 버퍼 수와 형식은 그대로 두고 크기만 바꾼다.
    ThrowIfFailed(m_swapChain->ResizeBuffers(0, static_cast<UINT>(width), static_cast<UINT>(height),
                                             DXGI_FORMAT_UNKNOWN, 0),
                  "IDXGISwapChain::ResizeBuffers");
    CreateBackBufferView();
}

void Renderer::Clear(float red, float green, float blue)
{
    const float color[4] = {red, green, blue, 1.0f};
    m_context->ClearRenderTargetView(m_backBufferView.Get(), color);
}

void Renderer::Present()
{
    // 첫 인자 1은 화면 갱신 주기 하나를 기다린다는 뜻이다. 이것이 수직 동기다.
    // 이 기다림이 게임 루프의 속도를 정한다.
    ThrowIfFailed(m_swapChain->Present(1, 0), "IDXGISwapChain::Present");
}
