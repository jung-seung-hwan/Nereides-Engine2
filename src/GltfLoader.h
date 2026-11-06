#pragma once
#include "RenderFrame.h"
#include <string>
#include <vector>

// 모델 파일 하나에서 읽은 CPU 원본. AssetManager가 소유하고, ModelRenderer는 안의 mesh를 가리키기만 한다.
struct ModelData
{
    std::string path;                   // 요청한 상대 경로를 정리한 모양('/' 구분자). 공유의 열쇠이고 로그·오류 메시지에 쓴다
    MeshData mesh;                      // 엔진 좌표계로 바꾼 메시. 파일의 모든 삼각형 묶음을 하나로 합친다
    std::size_t boneCount = 0;          // 스킨의 뼈 개수. 스킨이 없으면 0. 4-2가 쓴다
    std::vector<std::string> clipNames; // 애니메이션 클립 이름들. 4-2가 쓴다
};

// glTF 2.0 파일(.gltf, .glb)을 읽어 ModelData로 바꾼다. 파일 형식을 아는 곳은 이 함수뿐이다.
// 좌표계 변환도 여기서 한다: glTF는 오른손 좌표계·반시계 방향 앞면이고, 엔진은 왼손 좌표계·시계 방향 앞면이다.
// X를 뒤집고 삼각형의 정점 순서를 바꾼다. 그래서 glTF의 앞(+Z)이 엔진에서도 앞(+Z)이다.
// 실패하면 경로와 이유가 적힌 std::runtime_error를 던진다: 없는 파일, glTF가 아닌 파일, 지원하지 않는 데이터.
// fullPath는 실제로 열 파일 경로이고, displayPath는 메시지에 적을 경로다(요청한 상대 경로).
ModelData LoadGltf(const std::string& fullPath, const std::string& displayPath);
