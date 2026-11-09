#pragma once
#include "RenderFrame.h"
#include <string>
#include <vector>

// 뼈 하나. 기본 자세(바인드 포즈)는 부모 뼈 기준의 이동·회전·크기다.
// 역바인드 행렬은 모델 공간의 정점을 이 뼈 기준 공간으로 옮기는 행렬이다. 뼈가 움직인 뒤의 위치와 곱하면 정점이 따라 움직인다.
// 모든 값은 로더가 엔진 좌표계로 바꿔 둔 것이다.
struct Bone
{
    std::string name;
    int parent = -1; // 부모 뼈의 번호. 뿌리면 -1. 부모는 항상 자식보다 앞 번호다
    DirectX::XMFLOAT3 translation{};
    DirectX::XMFLOAT4 rotation{0.0f, 0.0f, 0.0f, 1.0f}; // 사원수 (x, y, z, w). w=1이면 회전 없음
    DirectX::XMFLOAT3 scale{1.0f, 1.0f, 1.0f};
    DirectX::XMFLOAT4X4 inverseBind{};
};

// 채널이 바꾸는 값의 종류
enum class ChannelTarget
{
    Translation,
    Rotation,
    Scale
};

// 클립의 채널 하나: 뼈 하나의 이동/회전/크기를 시각마다 정한 키 목록.
// times는 오름차순이고, values는 같은 개수다. 이동·크기는 x, y, z(w는 0), 회전은 사원수 x, y, z, w다.
struct AnimationChannel
{
    int bone = 0;
    ChannelTarget target = ChannelTarget::Translation;
    std::vector<float> times;
    std::vector<DirectX::XMFLOAT4> values;
};

// 클립 하나. 길이는 채널들의 마지막 키 시각 가운데 가장 큰 값이다.
struct AnimationClip
{
    std::string name;
    float duration = 0.0f;
    std::vector<AnimationChannel> channels;
};

// 모델 파일 하나에서 읽은 CPU 원본. AssetManager가 소유하고, ModelRenderer와 Animator는 가리키기만 한다.
struct ModelData
{
    std::string path;                 // 요청한 상대 경로를 정리한 모양('/' 구분자). 공유의 열쇠이고 로그·오류 메시지에 쓴다
    MeshData mesh;                    // 엔진 좌표계로 바꾼 메시. 파일의 모든 삼각형 묶음을 하나로 합친다
    std::vector<Bone> bones;          // 스킨의 뼈들. 부모가 자식보다 앞에 온다. 스킨이 없으면 비어 있다
    std::vector<AnimationClip> clips; // 애니메이션 클립들

    // 이름으로 클립 번호를 찾는다. 없으면 -1.
    int FindClip(const std::string& name) const;
};

// glTF 2.0 파일(.gltf, .glb)을 읽어 ModelData로 바꾼다. 파일 형식을 아는 곳은 이 함수뿐이다.
// 좌표계 변환도 여기서 한다: glTF는 오른손 좌표계·반시계 방향 앞면이고, 엔진은 왼손 좌표계·시계 방향 앞면이다.
// 정점은 X를 뒤집고 삼각형의 정점 순서를 바꾼다. 뼈와 클립도 같은 거울을 맞춘다(이동의 x, 사원수의 y·z 부호, 역바인드 행렬 S·M·S).
// 실패하면 경로와 이유가 적힌 std::runtime_error를 던진다: 없는 파일, glTF가 아닌 파일, 지원하지 않는 데이터.
// fullPath는 실제로 열 파일 경로이고, displayPath는 메시지에 적을 경로다(요청한 상대 경로).
ModelData LoadGltf(const std::string& fullPath, const std::string& displayPath);
