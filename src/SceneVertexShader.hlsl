#include "SceneShader.hlsli"

// 정점 버퍼의 정점 하나. 이름 POSITION, NORMAL은 Renderer.cpp의 입력 배치와 맞춘다.
struct VertexInput
{
    float3 position : POSITION;
    float3 normal : NORMAL;
};

// 정점 하나를 화면 위치로 바꾸고, 법선을 월드 방향으로 돌린다.
VertexOutput main(VertexInput input)
{
    VertexOutput output;
    // 위치는 w를 1로 두어 이동까지 적용한다.
    const float4 worldPosition = mul(float4(input.position, 1.0f), world);
    output.position = mul(worldPosition, viewProjection);
    // 법선은 방향이라 w를 0으로 두어 이동을 빼고 회전·크기만 적용한다.
    // 축마다 크기가 다르면 기울어진 면의 법선이 틀어진다. 지금은 상자를 같은 비율로만 키우고, 바닥은 법선이 +Y라 괜찮다.
    output.normal = mul(float4(input.normal, 0.0f), world).xyz;
    return output;
}
