#pragma once

// 아직 행렬 계산은 하지 않는다. 객체의 공간 정보를 한곳에 보관한다.
struct Vector3
{
    float x = 0;
    float y = 0;
    float z = 0;
};

struct Transform
{
    Vector3 position;
    Vector3 rotation; // 이후 회전 계산에서는 라디안을 사용한다.
    Vector3 scale{1, 1, 1};
};
