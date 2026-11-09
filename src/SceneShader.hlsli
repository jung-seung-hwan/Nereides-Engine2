// 정점 셰이더와 픽셀 셰이더가 함께 쓰는 선언이다.
// 상수 버퍼는 Renderer.cpp의 FrameConstants, ItemConstants, BoneConstants와 같은 순서·크기여야 한다.

// 프레임마다 한 번 바뀌는 값
cbuffer FrameConstants : register(b0)
{
    float4x4 viewProjection; // view × projection. C++에서 전치해서 올린다
    float3 lightDirection;   // 빛이 나아가는 방향
    float framePadding;      // 16바이트를 맞추는 빈 자리
};

// 그릴 항목마다 바뀌는 값
cbuffer ItemConstants : register(b1)
{
    float4x4 world; // C++에서 전치해서 올린다
    float4 color;
};

// 그릴 항목마다 바뀌는 뼈 행렬. 개수는 RenderFrame.h의 MaxBones와 같다. 쓰지 않는 자리는 단위 행렬이다.
static const int MaxBones = 128;
cbuffer BoneConstants : register(b2)
{
    float4x4 bones[MaxBones]; // C++에서 전치해서 올린다
};

// 정점 셰이더가 내보내고 픽셀 셰이더가 받는 값
struct VertexOutput
{
    float4 position : SV_POSITION; // 화면 위치
    float3 normal : NORMAL;        // 월드 방향의 법선
};
