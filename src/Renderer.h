#pragma once
#include "RenderFrame.h"
#include <d3d11.h>
#include <windows.h>
#include <wrl/client.h>
#include <vector>

// Renderer는 DX11 장치와, 그리는 데 필요한 GPU 자원을 소유한다.
// 받는 것은 RenderFrame뿐이다. RenderFrame.h만 포함하므로 Scene을 읽을 수 없다.
// 이 클래스의 안쪽(셰이더, 깊이 버퍼, GPU 버퍼, 그리기)은 나중에 셜록 렌더러로 바뀐다.
// 창은 소유하지 않는다. 스왑 체인이 그림을 보내는 창은 Renderer보다 오래 살아야 한다.
class Renderer
{
public:
    // 창 핸들과 안쪽 크기로 장치, 스왑 체인, 깊이 버퍼, 셰이더를 만든다. 실패하면 예외를 던진다.
    Renderer(HWND window, int width, int height);

    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;

    // 창 크기가 바뀐 뒤 Application이 부른다. 크기는 0보다 커야 한다.
    void Resize(int width, int height);
    // 한 프레임을 그린다: 화면을 지우고, 항목을 모두 그리고, 창에 보낸다. 화면 갱신 주기 하나를 기다린다.
    void Render(const RenderFrame& frame);

    bool IsUsingWarp() const { return m_usingWarp; }

private:
    // CPU 메시 원본 하나를 GPU에 복사한 것.
    struct GpuMesh
    {
        const MeshData* source = nullptr; // 원본 주소. 같은 원본인지 알아보는 데만 쓴다
        Microsoft::WRL::ComPtr<ID3D11Buffer> vertexBuffer;
        Microsoft::WRL::ComPtr<ID3D11Buffer> indexBuffer;
        UINT indexCount = 0;
    };

    void CreateBackBufferView();
    void CreateDepthBuffer();
    // 셰이더, 입력 배치, 상수 버퍼를 만든다.
    void CreatePipeline();
    // 장치를 다시 만들기 전에 들고 있던 DX11 객체를 모두 놓는다. 선언의 반대 순서로 놓는다.
    void ReleaseDeviceObjects();
    // 이 원본의 GPU 메시를 찾는다. 처음 보는 원본이면 GPU 버퍼를 만들어 목록에 넣는다.
    // 돌려준 참조는 다음 메시를 만들기 전까지만 쓴다. 목록이 커지면 자리가 옮겨질 수 있다.
    const GpuMesh& FindOrCreateMesh(const MeshData& mesh);

    // ComPtr은 DX11 객체의 참조 수를 대신 관리한다. 멤버가 정리될 때 자동으로 놓아 준다.
    // 선언 순서대로 만들고 반대 순서로 정리한다. GPU 메시가 가장 먼저, 장치가 가장 나중에 정리된다.
    Microsoft::WRL::ComPtr<ID3D11Device> m_device;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> m_context;
    Microsoft::WRL::ComPtr<IDXGISwapChain> m_swapChain;
    Microsoft::WRL::ComPtr<ID3D11RenderTargetView> m_backBufferView;
    Microsoft::WRL::ComPtr<ID3D11Texture2D> m_depthBuffer;
    Microsoft::WRL::ComPtr<ID3D11DepthStencilView> m_depthView;
    Microsoft::WRL::ComPtr<ID3D11VertexShader> m_vertexShader;
    Microsoft::WRL::ComPtr<ID3D11PixelShader> m_pixelShader;
    Microsoft::WRL::ComPtr<ID3D11InputLayout> m_inputLayout;
    Microsoft::WRL::ComPtr<ID3D11Buffer> m_frameConstants; // 프레임마다 바뀌는 값
    Microsoft::WRL::ComPtr<ID3D11Buffer> m_itemConstants;  // 항목마다 바뀌는 값
    Microsoft::WRL::ComPtr<ID3D11Buffer> m_boneConstants;  // 항목마다 바뀌는 뼈 행렬(최대 MaxBones개)
    std::vector<GpuMesh> m_meshes;
    int m_width;
    int m_height;
    bool m_usingWarp = false;
};
