#include "AssetTests.h"
#include "AssetManager.h"
#include <DirectXMath.h>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <Windows.h>

namespace
{
const char* TestModelPath = "assets/test/arrow.gltf";

void Check(bool condition, const char* message)
{
    if (!condition)
        throw std::runtime_error(message);
}

bool Contains(const std::string& text, const char* piece)
{
    return text.find(piece) != std::string::npos;
}

// 실행마다 다른 이름의 임시 폴더. 만들 때 생기고, 이 객체가 사라질 때(시험이 예외로 끝나도) 폴더를 지운다.
struct TempFolder
{
    std::filesystem::path path;
    TempFolder()
    {
        // 프로세스 번호와 시각을 함께 써서 Debug·Release를 동시에 돌려도 이름이 겹치지 않게 한다.
        const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
        path = std::filesystem::temp_directory_path() /
               ("nereides-asset-tests-" + std::to_string(GetCurrentProcessId()) + "-" + std::to_string(stamp));
        std::filesystem::create_directories(path);
    }
    ~TempFolder()
    {
        std::error_code ignored;
        std::filesystem::remove_all(path, ignored);
    }
};

// 시험 모델: tools/make_test_model.py가 만든 화살표와 받침. 정점 72+24개, 삼각형 36+12개, 뼈 2개, 클립 "bend"·"swing" 2개.
// 뼈·클립의 내용은 --animation-tests가 본다. 여기서는 개수만 본다.
void TestLoadTestModel(const std::string& root)
{
    AssetManager assets(root);
    const ModelData& model = assets.LoadModel(TestModelPath);
    Check(model.path == TestModelPath, "The model must remember the requested path.");
    Check(model.mesh.vertices.size() == 96, "The arrow and its base must have 96 vertices.");
    Check(model.mesh.indices.size() == 144, "The arrow and its base must have 144 indices (48 triangles).");
    Check(model.bones.size() == 2, "The arrow must report 2 bones.");
    Check(model.clips.size() == 2 && model.clips[0].name == "bend" && model.clips[1].name == "swing",
          "The arrow must report the clips bend and swing.");
}

// 좌표계 변환: glTF의 오른손 좌표계를 엔진의 왼손 좌표계로 바꾼 결과를 본다.
void TestCoordinateConversion(const std::string& root)
{
    AssetManager assets(root);
    const MeshData& mesh = assets.LoadModel(TestModelPath).mesh;

    // 화살표는 glTF의 +Z로 뻗는다. 엔진에서도 앞은 +Z여야 한다(끝이 z=1.4).
    // 날개는 glTF의 -X 쪽에 있다. X를 뒤집었으므로 엔진에서는 +X 쪽(x가 0.1 이상)에 있어야 하고, -0.1보다 작은 x는 없어야 한다.
    float maxZ = 0.0f, minX = 0.0f, maxX = 0.0f;
    for (const MeshVertex& vertex : mesh.vertices)
    {
        if (vertex.position.z > maxZ) maxZ = vertex.position.z;
        if (vertex.position.x < minX) minX = vertex.position.x;
        if (vertex.position.x > maxX) maxX = vertex.position.x;
    }
    Check(maxZ > 1.39f && maxZ < 1.41f, "The arrow must still point to +Z after conversion.");
    Check(maxX > 0.39f && maxX < 0.41f, "The wing must be on the +X side after flipping X.");
    Check(minX > -0.31f, "No vertex may remain on the far -X side after flipping X.");

    // 받침은 뼈가 없는 노드라 노드 변환(이동 0,-0.3,0.2와 크기 2,1,1)이 정점에 적용된다.
    // 받침 상자는 원점 둘레 ±0.15, 높이 ±0.05였다. 변환 뒤 y는 -0.35~-0.25, x는 ±0.3, z는 0.05~0.35여야 한다.
    float baseMinY = 0.0f, baseMinX = 0.0f, baseMaxX = 0.0f, baseMinZ = 10.0f, baseMaxZ = -10.0f;
    for (const MeshVertex& vertex : mesh.vertices)
    {
        if (vertex.position.y > -0.2f)
            continue; // 화살표의 정점은 y가 -0.1 이상이다
        if (vertex.position.y < baseMinY) baseMinY = vertex.position.y;
        if (vertex.position.x < baseMinX) baseMinX = vertex.position.x;
        if (vertex.position.x > baseMaxX) baseMaxX = vertex.position.x;
        if (vertex.position.z < baseMinZ) baseMinZ = vertex.position.z;
        if (vertex.position.z > baseMaxZ) baseMaxZ = vertex.position.z;
    }
    Check(baseMinY > -0.36f && baseMinY < -0.34f, "The base must be moved down by the node translation.");
    Check(baseMinX > -0.31f && baseMinX < -0.29f && baseMaxX > 0.29f && baseMaxX < 0.31f,
          "The base must be scaled by 2 on X by the node scale.");
    Check(baseMinZ > 0.04f && baseMinZ < 0.06f && baseMaxZ > 0.34f && baseMaxZ < 0.36f,
          "The base must be moved forward by the node translation.");

    // 앞면 규칙: 3-2의 상자와 같은 검사다. 삼각형 (a, b, c)의 (b-a)×(c-a)가 법선 쪽을 향하면 바깥에서 볼 때 시계 방향이다.
    for (std::size_t i = 0; i + 2 < mesh.indices.size(); i += 3)
    {
        const MeshVertex& a = mesh.vertices[mesh.indices[i]];
        const MeshVertex& b = mesh.vertices[mesh.indices[i + 1]];
        const MeshVertex& c = mesh.vertices[mesh.indices[i + 2]];
        const DirectX::XMVECTOR pa = DirectX::XMLoadFloat3(&a.position);
        const DirectX::XMVECTOR pb = DirectX::XMLoadFloat3(&b.position);
        const DirectX::XMVECTOR pc = DirectX::XMLoadFloat3(&c.position);
        const DirectX::XMVECTOR face = DirectX::XMVector3Cross(DirectX::XMVectorSubtract(pb, pa),
                                                               DirectX::XMVectorSubtract(pc, pa));
        const float facing = DirectX::XMVectorGetX(DirectX::XMVector3Dot(face, DirectX::XMLoadFloat3(&a.normal)));
        Check(facing > 0.0f, "Every loaded triangle must be clockwise when seen from outside.");
        // 법선 길이는 1이어야 한다.
        const float length = DirectX::XMVectorGetX(DirectX::XMVector3Length(DirectX::XMLoadFloat3(&a.normal)));
        Check(length > 0.999f && length < 1.001f, "Every normal must have length 1.");
    }
}

// 공유: 같은 경로를 두 번 요청하면 파일을 다시 읽지 않고 같은 원본(같은 주소)을 돌려준다.
void TestSharing(const std::string& root)
{
    AssetManager assets(root);
    const ModelData& first = assets.LoadModel(TestModelPath);
    const ModelData& second = assets.LoadModel(TestModelPath);
    Check(&first == &second, "The same path must return the same model.");
    // 같은 파일을 다른 표기로 적어도 같은 원본이다.
    const ModelData& third = assets.LoadModel("./assets\\test/arrow.gltf");
    Check(&first == &third, "A differently written path to the same file must return the same model.");
    Check(assets.ModelCount() == 1, "The same path must be loaded only once.");
}

// 오류 보고: 없는 파일, glTF가 아닌 파일, 지원하지 않는 데이터. 메시지에 경로와 이유가 들어간다.
void TestErrors(const std::string& root)
{
    AssetManager assets(root);

    std::string message;
    try { assets.LoadModel("assets/test/missing.gltf"); } catch (const std::runtime_error& error) { message = error.what(); }
    Check(Contains(message, "assets/test/missing.gltf") && Contains(message, "file not found"),
          "A missing file must be reported with its path.");

    // 시험 파일은 임시 폴더에 만든다. 저장소에 쓰레기 파일을 남기지 않고, 시험이 실패해도 폴더를 지운다.
    const TempFolder folder;
    const std::filesystem::path& temp = folder.path;
    AssetManager tempAssets(temp.string());

    // 경로 계약: 절대 경로와 루트 밖으로 나가는 경로는 읽기 전에 거부한다.
    message.clear();
    try { assets.LoadModel("../outside.gltf"); } catch (const std::runtime_error& error) { message = error.what(); }
    Check(Contains(message, "inside the asset root"), "A path with .. must be rejected.");
    message.clear();
    try { assets.LoadModel((temp / "absolute.gltf").string()); } catch (const std::runtime_error& error) { message = error.what(); }
    Check(Contains(message, "must be relative"), "An absolute path must be rejected.");

    {
        std::ofstream file(temp / "not-a-model.gltf");
        file << "hello, this is not a model\n";
    }
    message.clear();
    try { tempAssets.LoadModel("not-a-model.gltf"); } catch (const std::runtime_error& error) { message = error.what(); }
    Check(Contains(message, "not-a-model.gltf") && Contains(message, "not a glTF file"),
          "A file that is not glTF must be reported as such.");

    // 시험 모델의 그리기 방식을 삼각형(4)에서 점(0)으로 바꾼 사본을 만든다. 지원하지 않는 데이터다.
    {
        std::ifstream source(std::filesystem::path(root) / TestModelPath);
        std::string text((std::istreambuf_iterator<char>(source)), std::istreambuf_iterator<char>());
        const std::string::size_type at = text.find("\"mode\": 4");
        Check(at != std::string::npos, "The test model must declare triangle mode.");
        text.replace(at, 9, "\"mode\": 0");
        std::ofstream file(temp / "points.gltf");
        file << text;
    }
    message.clear();
    try { tempAssets.LoadModel("points.gltf"); } catch (const std::runtime_error& error) { message = error.what(); }
    Check(Contains(message, "points.gltf") && Contains(message, "unsupported data"),
          "Unsupported primitive types must be reported as unsupported data.");
    Check(tempAssets.ModelCount() == 0, "A failed load must not leave a model behind.");
}
}

int RunAssetTests(const std::string& rootFolder)
{
    try
    {
        TestLoadTestModel(rootFolder);
        TestCoordinateConversion(rootFolder);
        TestSharing(rootFolder);
        TestErrors(rootFolder);
        std::cout << "Asset tests PASS (4 cases)\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "Asset tests FAIL: " << error.what() << '\n';
        return 1;
    }
}
