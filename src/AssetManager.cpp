#include "AssetManager.h"
#include <filesystem>
#include <iostream>
#include <Windows.h>

AssetManager::AssetManager(const std::string& rootFolder) : m_rootFolder(rootFolder)
{
}

std::string AssetManager::FindRootFolder()
{
    std::error_code ignored;
    const std::filesystem::path current = std::filesystem::current_path(ignored);
    if (std::filesystem::is_directory(current / "assets", ignored))
        return current.string();

    // 실행 파일의 경로를 얻어, 그 폴더에서 위로 세 단계까지 assets 폴더를 찾는다(bin\x64\Debug → 저장소 루트).
    wchar_t buffer[MAX_PATH] = {};
    const DWORD length = GetModuleFileNameW(nullptr, buffer, MAX_PATH);
    if (length > 0 && length < MAX_PATH)
    {
        std::filesystem::path folder = std::filesystem::path(buffer).parent_path();
        for (int up = 0; up < 4; ++up)
        {
            if (std::filesystem::is_directory(folder / "assets", ignored))
                return folder.string();
            folder = folder.parent_path();
        }
    }
    return current.string();
}

const ModelData& AssetManager::LoadModel(const std::string& relativePath)
{
    // 이미 읽은 원본이면 그것을 돌려준다. 모델이 몇 개뿐이라 처음부터 차례로 찾는다.
    for (const auto& model : m_models)
    {
        if (model->path == relativePath)
            return *model;
    }

    const std::filesystem::path fullPath = std::filesystem::path(m_rootFolder) / relativePath;
    auto model = std::make_unique<ModelData>(LoadGltf(fullPath.string(), relativePath));
    std::cout << "Load model: " << relativePath << " (vertices=" << model->mesh.vertices.size()
              << " triangles=" << model->mesh.indices.size() / 3 << " bones=" << model->boneCount
              << " clips=" << model->clipNames.size() << ")\n";
    m_models.push_back(std::move(model));
    return *m_models.back();
}
