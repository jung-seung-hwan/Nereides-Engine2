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

// glTF의 16개 숫자(열 우선)를 DirectXMath 행렬로 옮긴다.
// 이동 값은 13~15번째인데, 행 벡터 관례의 XMFLOAT4X4에서도 _41~_43이 같은 자리라 전치하지 않고 차례로 넣는다.
DirectX::XMFLOAT4X4 StoreGltfMatrix(const cgltf_float values[16])
{
    DirectX::XMFLOAT4X4 stored;
    for (int i = 0; i < 16; ++i)
        (&stored._11)[i] = values[i];
    return stored;
}

// 좌표계 변환(오른손 → 왼손, X 뒤집기)을 뼈 쪽 값에 맞춘다. 정점은 X를 뒤집으므로, 뼈의 움직임도 같은 거울에 비친 모양이어야 한다.
// 이동: x의 부호를 바꾼다. 회전 사원수: y와 z의 부호를 바꾼다(거울에 비친 회전은 축의 x만 남기고 방향이 반대다).
// 행렬: S·M·S (S는 X를 뒤집는 행렬). 첫 행과 첫 열에서 대각선이 아닌 원소의 부호가 바뀐다.
void MirrorTranslation(DirectX::XMFLOAT3& translation)
{
    translation.x = -translation.x;
}

void MirrorRotation(DirectX::XMFLOAT4& rotation)
{
    rotation.y = -rotation.y;
    rotation.z = -rotation.z;
}

void MirrorMatrix(DirectX::XMFLOAT4X4& m)
{
    m._12 = -m._12; m._13 = -m._13; m._14 = -m._14;
    m._21 = -m._21; m._31 = -m._31; m._41 = -m._41;
}

// 접근자(accessor)는 파일 안의 숫자 배열 하나를 가리킨다. 종류가 type이고 첫 번째(index 0)인 정점 속성의 접근자를 찾는다.
const cgltf_accessor* FindAttribute(const cgltf_primitive& primitive, cgltf_attribute_type type)
{
    for (cgltf_size i = 0; i < primitive.attributes_count; ++i)
    {
        if (primitive.attributes[i].type == type && primitive.attributes[i].index == 0)
            return primitive.attributes[i].data;
    }
    return nullptr;
}

// 스킨의 관절 가운데 node가 몇 번째인지. 관절이 아니면 -1.
int JointIndexOf(const cgltf_skin& skin, const cgltf_node* node)
{
    for (cgltf_size j = 0; j < skin.joints_count; ++j)
    {
        if (skin.joints[j] == node)
            return static_cast<int>(j);
    }
    return -1;
}

// 노드 하나의 부모 기준 행렬. 이동·회전·크기로 적혔든 행렬로 적혔든 cgltf가 행렬 하나로 준다.
DirectX::XMMATRIX LocalMatrix(const cgltf_node& node)
{
    cgltf_float local[16];
    cgltf_node_transform_local(&node, local);
    const DirectX::XMFLOAT4X4 stored = StoreGltfMatrix(local);
    return DirectX::XMLoadFloat4x4(&stored);
}

// 스킨의 뼈를 읽는다. 뼈 번호는 부모가 자식보다 앞에 오도록 다시 매긴다. 그래야 뼈 행렬을 앞에서 뒤로 한 번만 돌면 된다.
// jointToBone[i]는 glTF의 i번째 관절이 엔진에서 몇 번 뼈가 됐는지다. 정점의 뼈 번호를 고칠 때 쓴다.
void ReadSkeleton(const cgltf_skin& skin, const std::string& displayPath, std::vector<Bone>& bones,
                  std::vector<int>& jointToBone)
{
    const int jointCount = static_cast<int>(skin.joints_count);
    if (jointCount > MaxBones)
        throw LoadError(displayPath, "unsupported data (more than " + std::to_string(MaxBones) + " bones)");

    // 1. 관절마다 부모 관절을 찾고, 그 사이에 낀 관절 아닌 노드들의 변환을 따로 모아 둔다(parentOffset).
    //    (root 관절 → 보통 노드 → tip 관절 같은 파일에서 중간 노드의 이동·회전이 빠지지 않게 한다.)
    //    자기 변환에 합치지 않는 이유: 클립의 채널이 자기 이동·회전·크기를 통째로 바꾸므로, 합쳐 두면 재생 중에 중간 노드 몫이 사라진다.
    //    뿌리 관절 위의 노드들도 같은 방식으로 모은다. 스킨이 달린 메시 노드 자체의 변환은 4-1처럼 무시한다.
    std::vector<int> parentJoint(jointCount, -1);
    std::vector<DirectX::XMFLOAT4X4> offsets(static_cast<std::size_t>(jointCount));
    for (int i = 0; i < jointCount; ++i)
    {
        DirectX::XMMATRIX offset = DirectX::XMMatrixIdentity();
        const cgltf_node* ancestor = skin.joints[i]->parent;
        while (ancestor != nullptr && JointIndexOf(skin, ancestor) < 0)
        {
            // 먼저 적용할 것(가까운 조상)을 왼쪽에 둔다. 먼 조상의 변환이 오른쪽에 쌓인다.
            offset = offset * LocalMatrix(*ancestor);
            ancestor = ancestor->parent;
        }
        parentJoint[i] = ancestor != nullptr ? JointIndexOf(skin, ancestor) : -1;
        DirectX::XMStoreFloat4x4(&offsets[static_cast<std::size_t>(i)], offset);
    }

    // 2. 부모가 먼저 오는 순서를 만든다. 자리가 정해진 부모를 가진(또는 뿌리인) 관절을 차례로 붙인다.
    jointToBone.assign(jointCount, -1);
    std::vector<int> order;
    while (static_cast<int>(order.size()) < jointCount)
    {
        const std::size_t before = order.size();
        for (int i = 0; i < jointCount; ++i)
        {
            if (jointToBone[i] >= 0)
                continue;
            if (parentJoint[i] < 0 || jointToBone[parentJoint[i]] >= 0)
            {
                jointToBone[i] = static_cast<int>(order.size());
                order.push_back(i);
            }
        }
        if (order.size() == before)
            throw LoadError(displayPath, "unsupported data (the bone hierarchy has a cycle)");
    }

    // 3. 새 순서대로 뼈를 채운다.
    bones.clear();
    for (int joint : order)
    {
        const cgltf_node& node = *skin.joints[joint];
        Bone bone;
        bone.name = node.name != nullptr ? node.name : "";
        bone.parent = parentJoint[joint] < 0 ? -1 : jointToBone[parentJoint[joint]];

        // 자기 변환(부모 쪽 노드 기준)을 이동·회전·크기로 나눈다. 기본 자세는 이 셋으로 두고, 클립의 채널이 그중 일부를 바꾼다.
        // 기울임(shear)이 있는 행렬은 나눌 수 없다. 그런 파일은 받지 않는다.
        const DirectX::XMMATRIX local = LocalMatrix(node);
        DirectX::XMVECTOR scale, rotation, translation;
        if (!DirectX::XMMatrixDecompose(&scale, &rotation, &translation, local))
            throw LoadError(displayPath, "unsupported data (a bone transform cannot be split into scale, rotation and translation)");
        DirectX::XMStoreFloat3(&bone.scale, scale);
        DirectX::XMStoreFloat4(&bone.rotation, rotation);
        DirectX::XMStoreFloat3(&bone.translation, translation);
        MirrorTranslation(bone.translation);
        MirrorRotation(bone.rotation);
        // 사이에 낀 노드들의 변환은 행렬 그대로 두고 거울만 맞춘다(S·M·S).
        bone.parentOffset = offsets[static_cast<std::size_t>(joint)];
        MirrorMatrix(bone.parentOffset);

        // 역바인드 행렬. 파일에 없으면 규격대로 단위 행렬이다.
        DirectX::XMStoreFloat4x4(&bone.inverseBind, DirectX::XMMatrixIdentity());
        if (skin.inverse_bind_matrices != nullptr)
        {
            cgltf_float values[16] = {};
            if (!cgltf_accessor_read_float(skin.inverse_bind_matrices, static_cast<cgltf_size>(joint), values, 16))
                throw LoadError(displayPath, "unsupported data (cannot read an inverse bind matrix)");
            bone.inverseBind = StoreGltfMatrix(values);
            MirrorMatrix(bone.inverseBind);
        }
        bones.push_back(bone);
    }
}

// 노드가 스킨의 관절이면 엔진의 뼈 번호를, 아니면 -1을 돌려준다.
int BoneOfNode(const cgltf_skin* skin, const std::vector<int>& jointToBone, const cgltf_node* node)
{
    if (skin == nullptr || node == nullptr)
        return -1;
    for (cgltf_size i = 0; i < skin->joints_count; ++i)
    {
        if (skin->joints[i] == node)
            return jointToBone[i];
    }
    return -1;
}

// 애니메이션 클립들을 읽는다. 채널은 뼈 하나의 이동/회전/크기를 시각마다 정한다. 뼈가 아닌 노드를 움직이는 채널은 무시한다.
void ReadClips(const cgltf_data& data, const cgltf_skin* skin, const std::vector<int>& jointToBone,
               const std::string& displayPath, std::vector<AnimationClip>& clips)
{
    for (cgltf_size a = 0; a < data.animations_count; ++a)
    {
        const cgltf_animation& animation = data.animations[a];
        AnimationClip clip;
        clip.name = animation.name != nullptr ? animation.name : "";

        for (cgltf_size c = 0; c < animation.channels_count; ++c)
        {
            const cgltf_animation_channel& source = animation.channels[c];
            if (source.sampler == nullptr || source.sampler->input == nullptr || source.sampler->output == nullptr)
                throw LoadError(displayPath, "unsupported data (an animation channel has no sampler)");
            if (source.sampler->interpolation != cgltf_interpolation_type_linear)
                throw LoadError(displayPath, "unsupported data (only LINEAR animation interpolation is supported)");

            AnimationChannel channel;
            int components = 3;
            if (source.target_path == cgltf_animation_path_type_translation)
                channel.target = ChannelTarget::Translation;
            else if (source.target_path == cgltf_animation_path_type_rotation)
            {
                channel.target = ChannelTarget::Rotation;
                components = 4;
            }
            else if (source.target_path == cgltf_animation_path_type_scale)
                channel.target = ChannelTarget::Scale;
            else
                throw LoadError(displayPath, "unsupported data (only translation, rotation and scale channels are supported)");

            channel.bone = BoneOfNode(skin, jointToBone, source.target_node);
            if (channel.bone < 0)
                continue; // 뼈가 아닌 노드의 채널. 메시 노드를 통째로 움직이는 클립은 이 단계에서 받지 않는다.

            const cgltf_accessor& input = *source.sampler->input;
            const cgltf_accessor& output = *source.sampler->output;
            if (input.count == 0 || output.count != input.count)
                throw LoadError(displayPath, "unsupported data (animation keys and values must have the same count)");

            for (cgltf_size k = 0; k < input.count; ++k)
            {
                cgltf_float time = 0.0f;
                cgltf_float value[4] = {};
                if (!cgltf_accessor_read_float(&input, k, &time, 1) ||
                    !cgltf_accessor_read_float(&output, k, value, static_cast<cgltf_size>(components)))
                    throw LoadError(displayPath, "unsupported data (cannot read an animation key)");
                // 규격대로 키 시각은 앞 키보다 커야 한다. 같은 시각이 두 번 오면 그 사이의 보간 폭이 0이 된다.
                if (!channel.times.empty() && time <= channel.times.back())
                    throw LoadError(displayPath, "unsupported data (animation key times must strictly increase)");

                DirectX::XMFLOAT4 stored = {value[0], value[1], value[2], value[3]};
                if (channel.target == ChannelTarget::Rotation)
                    MirrorRotation(stored);
                else if (channel.target == ChannelTarget::Translation)
                    stored.x = -stored.x;
                channel.times.push_back(time);
                channel.values.push_back(stored);
            }
            if (channel.times.back() > clip.duration)
                clip.duration = channel.times.back();
            clip.channels.push_back(channel);
        }
        clips.push_back(clip);
    }
}

// 노드 하나의 메시를 읽어 out에 더한다. 노드의 월드 변환을 정점에 적용한다.
// 스킨이 달린 노드는 규격대로 노드 변환을 무시하고(정점이 이미 뼈 기준 공간에 있다), 정점의 뼈 번호·가중치를 읽는다.
void AppendNodeMesh(const cgltf_node& node, const std::vector<int>& jointToBone, const std::string& displayPath,
                    MeshData& out)
{
    DirectX::XMMATRIX nodeToWorld = DirectX::XMMatrixIdentity();
    if (node.skin == nullptr)
    {
        cgltf_float world[16];
        cgltf_node_transform_world(&node, world);
        const DirectX::XMFLOAT4X4 stored = StoreGltfMatrix(world);
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

        // 스킨 노드의 정점은 뼈 번호(JOINTS_0)와 가중치(WEIGHTS_0)가 있어야 한다.
        const cgltf_accessor* jointsAccessor = nullptr;
        const cgltf_accessor* weightsAccessor = nullptr;
        if (node.skin != nullptr)
        {
            jointsAccessor = FindAttribute(primitive, cgltf_attribute_type_joints);
            weightsAccessor = FindAttribute(primitive, cgltf_attribute_type_weights);
            if (jointsAccessor == nullptr || weightsAccessor == nullptr || jointsAccessor->count != positions->count ||
                weightsAccessor->count != positions->count)
                throw LoadError(displayPath, "unsupported data (a skinned primitive needs JOINTS_0 and WEIGHTS_0 for every vertex)");
            if (jointsAccessor->type != cgltf_type_vec4 || weightsAccessor->type != cgltf_type_vec4)
                throw LoadError(displayPath, "unsupported data (JOINTS_0 and WEIGHTS_0 must be VEC4)");
        }

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

            if (jointsAccessor != nullptr)
            {
                // 뼈 번호는 파일의 관절 번호다. 부모 먼저 순서로 다시 매긴 엔진의 뼈 번호로 바꾼다.
                cgltf_uint joints[4] = {};
                cgltf_float weights[4] = {};
                if (!cgltf_accessor_read_uint(jointsAccessor, i, joints, 4) ||
                    !cgltf_accessor_read_float(weightsAccessor, i, weights, 4))
                    throw LoadError(displayPath, "unsupported data (cannot read a vertex's bones)");
                for (int j = 0; j < 4; ++j)
                {
                    if (joints[j] >= jointToBone.size())
                        throw LoadError(displayPath, "unsupported data (a vertex points to a bone that does not exist)");
                    vertex.joints[j] = static_cast<std::uint8_t>(jointToBone[joints[j]]);
                    vertex.weights[j] = weights[j];
                }
            }
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
void AppendNodeTree(const cgltf_node& node, const std::vector<int>& jointToBone, const std::string& displayPath,
                    MeshData& out)
{
    if (node.mesh != nullptr)
        AppendNodeMesh(node, jointToBone, displayPath, out);
    for (cgltf_size i = 0; i < node.children_count; ++i)
        AppendNodeTree(*node.children[i], jointToBone, displayPath, out);
}
}

int ModelData::FindClip(const std::string& name) const
{
    for (std::size_t i = 0; i < clips.size(); ++i)
    {
        if (clips[i].name == name)
            return static_cast<int>(i);
    }
    return -1;
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

        // 뼈를 메시보다 먼저 읽는다. 정점의 뼈 번호를 새 순서로 바꾸려면 jointToBone이 필요하다.
        const cgltf_skin* skin = data->skins_count == 1 ? &data->skins[0] : nullptr;
        std::vector<int> jointToBone;
        if (skin != nullptr)
            ReadSkeleton(*skin, displayPath, model.bones, jointToBone);

        // 파일이 가리키는 장면(없으면 첫 장면)의 뿌리 노드부터 자식을 따라가며 메시가 달린 노드만 읽는다.
        // 장면에 들어 있지 않은 노드는 그리지 않는다. 장면이 하나도 없으면 모든 노드를 읽는다.
        const cgltf_scene* scene = data->scene;
        if (scene == nullptr && data->scenes_count > 0)
            scene = &data->scenes[0];
        if (scene != nullptr)
        {
            for (cgltf_size i = 0; i < scene->nodes_count; ++i)
                AppendNodeTree(*scene->nodes[i], jointToBone, displayPath, model.mesh);
        }
        else
        {
            for (cgltf_size i = 0; i < data->nodes_count; ++i)
            {
                if (data->nodes[i].mesh != nullptr)
                    AppendNodeMesh(data->nodes[i], jointToBone, displayPath, model.mesh);
            }
        }
        if (model.mesh.vertices.empty() || model.mesh.indices.empty())
            throw LoadError(displayPath, "unsupported data (no triangle mesh in the file)");

        ReadClips(*data, skin, jointToBone, displayPath, model.clips);

        cgltf_free(data);
        return model;
    }
    catch (...)
    {
        cgltf_free(data);
        throw;
    }
}
