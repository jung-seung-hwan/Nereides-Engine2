#include "Animator.h"
#include "GameObject.h"
#include "Time.h"
#include <cmath>
#include <stdexcept>

namespace
{
const std::string NoClipName;

// 채널의 키에서 시각 time의 값을 읽는다. 첫 키 앞은 첫 값, 마지막 키 뒤는 마지막 값이다.
// 두 키 사이는 이동·크기면 직선으로, 회전이면 사원수 구면 보간(slerp)으로 섞는다. glTF의 LINEAR 회전이 그 뜻이다.
DirectX::XMFLOAT4 SampleChannel(const AnimationChannel& channel, double time)
{
    const std::vector<float>& times = channel.times;
    const float t = static_cast<float>(time);
    if (t <= times.front())
        return channel.values.front();
    if (t >= times.back())
        return channel.values.back();

    // 키가 몇 개뿐이라 처음부터 차례로 찾는다. times[i] ≤ t < times[i + 1]인 i를 찾는다.
    std::size_t i = 0;
    while (i + 1 < times.size() && times[i + 1] <= t)
        ++i;
    const float span = times[i + 1] - times[i];
    const float u = span > 0.0f ? (t - times[i]) / span : 0.0f;

    const DirectX::XMVECTOR a = DirectX::XMLoadFloat4(&channel.values[i]);
    const DirectX::XMVECTOR b = DirectX::XMLoadFloat4(&channel.values[i + 1]);
    DirectX::XMVECTOR mixed;
    if (channel.target == ChannelTarget::Rotation)
        mixed = DirectX::XMQuaternionNormalize(
            DirectX::XMQuaternionSlerp(DirectX::XMQuaternionNormalize(a), DirectX::XMQuaternionNormalize(b), u));
    else
        mixed = DirectX::XMVectorLerp(a, b, u);
    DirectX::XMFLOAT4 result;
    DirectX::XMStoreFloat4(&result, mixed);
    return result;
}
}

Animator::Animator(GameObject& owner, const ModelData& model) : Component(owner), m_model(&model)
{
    // 한 번도 재생하지 않아도 기본 자세로 그려지도록 뼈 행렬을 미리 계산해 둔다.
    m_pose.resize(model.bones.size());
    m_boneWorlds.resize(model.bones.size());
    m_skinMatrices.resize(model.bones.size());
    ResetPoseToBind();
    ComputeMatrices();
}

void Animator::Play(const std::string& clipName, bool loop)
{
    const int clip = m_model->FindClip(clipName);
    if (clip < 0)
        throw std::invalid_argument("The model '" + m_model->path + "' has no clip named '" + clipName + "'.");
    m_hasRequest = true;
    m_requestedClip = clip;
    m_requestedLoop = loop;
}

void Animator::Stop()
{
    m_hasRequest = false;
    m_playing = false;
}

const std::string& Animator::ClipName() const
{
    if (m_clip < 0)
        return NoClipName;
    return m_model->clips[static_cast<std::size_t>(m_clip)].name;
}

void Animator::AdvanceAnimation()
{
    TickRecord record;

    // 1. 기다리던 요청을 적용한다. 이번 tick부터 시각 0으로 시작한다.
    if (m_hasRequest)
    {
        m_hasRequest = false;
        m_clip = m_requestedClip;
        m_loop = m_requestedLoop;
        m_time = 0.0;
        m_playing = true;
        record.started = true;
    }
    if (!m_playing)
    {
        // 재생 중이 아니면 자세와 행렬은 그대로 둔다. 기록만 "진행 없음"으로 남긴다.
        m_lastTick = record;
        return;
    }

    // 2. tick 하나만큼 진행한다. 배속·정지는 tick 수에 이미 들어 있다.
    const AnimationClip& clip = m_model->clips[static_cast<std::size_t>(m_clip)];
    const double duration = clip.duration;
    record.played = true;
    record.clip = m_clip;
    record.start = m_time;
    m_time += GetTime().FixedDeltaTime();

    // 3. 끝을 지났으면 반복 클립은 되감고, 아니면 끝에서 멈춘다. 길이가 0인 클립은 되감을 수 없으므로 바로 끝낸다.
    if (m_time >= duration)
    {
        if (m_loop && duration > 0.0)
        {
            record.wrapped = true;
            m_time = std::fmod(m_time, duration);
        }
        else
        {
            m_time = duration;
            record.finished = true;
            m_playing = false;
        }
    }
    record.end = m_time;

    // 4~5. 지금 시각의 자세를 읽고 뼈 행렬을 만든다.
    SamplePose(clip, m_time);
    ComputeMatrices();
    m_lastTick = record;
}

void Animator::ResetPoseToBind()
{
    for (std::size_t i = 0; i < m_pose.size(); ++i)
    {
        const Bone& bone = m_model->bones[i];
        m_pose[i].translation = bone.translation;
        m_pose[i].rotation = bone.rotation;
        m_pose[i].scale = bone.scale;
    }
}

void Animator::SamplePose(const AnimationClip& clip, double time)
{
    ResetPoseToBind();
    for (const AnimationChannel& channel : clip.channels)
    {
        if (channel.times.empty())
            continue;
        const DirectX::XMFLOAT4 value = SampleChannel(channel, time);
        BonePose& pose = m_pose[static_cast<std::size_t>(channel.bone)];
        if (channel.target == ChannelTarget::Translation)
            pose.translation = {value.x, value.y, value.z};
        else if (channel.target == ChannelTarget::Rotation)
            pose.rotation = value;
        else
            pose.scale = {value.x, value.y, value.z};
    }
}

void Animator::ComputeMatrices()
{
    for (std::size_t i = 0; i < m_pose.size(); ++i)
    {
        const BonePose& pose = m_pose[i];
        const Bone& bone = m_model->bones[i];
        // 부모 기준 행렬 = 크기 × 회전 × 이동. Transform의 월드 행렬과 같은 순서다.
        const DirectX::XMMATRIX local = DirectX::XMMatrixScaling(pose.scale.x, pose.scale.y, pose.scale.z) *
                                        DirectX::XMMatrixRotationQuaternion(DirectX::XMLoadFloat4(&pose.rotation)) *
                                        DirectX::XMMatrixTranslation(pose.translation.x, pose.translation.y, pose.translation.z);
        // 모델 공간 행렬 = 부모 기준 행렬 × 부모의 모델 공간 행렬. 부모는 앞 번호라 이미 계산돼 있다.
        DirectX::XMMATRIX world = local;
        if (bone.parent >= 0)
            world = local * DirectX::XMLoadFloat4x4(&m_boneWorlds[static_cast<std::size_t>(bone.parent)]);
        DirectX::XMStoreFloat4x4(&m_boneWorlds[i], world);
        // 스키닝 행렬 = 역바인드 × 모델 공간 행렬. 기본 자세의 정점을 뼈 기준으로 옮겼다가 지금 자세로 되돌린다.
        // 기본 자세 그대로면 둘이 서로 지워져 단위 행렬이 된다.
        DirectX::XMStoreFloat4x4(&m_skinMatrices[i], DirectX::XMLoadFloat4x4(&bone.inverseBind) * world);
    }
}
