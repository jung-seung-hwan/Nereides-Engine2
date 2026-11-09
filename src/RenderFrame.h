#pragma once
#include <DirectXMath.h>
#include <cstdint>
#include <vector>

// Renderer에 넘기는 값의 모양이다. 이 헤더는 Scene을 모른다.
// 그래서 이 헤더만 포함하는 Renderer는 Scene을 읽을 방법이 없다.
// 이름과 모양은 상위 Nereides 프로젝트가 셜록 렌더러를 붙인 경계를 따른다. 셜록을 붙일 때 이 형식을 셜록 쪽에 맞춘다.

// 그리기 항목 하나가 가질 수 있는 뼈 행렬의 최대 개수. 셰이더의 상수 버퍼 크기와 같다(SceneShader.hlsli).
const int MaxBones = 128;

// 정점 하나. 위치, 법선, 그리고 단계 4-2부터 뼈 번호와 가중치를 둔다. 법선은 면이 바라보는 방향을 뜻하는 길이 1짜리 벡터다.
// 뼈 번호는 이 정점이 따르는 뼈(최대 4개)이고, 가중치는 각 뼈를 얼마나 따르는지다(합이 1).
// 가중치가 모두 0이면 뼈를 따르지 않는 정점이다. 코드로 만든 바닥·상자와 스킨 없는 메시의 정점이 그렇다.
struct MeshVertex
{
    DirectX::XMFLOAT3 position{};
    DirectX::XMFLOAT3 normal{0.0f, 1.0f, 0.0f};
    std::uint8_t joints[4] = {};
    float weights[4] = {};
};

// CPU에 있는 메시 원본. 정점 배열과, 정점을 세 개씩 묶어 삼각형을 만드는 순서(인덱스)다.
// 바깥에서 볼 때 시계 방향이 앞면이다.
struct MeshData
{
    std::vector<MeshVertex> vertices;
    std::vector<std::uint32_t> indices;
};

// 그릴 항목 하나: 어떤 메시를, 어디에, 무슨 색으로, 어떤 뼈 자세로 그릴지.
// mesh는 소유하지 않고 가리키기만 한다. 원본은 Application이 프로그램이 끝날 때까지 갖고 있다.
// bones는 뼈마다 하나씩, 정점을 기본 자세에서 지금 자세로 옮기는 행렬이다(Animator가 tick 안에서 계산한 값의 복사본).
// 비어 있으면 Renderer가 단위 행렬을 써서 기본 자세로 그린다.
struct DrawItem
{
    const MeshData* mesh = nullptr;
    DirectX::XMFLOAT4X4 world{};
    DirectX::XMFLOAT4 color{1.0f, 1.0f, 1.0f, 1.0f};
    std::vector<DirectX::XMFLOAT4X4> bones;
};

// 프레임 하나를 그리는 데 필요한 값 전부. 수집 함수가 만들고 Renderer가 읽는다.
struct RenderFrame
{
    DirectX::XMFLOAT4X4 view{};
    DirectX::XMFLOAT4X4 projection{};
    std::vector<DrawItem> items;
    // 빛이 나아가는 방향. 위쪽, 카메라 쪽에서 비스듬히 내려온다. 셰이더가 길이를 1로 맞춰 쓴다.
    DirectX::XMFLOAT3 lightDirection{0.3f, -0.8f, 0.52f};
    // 화면을 지우는 색. 단계 3-1의 어두운 파란색이다.
    DirectX::XMFLOAT4 clearColor{0.08f, 0.12f, 0.20f, 1.0f};
};

// 한 변이 1인 상자. 중심이 원점이다. 면마다 법선이 달라 면마다 정점 4개를 둔다(정점 24개, 인덱스 36개).
MeshData MakeBox();
// 한 변이 1인 평면. y=0에 놓이고 위(+Y)를 본다. 넓이는 Transform의 크기로 키운다.
MeshData MakeFloor();
