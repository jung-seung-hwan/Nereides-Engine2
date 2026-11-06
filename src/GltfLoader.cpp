#include "GltfLoader.h"

// cgltf는 C로 쓰인 헤더 하나짜리 glTF 읽기 라이브러리다(third_party/cgltf, MIT).
// 프로젝트에서 딱 한 파일이 CGLTF_IMPLEMENTATION을 정의하고 포함한다. 그 파일이 여기다.
// 외부 코드라 경고 수준 4의 경고를 내지 않도록 포함하는 동안만 경고 수준을 낮춘다.
// C4996은 fopen·strcpy 같은 C 함수에 대한 MSVC의 "안전하지 않을 수 있다" 경고다. cgltf가 C99 표준 함수를 쓰므로 여기서만 끈다.
#define CGLTF_IMPLEMENTATION
#pragma warning(push, 3)
#pragma warning(disable : 4996)
#include <cgltf/cgltf.h>
#pragma warning(pop)

#include <DirectXMath.h>
#include <filesystem>
#include <stdexcept>

namespace
{
std::runtime_error LoadError(const std::string& displayPath, const std::string& reason)
{
    return std::runtime_error("Cannot load model '" + displayPath + "': " + reason);
}

const char* ResultName(cgltf_result result)
{
    switch (result)
    {
    case cgltf_result_data_too_short: return "data too short";
    case cgltf_result_unknown_format: return "unknown format";
    case cgltf_result_invalid_json: return "invalid JSON";
    case cgltf_result_invalid_gltf: return "invalid glTF";
    case cgltf_result_invalid_options: return "invalid options";
    case cgltf_result_file_not_found: return "file not found";
    case cgltf_result_io_error: return "I/O error";
    case cgltf_result_out_of_memory: return "out of memory";
    case cgltf_result_legacy_gltf: return "legacy glTF 1.0";
    default: return "unknown error";
    }
}

// 접근자(accessor)는 파일 안의 숫자 배열 하나를 가리킨다. 이름이 attributeName인 정점 속성의 접근자를 찾는다.
const cgltf_accessor* FindAttribute(const cgltf_primitive& primitive, cgltf_attribute_type type)
{
    for (cgltf_size i = 0; i < primitive.attributes_count; ++i)
    {
        if (primitive.attributes[i].type == type && primitive.attributes[i].index == 0)
            return primitive.attributes[i].data;
    }
    return nullptr;
}

// 노드 하나의 메시를 읽어 out에 더한다. 노드의 월드 변환을 정점에 적용한다.
// 스킨이 달린 노드는 규격대로 노드 변환을 무시한다(정점이 이미 뼈 기준 공간에 있다).
void AppendNodeMesh(const cgltf_node& node, const std::string& displayPath, MeshData& out)
{
    DirectX::XMMATRIX nodeToWorld = DirectX::XMMatrixIdentity();
    if (node.skin == nullptr)
    {
        // glTF 행렬은 열 우선(column-major)으로 16개가 늘어서 있고, 이동 값은 13~15번째다.
        // DirectXMath의 행 벡터 관례에서는 이동 값이 넷째 행(_41~_43)에 있는데, 16개를 차례로 넣으면 바로 그 자리에 들어간다.
        // 그래서 전치하지 않고 그대로 쓴다. 메모리에 놓인 순서가 같기 때문이다.
        cgltf_float world[16];
        cgltf_node_transform_world(&node, world);
        DirectX::XMFLOAT4X4 stored;
        for (int i = 0; i < 16; ++i)
            (&stored._11)[i] = world[i];
        nodeToWorld = DirectX::XMLoadFloat4x4(&stored);
    }
    // 행렬식이 0이면(크기에 0이 있으면) 역행렬이 없고 메시가 납작해진다. 그런 노드는 받지 않는다.
    const float determinant = DirectX::XMVectorGetX(DirectX::XMMatrixDeterminant(nodeToWorld));
    if (determinant > -1e-8f && determinant < 1e-8f)
        throw LoadError(displayPath, "unsupported data (a node has a zero scale)");
    // 법선은 위치와 같은 행렬로 돌리면 안 된다. 크기가 축마다 다르면(비균등) 면과 수직이 아니게 된다.
    // 역행렬의 전치가 법선용 행렬이다. 크기가 같으면 두 행렬은 같은 방향을 준다.
    const DirectX::XMMATRIX normalMatrix = DirectX::XMMatrixTranspose(DirectX::XMMatrixInverse(nullptr, nodeToWorld));
    // 행렬식이 음수면 노드 변환 자체가 거울이라 삼각형 방향이 한 번 더 뒤집힌다. 그때는 순서를 바꾸지 않아야 다시 맞는다.
    const bool mirroredNode = determinant < 0.0f;

    for (cgltf_size p = 0; p < node.mesh->primitives_count; ++p)
    {
        const cgltf_primitive& primitive = node.mesh->primitives[p];
        if (primitive.type != cgltf_primitive_type_triangles)
            throw LoadError(displayPath, "unsupported data (only triangle lists are supported)");
        const cgltf_accessor* positions = FindAttribute(primitive, cgltf_attribute_type_position);
        const cgltf_accessor* normals = FindAttribute(primitive, cgltf_attribute_type_normal);
        if (positions == nullptr)
            throw LoadError(displayPath, "unsupported data (a primitive has no POSITION)");
        if (normals == nullptr || normals->count != positions->count)
            throw LoadError(displayPath, "unsupported data (a primitive has no NORMAL for every vertex)");
        if (positions->type != cgltf_type_vec3 || normals->type != cgltf_type_vec3)
            throw LoadError(displayPath, "unsupported data (POSITION and NORMAL must be VEC3)");
        if (primitive.indices == nullptr)
            throw LoadError(displayPath, "unsupported data (a primitive has no indices)");
        if (primitive.indices->count % 3 != 0)
            throw LoadError(displayPath, "unsupported data (index count is not a multiple of 3)");

        // 이 묶음의 정점은 합친 배열의 base번째부터 들어간다. 인덱스에도 base를 더한다.
        const std::uint32_t base = static_cast<std::uint32_t>(out.vertices.size());
        for (cgltf_size i = 0; i < positions->count; ++i)
        {
            cgltf_float position[3] = {};
            cgltf_float normal[3] = {};
            if (!cgltf_accessor_read_float(positions, i, position, 3) || !cgltf_accessor_read_float(normals, i, normal, 3))
                throw LoadError(displayPath, "unsupported data (cannot read a vertex)");

            DirectX::XMVECTOR pos = DirectX::XMVectorSet(position[0], position[1], position[2], 1.0f);
            DirectX::XMVECTOR nrm = DirectX::XMVectorSet(normal[0], normal[1], normal[2], 0.0f);
            pos = DirectX::XMVector3TransformCoord(pos, nodeToWorld);
            nrm = DirectX::XMVector3Normalize(DirectX::XMVector3TransformNormal(nrm, normalMatrix));

            MeshVertex vertex;
            DirectX::XMStoreFloat3(&vertex.position, pos);
            DirectX::XMStoreFloat3(&vertex.normal, nrm);
            // 오른손 → 왼손: X를 뒤집는다. 법선도 같은 방향으로 뒤집는다.
            vertex.position.x = -vertex.position.x;
            vertex.normal.x = -vertex.normal.x;
            out.vertices.push_back(vertex);
        }

        for (cgltf_size i = 0; i < primitive.indices->count; i += 3)
        {
            const cgltf_size a = cgltf_accessor_read_index(primitive.indices, i);
            const cgltf_size b = cgltf_accessor_read_index(primitive.indices, i + 1);
            const cgltf_size c = cgltf_accessor_read_index(primitive.indices, i + 2);
            if (a >= positions->count || b >= positions->count || c >= positions->count)
                throw LoadError(displayPath, "unsupported data (an index points outside the vertex array)");
            // X를 뒤집으면 삼각형이 도는 방향도 뒤집힌다. 둘째와 셋째를 바꿔 다시 시계 방향(엔진의 앞면)으로 만든다.
            // 노드가 거울 변환이면 이미 한 번 뒤집혀 있으므로 그대로 둔다.
            out.indices.push_back(base + static_cast<std::uint32_t>(a));
            if (mirroredNode)
            {
                out.indices.push_back(base + static_cast<std::uint32_t>(b));
                out.indices.push_back(base + static_cast<std::uint32_t>(c));
            }
            else
            {
                out.indices.push_back(base + static_cast<std::uint32_t>(c));
                out.indices.push_back(base + static_cast<std::uint32_t>(b));
            }
        }
    }
}

// 노드와 그 자식들을 차례로 읽는다. 자식 안에 또 자식이 있을 수 있어서 자기 자신을 다시 부른다(재귀).
void AppendNodeTree(const cgltf_node& node, const std::string& displayPath, MeshData& out)
{
    if (node.mesh != nullptr)
        AppendNodeMesh(node, displayPath, out);
    for (cgltf_size i = 0; i < node.children_count; ++i)
        AppendNodeTree(*node.children[i], displayPath, out);
}
}

ModelData LoadGltf(const std::string& fullPath, const std::string& displayPath)
{
    // 없는 파일은 cgltf에 맡기지 않고 먼저 확인한다. 가장 흔한 실수를 가장 읽기 쉬운 말로 알리기 위해서다.
    if (!std::filesystem::exists(fullPath))
        throw LoadError(displayPath, "file not found (looked for '" + fullPath + "')");

    cgltf_options options = {};
    cgltf_data* data = nullptr;
    cgltf_result result = cgltf_parse_file(&options, fullPath.c_str(), &data);
    if (result != cgltf_result_success)
        throw LoadError(displayPath, std::string("not a glTF file (") + ResultName(result) + ")");

    // 여기서부터는 data를 반드시 cgltf_free로 돌려줘야 한다. 예외가 나도 돌려주도록 try로 감싼다.
    try
    {
        // .gltf 안의 base64 버퍼나 옆의 .bin 파일을 읽는다.
        result = cgltf_load_buffers(&options, data, fullPath.c_str());
        if (result != cgltf_result_success)
            throw LoadError(displayPath, std::string("cannot read the model's binary data (") + ResultName(result) + ")");
        result = cgltf_validate(data);
        if (result != cgltf_result_success)
            throw LoadError(displayPath, std::string("not a valid glTF file (") + ResultName(result) + ")");
        if (data->skins_count > 1)
            throw LoadError(displayPath, "unsupported data (more than one skin)");

        ModelData model;
        model.path = displayPath;
        // 파일이 가리키는 장면(없으면 첫 장면)의 뿌리 노드부터 자식을 따라가며 메시가 달린 노드만 읽는다.
        // 장면에 들어 있지 않은 노드는 그리지 않는다. 장면이 하나도 없으면 모든 노드를 읽는다.
        const cgltf_scene* scene = data->scene;
        if (scene == nullptr && data->scenes_count > 0)
            scene = &data->scenes[0];
        if (scene != nullptr)
        {
            for (cgltf_size i = 0; i < scene->nodes_count; ++i)
                AppendNodeTree(*scene->nodes[i], displayPath, model.mesh);
        }
        else
        {
            for (cgltf_size i = 0; i < data->nodes_count; ++i)
            {
                if (data->nodes[i].mesh != nullptr)
                    AppendNodeMesh(data->nodes[i], displayPath, model.mesh);
            }
        }
        if (model.mesh.vertices.empty() || model.mesh.indices.empty())
            throw LoadError(displayPath, "unsupported data (no triangle mesh in the file)");

        if (data->skins_count == 1)
            model.boneCount = data->skins[0].joints_count;
        for (cgltf_size i = 0; i < data->animations_count; ++i)
        {
            const char* name = data->animations[i].name;
            model.clipNames.push_back(name != nullptr ? name : "");
        }

        cgltf_free(data);
        return model;
    }
    catch (...)
    {
        cgltf_free(data);
        throw;
    }
}
