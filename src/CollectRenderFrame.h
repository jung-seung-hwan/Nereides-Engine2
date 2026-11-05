#pragma once
#include "RenderFrame.h"

class CameraComponent;
class Scene;

// Scene을 읽어 이번 프레임에 그릴 RenderFrame을 만든다. Scene은 바꾸지 않는다.
// Scene을 읽어 RenderFrame을 만드는 곳은 여기 하나다. 셜록을 붙일 때 셜록 쪽 형식으로 옮기는 곳도 여기다.
// Application이 모든 tick이 끝난 뒤 프레임마다 한 번 부른다. 창이나 GPU 장치 없이도 불린다.
// - camera: 화면을 볼 카메라
// - aspect: 화면의 너비 ÷ 높이. 0보다 커야 한다
// - alpha: Time의 보간 계수. 0 이상 1 이하다. 직전 tick과 현재 tick 사이를 이만큼 섞는다
RenderFrame CollectRenderFrame(const Scene& scene, const CameraComponent& camera, float aspect, float alpha);
