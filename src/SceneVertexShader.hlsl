#include "SceneShader.hlsli"

// 정점 버퍼의 정점 하나. 이름 POSITION, NORMAL, BLENDINDICES, BLENDWEIGHT는 Renderer.cpp의 입력 배치와 맞춘다.
struct VertexInput
{
    float3 position : POSITION;
    float3 normal : NORMAL;
    uint4 joints : BLENDINDICES; // 이 정점이 따르는 뼈 번호 4개
    float4 weights : BLENDWEIGHT; // 각 뼈의 비중. 합이 0이면 뼈를 따르지 않는 정점
};

// 정점 하나를 화면 위치로 바꾸고, 법선을 월드 방향으로 돌린다.
VertexOutput main(VertexInput input)
{
    VertexOutput output;
    // 위치는 w를 1로 두어 이동까지 적용한다. 법선은 방향이라 w를 0으로 두어 이동을 빼고 회전·크기만 적용한다.
    float4 localPosition = float4(input.position, 1.0f);
    float4 localNormal = float4(input.normal, 0.0f);

    // 스키닝: 뼈 4개의 행렬을 가중치만큼 섞어 하나의 행렬로 만들고, 정점을 기본 자세에서 지금 자세로 옮긴다.
    // 가중치 합으로 나눠서 합이 정확히 1이 아닌 파일(도구의 반올림)에서도 정점이 줄거나 늘지 않게 한다.
    // 가중치가 모두 0인 정점은 그대로 둔다.
    const float weightSum = input.weights.x + input.weights.y + input.weights.z + input.weights.w;
    if (weightSum > 0.0f)
    {
        const float4x4 skin = (input.weights.x * bones[input.joints.x] + input.weights.y * bones[input.joints.y] +
                               input.weights.z * bones[input.joints.z] + input.weights.w * bones[input.joints.w]) /
                              weightSum;
        localPosition = mul(localPosition, skin);
        localNormal = mul(localNormal, skin);
    }

    const float4 worldPosition = mul(localPosition, world);
    output.position = mul(worldPosition, viewProjection);
    // 축마다 크기가 다르면 기울어진 면의 법선이 틀어진다. 지금은 상자를 같은 비율로만 키우고, 바닥은 법선이 +Y라 괜찮다.
    // 뼈에 비균등 크기가 있을 때도 같은 한계가 있다.
    output.normal = mul(localNormal, world).xyz;
    return output;
}
