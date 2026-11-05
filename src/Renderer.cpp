#include "Renderer.h"
#include <iostream>
#include <iterator>
#include <sstream>
#include <stdexcept>
#include <utility>

// 빌드할 때 FxCompile이 .hlsl을 컴파일해서 중간 폴더에 만든 헤더다.
// 셰이더 바이트 배열 g_SceneVertexShader, g_ScenePixelShader가 들어 있다.
#include "ScenePixelShader.h"
#include "SceneVertexShader.h"

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
    // 기능 수준은 11_0만 받는다. 셰이더가 셰이더 모델 5.0을 쓰려면 11_0이 필요하다.
    // 11_0을 지원하지 않는 그래픽 하드웨어라면 WARP로 넘어간다. WARP는 11_0을 지원한다.
    const D3D_FEATURE_LEVEL featureLevel = D3D_FEATURE_LEVEL_11_0;
    return D3D11CreateDeviceAndSwapChain(nullptr, driverType, nullptr, flags, &featureLevel, 1, D3D11_SDK_VERSION,
                                         &desc, swapChain, device, nullptr, context);
}

// 셰이더의 상수 버퍼(SceneShader.hlsli)와 같은 순서·크기다. 상수 버퍼의 크기는 16바이트의 배수여야 한다.
// 행렬은 GPU에 올리기 전에 전치한다. DirectXMath는 행 벡터 규칙이고, HLSL 상수 버퍼는 기본이 열 우선 배치라서다.
struct FrameConstants
{
    DirectX::XMFLOAT4X4 viewProjection;
    DirectX::XMFLOAT3 lightDirection;
    float padding; // 16바이트를 맞추는 빈 자리
};
static_assert(sizeof(FrameConstants) % 16 == 0, "Constant buffer size must be a multiple of 16 bytes.");

struct ItemConstants
{
    DirectX::XMFLOAT4X4 world;
    DirectX::XMFLOAT4 color;
};
static_assert(sizeof(ItemConstants) % 16 == 0, "Constant buffer size must be a multiple of 16 bytes.");
}

Renderer::Renderer(HWND window, int width, int height) : m_width(width), m_height(height)
{
    if (width <= 0 || height <= 0)
        throw std::invalid_argument("Renderer needs a positive width and height.");

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
        // 실패한 앞 시도가 남긴 객체가 있으면 먼저 놓는다. GetAddressOf는 들고 있던 객체를 놓지 않고 자리만 넘긴다.
        ReleaseDeviceObjects();
        result = CreateDeviceAndSwapChain(driverType, flags, desc, m_swapChain.GetAddressOf(),
                                          m_device.GetAddressOf(), m_context.GetAddressOf());
        // 디버그 계층이 설치되지 않은 PC다. 이 경우에만 디버그 계층을 끄고 다시 만든다.
        if (result == DXGI_ERROR_SDK_COMPONENT_MISSING && flags != 0)
        {
            std::cout << "DX11 debug layer is not installed. Creating the device without it.\n";
            flags = 0;
            ReleaseDeviceObjects();
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
    // IID_PPV_ARGS는 받을 자리의 주소와 그 자리에 들어갈 COM 형식의 번호를 함께 넘겨 주는 매크로다.
    Microsoft::WRL::ComPtr<IDXGIFactory> factory;
    ThrowIfFailed(m_swapChain->GetParent(IID_PPV_ARGS(factory.GetAddressOf())), "IDXGISwapChain::GetParent");
    ThrowIfFailed(factory->MakeWindowAssociation(window, DXGI_MWA_NO_ALT_ENTER), "MakeWindowAssociation");

    CreateBackBufferView();
    CreateDepthBuffer();
    CreatePipeline();
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

void Renderer::CreateDepthBuffer()
{
    // 깊이 버퍼: 화면의 점마다 지금까지 그린 것 가운데 가장 가까운 깊이를 기억한다.
    // 더 먼 면은 그리지 않으므로, 앞의 객체가 뒤의 객체를 가린다. 크기는 화면 버퍼와 같아야 한다.
    D3D11_TEXTURE2D_DESC desc = {};
    desc.Width = static_cast<UINT>(m_width);
    desc.Height = static_cast<UINT>(m_height);
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT; // 깊이 24비트와 스텐실 8비트. 스텐실은 쓰지 않는다
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
    ThrowIfFailed(m_device->CreateTexture2D(&desc, nullptr, m_depthBuffer.GetAddressOf()), "CreateTexture2D (depth)");
    // 깊이 대상: 깊이 버퍼를 그리기 단계에 연결하는 창구다. 화면 대상과 버퍼의 관계와 같다.
    ThrowIfFailed(m_device->CreateDepthStencilView(m_depthBuffer.Get(), nullptr, m_depthView.GetAddressOf()),
                  "CreateDepthStencilView");
}

void Renderer::CreatePipeline()
{
    // 셰이더: 빌드할 때 컴파일한 바이트 배열로 GPU 셰이더 객체를 만든다.
    ThrowIfFailed(m_device->CreateVertexShader(g_SceneVertexShader, sizeof(g_SceneVertexShader), nullptr,
                                               m_vertexShader.GetAddressOf()),
                  "CreateVertexShader");
    ThrowIfFailed(m_device->CreatePixelShader(g_ScenePixelShader, sizeof(g_ScenePixelShader), nullptr,
                                              m_pixelShader.GetAddressOf()),
                  "CreatePixelShader");

    // 입력 배치: 정점 버퍼의 정점 하나가 위치(float 3개) 다음 법선(float 3개)이라는 것을 알린다.
    // POSITION과 NORMAL은 정점 셰이더 입력에 붙인 이름과 맞춘다. 두 번째 칸의 위치는 앞 칸 바로 뒤로 자동으로 잡는다.
    const D3D11_INPUT_ELEMENT_DESC layout[] = {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0},
    };
    ThrowIfFailed(m_device->CreateInputLayout(layout, static_cast<UINT>(std::size(layout)), g_SceneVertexShader,
                                              sizeof(g_SceneVertexShader), m_inputLayout.GetAddressOf()),
                  "CreateInputLayout");

    // 상수 버퍼: 셰이더가 읽는 값을 담는다. 내용은 그릴 때 UpdateSubresource로 채운다.
    D3D11_BUFFER_DESC desc = {};
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    desc.ByteWidth = sizeof(FrameConstants);
    ThrowIfFailed(m_device->CreateBuffer(&desc, nullptr, m_frameConstants.GetAddressOf()),
                  "CreateBuffer (frame constants)");
    desc.ByteWidth = sizeof(ItemConstants);
    ThrowIfFailed(m_device->CreateBuffer(&desc, nullptr, m_itemConstants.GetAddressOf()),
                  "CreateBuffer (item constants)");
}

void Renderer::ReleaseDeviceObjects()
{
    m_meshes.clear();
    m_itemConstants.Reset();
    m_frameConstants.Reset();
    m_inputLayout.Reset();
    m_pixelShader.Reset();
    m_vertexShader.Reset();
    m_depthView.Reset();
    m_depthBuffer.Reset();
    m_backBufferView.Reset();
    m_swapChain.Reset();
    m_context.Reset();
    m_device.Reset();
}

void Renderer::Resize(int width, int height)
{
    // 크기가 0인 버퍼는 만들 수 없다. 최소화된 창의 크기는 Window와 Application이 미리 거른다.
    if (width <= 0 || height <= 0)
        throw std::invalid_argument("Renderer::Resize needs a positive width and height.");

    // 화면 대상이 버퍼를 붙잡고 있으면 버퍼 크기를 바꿀 수 없다.
    // 그리기 단계에서 풀고, 놓고, 놓은 것을 장치가 바로 정리하게 한다. 깊이 버퍼도 새 크기로 다시 만든다.
    m_context->OMSetRenderTargets(0, nullptr, nullptr);
    m_backBufferView.Reset();
    m_depthView.Reset();
    m_depthBuffer.Reset();
    m_context->Flush();

    // 버퍼 수와 형식은 그대로 두고 크기만 바꾼다.
    ThrowIfFailed(m_swapChain->ResizeBuffers(0, static_cast<UINT>(width), static_cast<UINT>(height),
                                             DXGI_FORMAT_UNKNOWN, 0),
                  "IDXGISwapChain::ResizeBuffers");
    m_width = width;
    m_height = height;
    CreateBackBufferView();
    CreateDepthBuffer();
}

void Renderer::Render(const RenderFrame& frame)
{
    // 1. 화면 대상과 깊이 대상을 연결하고 뷰포트를 잡는다.
    // 플립 모델 스왑 체인은 Present가 끝나면 화면 대상 연결을 푼다. 그래서 그리기 전마다 다시 연결한다.
    // 뷰포트는 그림을 화면 대상의 어느 영역에 그릴지 정한다. 크기를 바꾼 뒤에도 맞도록 매번 지금 크기로 잡는다.
    ID3D11RenderTargetView* targets[] = {m_backBufferView.Get()};
    m_context->OMSetRenderTargets(1, targets, m_depthView.Get());
    D3D11_VIEWPORT viewport = {};
    viewport.Width = static_cast<float>(m_width);
    viewport.Height = static_cast<float>(m_height);
    viewport.MaxDepth = 1.0f;
    m_context->RSSetViewports(1, &viewport);

    // 2. 화면은 지우는 색으로, 깊이는 가장 먼 값인 1로 지운다.
    const float clearColor[4] = {frame.clearColor.x, frame.clearColor.y, frame.clearColor.z, frame.clearColor.w};
    m_context->ClearRenderTargetView(m_backBufferView.Get(), clearColor);
    m_context->ClearDepthStencilView(m_depthView.Get(), D3D11_CLEAR_DEPTH, 1.0f, 0);

    // 3. 셰이더, 입력 배치, 상수 버퍼를 설정하고 프레임 값을 올린다.
    // 래스터라이저와 깊이는 DX11 기본 상태를 쓴다. 시계 방향이 앞면이고 뒷면은 그리지 않으며, 가까운 것이 이긴다.
    m_context->IASetInputLayout(m_inputLayout.Get());
    m_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    m_context->VSSetShader(m_vertexShader.Get(), nullptr, 0);
    m_context->PSSetShader(m_pixelShader.Get(), nullptr, 0);
    ID3D11Buffer* constants[] = {m_frameConstants.Get(), m_itemConstants.Get()};
    m_context->VSSetConstantBuffers(0, static_cast<UINT>(std::size(constants)), constants);
    m_context->PSSetConstantBuffers(0, static_cast<UINT>(std::size(constants)), constants);

    // view × projection은 프레임마다 한 번만 곱해 둔다.
    FrameConstants frameConstants = {};
    const DirectX::XMMATRIX viewProjection =
        DirectX::XMLoadFloat4x4(&frame.view) * DirectX::XMLoadFloat4x4(&frame.projection);
    DirectX::XMStoreFloat4x4(&frameConstants.viewProjection, DirectX::XMMatrixTranspose(viewProjection));
    frameConstants.lightDirection = frame.lightDirection;
    m_context->UpdateSubresource(m_frameConstants.Get(), 0, nullptr, &frameConstants, 0, 0);

    // 4. 항목마다 GPU 메시를 찾고, 항목 값을 올리고, 그린다.
    for (const DrawItem& item : frame.items)
    {
        if (item.mesh == nullptr)
            throw std::invalid_argument("A draw item has no mesh.");
        const GpuMesh& mesh = FindOrCreateMesh(*item.mesh);

        ItemConstants itemConstants = {};
        DirectX::XMStoreFloat4x4(&itemConstants.world, DirectX::XMMatrixTranspose(DirectX::XMLoadFloat4x4(&item.world)));
        itemConstants.color = item.color;
        m_context->UpdateSubresource(m_itemConstants.Get(), 0, nullptr, &itemConstants, 0, 0);

        // 정점 하나의 바이트 수(stride)를 알려 줘야 GPU가 다음 정점을 찾는다.
        const UINT stride = sizeof(MeshVertex);
        const UINT offset = 0;
        ID3D11Buffer* vertexBuffers[] = {mesh.vertexBuffer.Get()};
        m_context->IASetVertexBuffers(0, 1, vertexBuffers, &stride, &offset);
        m_context->IASetIndexBuffer(mesh.indexBuffer.Get(), DXGI_FORMAT_R32_UINT, 0);
        m_context->DrawIndexed(mesh.indexCount, 0, 0);
    }

    // 5. 다 그린 화면을 창에 보낸다. 첫 인자 1은 화면 갱신 주기 하나를 기다린다는 뜻이다. 이것이 수직 동기다.
    // 이 기다림이 게임 루프의 속도를 정한다.
    ThrowIfFailed(m_swapChain->Present(1, 0), "IDXGISwapChain::Present");
}

const Renderer::GpuMesh& Renderer::FindOrCreateMesh(const MeshData& mesh)
{
    // 메시가 몇 개뿐이라 처음부터 차례로 찾는다.
    for (const GpuMesh& gpuMesh : m_meshes)
    {
        if (gpuMesh.source == &mesh)
            return gpuMesh;
    }

    // 처음 보는 원본이다. 정점과 인덱스를 GPU 버퍼로 복사한다.
    // 만든 버퍼는 Renderer가 정리될 때까지 둔다. 그래서 원본은 Renderer보다 오래 살아야 한다.
    if (mesh.vertices.empty() || mesh.indices.empty())
        throw std::invalid_argument("A mesh needs vertices and indices.");

    GpuMesh gpuMesh;
    gpuMesh.source = &mesh;
    gpuMesh.indexCount = static_cast<UINT>(mesh.indices.size());

    // 바뀌지 않는 데이터라 IMMUTABLE로 만든다. 만들 때 내용을 함께 넘겨야 한다.
    D3D11_BUFFER_DESC desc = {};
    desc.Usage = D3D11_USAGE_IMMUTABLE;
    desc.ByteWidth = static_cast<UINT>(mesh.vertices.size() * sizeof(MeshVertex));
    desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    D3D11_SUBRESOURCE_DATA data = {};
    data.pSysMem = mesh.vertices.data();
    ThrowIfFailed(m_device->CreateBuffer(&desc, &data, gpuMesh.vertexBuffer.GetAddressOf()),
                  "CreateBuffer (vertices)");

    desc.ByteWidth = static_cast<UINT>(mesh.indices.size() * sizeof(std::uint32_t));
    desc.BindFlags = D3D11_BIND_INDEX_BUFFER;
    data.pSysMem = mesh.indices.data();
    ThrowIfFailed(m_device->CreateBuffer(&desc, &data, gpuMesh.indexBuffer.GetAddressOf()),
                  "CreateBuffer (indices)");

    m_meshes.push_back(std::move(gpuMesh));
    return m_meshes.back();
}
