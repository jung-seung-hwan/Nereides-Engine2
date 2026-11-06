#pragma once
#include "GltfLoader.h"
#include <memory>
#include <string>
#include <vector>

// 모델 원본의 주인이다. 경로로 모델을 읽어 두고, 같은 경로를 다시 요청하면 이미 읽은 원본을 돌려준다.
// 원본은 프로그램이 끝날 때까지 갖고 있고 실행 중에 내리지 않는다.
// 그래서 ModelRenderer가 가리키는 MeshData 주소와 Renderer가 열쇠로 쓰는 원본 주소가 끝까지 유효하다.
// Application이 소유하며, Renderer와 Scene보다 먼저 선언해 더 오래 산다.
// 컴포넌트는 이 클래스를 모른다. Application이 장면을 만들 때 읽어서 컴포넌트에 넘긴다.
class AssetManager
{
public:
    // rootFolder: 상대 경로의 기준 폴더. 저장소 루트(assets 폴더가 있는 곳)다.
    explicit AssetManager(const std::string& rootFolder);

    // rootFolder 기준 상대 경로로 모델을 읽는다. 처음이면 파일을 읽고, 다시 요청하면 같은 원본을 돌려준다.
    // 실패하면 경로와 이유가 적힌 std::runtime_error가 올라온다.
    const ModelData& LoadModel(const std::string& relativePath);

    std::size_t ModelCount() const { return m_models.size(); }
    const std::string& RootFolder() const { return m_rootFolder; }

    // assets 폴더가 있는 저장소 루트를 찾는다. 현재 폴더부터 보고, 없으면 실행 파일 위치의 위 폴더들을 본다.
    // VS 디버거와 CI는 저장소 루트에서 실행하므로 현재 폴더가 맞는다. bin\x64\Debug의 실행 파일을 직접 열어도 찾도록 했다.
    // 어디에도 없으면 현재 폴더를 돌려주고, 읽을 때 "file not found" 오류가 경로를 알려 준다.
    static std::string FindRootFolder();

private:
    std::string m_rootFolder;
    // unique_ptr로 두면 목록이 늘어나도 각 원본의 주소가 그대로다. 주소를 가리키는 쪽이 있으므로 중요하다.
    std::vector<std::unique_ptr<ModelData>> m_models;
};
