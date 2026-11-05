#pragma once
#include <DirectXMath.h>

// 객체의 공간 정보를 한곳에 보관한다.
struct Vector3
{
    float x = 0;
    float y = 0;
    float z = 0;
};

struct Transform
{
    Vector3 position;
    Vector3 rotation; // 라디안. x는 피치(위아래), y는 요(좌우), z는 롤(기울기)이다.
    Vector3 scale{1, 1, 1};
};

// from과 to 사이를 t만큼 섞는다. t가 0이면 from, 1이면 to다. 위치·회전·크기를 모두 섞는다.
// 회전은 감싸지 않은 라디안을 그대로 섞는다. 각도를 -π~π로 감싸게 되면 짧은 쪽으로 섞도록 바꾼다.
Transform Lerp(const Transform& from, const Transform& to, float t);

// 월드 행렬 = 크기 × 회전 × 이동. 이 계산은 여기 한 곳에서만 한다.
// XMMATRIX는 계산용 형식이다. 오래 보관할 때는 XMFLOAT4X4에 담는다.
DirectX::XMMATRIX WorldMatrix(const Transform& transform);
