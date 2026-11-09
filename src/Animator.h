#pragma once
#include "Component.h"
#include "GltfLoader.h"
#include <string>
#include <vector>

// 이번 tick에 재생한 구간의 기록. 단계 5의 마커 판정이 읽는다.
// 보통 tick은 start ≤ m < end 인 마커 m을 지난 것으로 센다.
// wrapCount가 1 이상이면 토막이 둘이다: start ≤ m ≤ 클립 길이, 그리고 0 ≤ m < end. 그 사이에 클립 전체를 (wrapCount - 1)번 더 지났다.
// finished면 start ≤ m ≤ 클립 길이다(끝에 놓인 마커를 한 번 세기 위해 닫는다).
struct TickRecord
{
    bool played = false;   // 이번 tick에 재생이 진행됐나. 클립이 없거나 끝나 멈춘 뒤면 false
    int clip = -1;         // 재생한 클립 번호
    double start = 0.0;    // 이번 tick이 재생한 구간의 시작 시각(클립 안의 초)
    double end = 0.0;      // 구간의 끝 시각. 되감겼으면 되감긴 뒤의 값
    bool started = false;  // 이번 tick에 재생을 시작했나(Play 요청이 적용됐나)
    int wrapCount = 0;     // 반복 클립이 이번 tick에 끝을 지나 처음으로 되감긴 횟수. 클립이 tick보다 짧으면 2 이상일 수 있다
    bool finished = false; // 반복하지 않는 클립이 이번 tick에 끝에 닿았나. 한 번만 선다
};

// 객체 하나의 애니메이션 재생 상태. 클립·뼈 원본은 ModelData(AssetManager 소유)를 가리키기만 한다.
// 재생 시각, 자세, 뼈 행렬, tick 기록은 객체마다 따로 있다. 같은 원본을 쓰는 두 객체가 다른 시각을 재생할 수 있다.
// 유니티의 Animator 자리다. 다만 뼈가 GameObject가 아니라 이 컴포넌트 안의 배열이고, 전환은 바로 바뀐다(섞지 않는다).
class Animator : public Component
{
public:
    Animator(GameObject& owner, const ModelData& model);

    // 재생을 요청한다. 요청은 그 자리에서 저장되고, 자세는 같은 tick의 애니메이션 단계(AdvanceAnimation)에서 바뀐다.
    // 그 tick부터 시각 0으로 시작한다. 재생 중에 부르면 섞지 않고 새 클립으로 넘어간다. 같은 클립이면 처음부터 다시 한다.
    // 없는 이름이면 예외를 던진다.
    void Play(const std::string& clipName, bool loop);
    // 재생을 멈춘다. 자세는 그대로 둔다. 아직 적용되지 않은 Play 요청도 버린다.
    void Stop();

    // tick 안, Scene.FixedUpdate 뒤의 애니메이션 단계에서 Scene이 부른다.
    // 요청 적용 → FixedDeltaTime()만큼 진행 → 되감기·끝 처리 → 키에서 자세 읽기 → 뼈 행렬 계산 → 기록.
    // 정지·배속은 보지 않는다(학생 질문 2).
    void AdvanceAnimation() override;

    bool IsPlaying() const { return m_playing; }
    bool IsLooping() const { return m_loop; }
    // 지금 클립의 이름. 한 번도 재생하지 않았으면 빈 문자열.
    const std::string& ClipName() const;
    double PlaybackTime() const { return m_time; }
    const TickRecord& LastTick() const { return m_lastTick; }
    const ModelData& Model() const { return *m_model; }

    // 뼈마다 하나. 기본 자세의 정점을 지금 자세로 옮기는 행렬(모델 공간). ModelRenderer가 그리기 항목에 복사한다.
    const std::vector<DirectX::XMFLOAT4X4>& SkinMatrices() const { return m_skinMatrices; }
    // 뼈마다 하나. 뼈의 모델 공간 위치·방향. 단계 5·6에서 판정체나 무기를 뼈에 붙일 때 읽는다.
    const std::vector<DirectX::XMFLOAT4X4>& BoneWorldMatrices() const { return m_boneWorlds; }

private:
    // 뼈 하나의 지금 자세(부모 기준). 키에서 읽은 값이다.
    struct BonePose
    {
        DirectX::XMFLOAT3 translation{};
        DirectX::XMFLOAT4 rotation{0.0f, 0.0f, 0.0f, 1.0f};
        DirectX::XMFLOAT3 scale{1.0f, 1.0f, 1.0f};
    };

    // 모든 뼈를 기본 자세로 둔다.
    void ResetPoseToBind();
    // 클립의 시각 time에서 채널마다 값을 읽어 자세에 넣는다. 채널이 없는 뼈는 기본 자세다.
    void SamplePose(const AnimationClip& clip, double time);
    // 자세에서 뼈 행렬 두 벌을 계산한다. 부모가 앞 번호라 앞에서 뒤로 한 번 돈다.
    void ComputeMatrices();

    const ModelData* m_model;
    std::vector<BonePose> m_pose;
    std::vector<DirectX::XMFLOAT4X4> m_boneWorlds;
    std::vector<DirectX::XMFLOAT4X4> m_skinMatrices;

    int m_clip = -1;
    bool m_loop = false;
    bool m_playing = false;
    double m_time = 0.0;

    // Play 요청. 다음 AdvanceAnimation에서 적용한다.
    bool m_hasRequest = false;
    int m_requestedClip = -1;
    bool m_requestedLoop = false;

    TickRecord m_lastTick;
};
