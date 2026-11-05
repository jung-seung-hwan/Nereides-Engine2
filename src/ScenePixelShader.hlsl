#include "SceneShader.hlsli"

// 빛을 등진 면도 완전히 검게 되지 않게 하는 기본 밝기
static const float Ambient = 0.3f;

// 화면의 점 하나의 색을 정한다. 빛이 오는 쪽을 정면으로 볼수록 밝다.
float4 main(VertexOutput input) : SV_TARGET
{
    const float3 normal = normalize(input.normal);
    const float3 toLight = -normalize(lightDirection);
    // dot은 두 방향이 같으면 1, 수직이면 0, 반대면 -1이다. saturate로 0보다 작은 값을 0으로 자른다.
    const float facing = saturate(dot(normal, toLight));
    const float brightness = Ambient + (1.0f - Ambient) * facing;
    return float4(color.rgb * brightness, color.a);
}
