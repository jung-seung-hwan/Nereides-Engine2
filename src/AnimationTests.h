#pragma once
#include <string>

// 단계 4-2의 재생·스키닝 데이터 시험. GPU 없이 돈다. 시험 모델이 있는 저장소 루트를 받는다.
// 통과하면 0, 실패하면 1을 돌려준다.
int RunAnimationTests(const std::string& rootFolder);
