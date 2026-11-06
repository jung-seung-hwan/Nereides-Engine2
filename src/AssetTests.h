#pragma once
#include <string>

// 모델 읽기 검증. GPU 없이 돈다. rootFolder는 assets 폴더가 있는 저장소 루트다.
int RunAssetTests(const std::string& rootFolder);
