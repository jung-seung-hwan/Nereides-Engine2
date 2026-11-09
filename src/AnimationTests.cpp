#include "AnimationTests.h"
#include "Animator.h"
#include "AssetManager.h"
#include "GameObject.h"
#include "Input.h"
#include "PlayerControl.h"
#include "Scene.h"
#include "Time.h"
#include <chrono>
#include <cmath>
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

bool Near(double a, double b, double tolerance = 1e-3)
{
    return std::fabs(a - b) <= tolerance;
}

bool Contains(const std::string& text, const char* piece)
{
    return text.find(piece) != std::string::npos;
}

// 행렬 m으로 점 p를 옮긴다(w=1). Animator의 스키닝 행렬을 정점 하나에 적용해 볼 때 쓴다.
DirectX::XMFLOAT3 Apply(const DirectX::XMFLOAT4X4& m, const DirectX::XMFLOAT3& p)
{
    const DirectX::XMVECTOR moved =
        DirectX::XMVector3TransformCoord(DirectX::XMLoadFloat3(&p), DirectX::XMLoadFloat4x4(&m));
    DirectX::XMFLOAT3 result;
    DirectX::XMStoreFloat3(&result, moved);
    return result;
}

bool IsIdentity(const DirectX::XMFLOAT4X4& m)
{
    for (int r = 0; r < 4; ++r)
    {
        for (int c = 0; c < 4; ++c)
        {
            const float expected = r == c ? 1.0f : 0.0f;
            if (!Near(m.m[r][c], expected, 1e-5))
                return false;
        }
    }
    return true;
}

// 좌표가 (x, y, z)인 정점을 찾는다. 없으면 예외.
const MeshVertex& FindVertex(const MeshData& mesh, float x, float y, float z)
{
    for (const MeshVertex& vertex : mesh.vertices)
    {
        if (Near(vertex.position.x, x) && Near(vertex.position.y, y) && Near(vertex.position.z, z))
            return vertex;
    }
    throw std::runtime_error("The test model must have the vertex the test looks for.");
}

// Application::RunTick과 같은 순서로 tick 하나를 돌린다: 입력 소비 → BeginTick → FixedUpdate → 애니메이션 단계 → EndTick.
// Time의 약속대로 BeginFrame이 정한 tick 안에서만 부른다.
void RunTickInsideFrame(Scene& scene, Input& input, Time& time)
{
    input.ConsumeForTick();
    scene.BeginTick();
    scene.SavePreviousTransforms();
    scene.FixedUpdate();
    scene.AdvanceAnimation();
    scene.EndTick();
    time.FinishTick();
}

// Application::AdvanceGame과 같은 순서로 프레임 하나를 돌린다. 실제로 흐른 시간을 넣으면 Time이 tick 수를 정한다.
int RunFrame(Scene& scene, Input& input, Time& time, double realSeconds)
{
    const int ticks = time.BeginFrame(realSeconds);
    for (int i = 0; i < ticks; ++i)
        RunTickInsideFrame(scene, input, time);
    scene.Update();
    return ticks;
}

// tick 하나짜리 프레임. 실제 시간으로 tick 길이를 넣으면 Time이 tick을 정확히 하나 준다.
void RunTick(Scene& scene, Input& input, Time& time)
{
    const int ticks = RunFrame(scene, input, time, time.FixedDeltaTime());
    Check(ticks == 1, "A frame as long as one tick must run exactly one tick.");
}

// 시험마다 새로 만드는 장면. Scene은 Input·Time을 참조하므로 둘을 먼저 둔다.
struct TestWorld
{
    Input input;
    Time time;
    Scene scene;
    explicit TestWorld(double fixedDeltaTime) : time(fixedDeltaTime, 5), scene(input, time) {}
};

// 객체에 Animator를 붙이고 주소를 돌려준다. unique_ptr을 옮겨도 가리키는 객체의 주소는 그대로다.
Animator& AddAnimator(GameObject& object, const ModelData& model)
{
    auto animator = std::make_unique<Animator>(object, model);
    Animator& result = *animator;
    object.AddComponent(std::move(animator));
    return result;
}

// 실행마다 다른 이름의 임시 폴더. 이 객체가 사라질 때 폴더를 지운다(AssetTests와 같다).
struct TempFolder
{
    std::filesystem::path path;
    TempFolder()
    {
        const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
        path = std::filesystem::temp_directory_path() /
               ("nereides-animation-tests-" + std::to_string(GetCurrentProcessId()) + "-" + std::to_string(stamp));
        std::filesystem::create_directories(path);
    }
    ~TempFolder()
    {
        std::error_code ignored;
        std::filesystem::remove_all(path, ignored);
    }
};

// 1. 읽기: 뼈 2개(root → tip), 클립 2개, 화살표 정점은 뼈를 따르고 받침 정점은 따르지 않는다.
void TestLoadSkeleton(const ModelData& model)
{
    Check(model.bones.size() == 2, "The arrow must have 2 bones.");
    Check(model.bones[0].name == "root" && model.bones[0].parent == -1, "Bone 0 must be the root without a parent.");
    Check(model.bones[1].name == "tip" && model.bones[1].parent == 0, "Bone 1 must be the tip whose parent is the root.");
    Check(Near(model.bones[1].translation.z, 1.0f) && Near(model.bones[1].translation.x, 0.0f),
          "The tip must sit 1 unit along +Z from the root.");
    Check(model.clips.size() == 2 && model.FindClip("bend") == 0 && model.FindClip("swing") == 1 &&
              model.FindClip("none") == -1,
          "The clips must be bend and swing, found by name.");
    Check(Near(model.clips[0].duration, 1.0f) && Near(model.clips[1].duration, 1.0f), "Both clips must last 1 second.");
    Check(model.clips[0].channels.size() == 1 && model.clips[1].channels.size() == 2,
          "bend must have 1 channel and swing 2 channels.");

    // 정점: z가 1 이상이면 tip, 아니면 root를 가중치 1로 따른다. 받침(y가 -0.2보다 낮다)은 가중치가 모두 0이다.
    int skinned = 0, unskinned = 0;
    for (const MeshVertex& vertex : model.mesh.vertices)
    {
        const float weightSum = vertex.weights[0] + vertex.weights[1] + vertex.weights[2] + vertex.weights[3];
        if (vertex.position.y < -0.2f)
        {
            Check(weightSum == 0.0f, "Base vertices must not follow any bone.");
            ++unskinned;
            continue;
        }
        Check(Near(weightSum, 1.0f), "Arrow vertices must follow bones with weights that add up to 1.");
        const std::uint8_t expectedBone = vertex.position.z >= 1.0f ? 1 : 0;
        Check(vertex.joints[0] == expectedBone && Near(vertex.weights[0], 1.0f),
              "Arrow vertices must follow the root below z=1 and the tip from z=1.");
        ++skinned;
    }
    Check(skinned == 72 && unskinned == 24, "72 arrow vertices follow bones and 24 base vertices do not.");
}

// 2. 기본 자세: 한 번도 재생하지 않은 Animator의 스키닝 행렬은 단위 행렬이다. 그래서 4-1과 같은 모양으로 그려진다.
void TestBindPose(const ModelData& model)
{
    TestWorld world(0.1);
    GameObject& object = world.scene.CreateObject("bind");
    const Animator& animator = AddAnimator(object, model);
    Check(animator.SkinMatrices().size() == 2 && animator.BoneWorldMatrices().size() == 2,
          "The Animator must have one matrix per bone.");
    Check(IsIdentity(animator.SkinMatrices()[0]) && IsIdentity(animator.SkinMatrices()[1]),
          "In the bind pose every skin matrix must be the identity.");
    Check(!animator.IsPlaying() && animator.ClipName().empty() && !animator.LastTick().played,
          "A new Animator is not playing anything.");
    // tick을 돌려도 재생 요청이 없으면 그대로다.
    RunTick(world.scene, world.input, world.time);
    Check(IsIdentity(animator.SkinMatrices()[1]) && !animator.LastTick().played,
          "Without a Play request a tick changes nothing.");
}

// 3. 좌표 변환: 기대값은 glTF 공간에서 손으로 계산한 뒤 X를 뒤집은 값이다.
// 화살촉 정점 glTF (0.25, 0.05, 1.4)는 엔진에서 (-0.25, 0.05, 1.4)이고 tip을 따른다.
void TestConversion(const ModelData& model)
{
    TestWorld world(0.1);
    GameObject& object = world.scene.CreateObject("convert");
    Animator& animator = AddAnimator(object, model);
    const MeshVertex& vertex = FindVertex(model.mesh, -0.25f, 0.05f, 1.4f);
    Check(vertex.joints[0] == 1, "The arrowhead vertex must follow the tip.");

    // "swing" 0.5초: glTF에서 tip이 Y축으로 +90도 돌고 root가 x로 +0.2 움직인다.
    // tip 기준 (0.25, 0.05, 0.4)는 Y축 회전으로 (0.4, 0.05, -0.25)가 되고, tip 위치 (0,0,1)과 root 이동을 더해 (0.6, 0.05, 0.75)다.
    // X를 뒤집으면 (-0.6, 0.05, 0.75)다. 변환을 빠뜨리면 x가 +0.6으로 반대쪽에 간다.
    animator.Play("swing", false);
    for (int i = 0; i < 5; ++i)
        RunTick(world.scene, world.input, world.time);
    Check(Near(animator.PlaybackTime(), 0.5), "Five ticks of 0.1 s must reach 0.5 s.");
    DirectX::XMFLOAT3 moved = Apply(animator.SkinMatrices()[1], vertex.position);
    Check(Near(moved.x, -0.6f) && Near(moved.y, 0.05f) && Near(moved.z, 0.75f),
          "swing at 0.5 s must move the arrowhead vertex to (-0.6, 0.05, 0.75).");
    // root의 모델 공간 위치도 엔진 좌표로 -0.2다.
    const DirectX::XMFLOAT3 rootPosition = Apply(animator.BoneWorldMatrices()[0], {0.0f, 0.0f, 0.0f});
    Check(Near(rootPosition.x, -0.2f), "The root bone must move to x=-0.2 in engine space.");

    // "bend" 0.5초: tip이 X축으로 +90도. (0.25, 0.05, 0.4) → (0.25, -0.4, 0.05) → +tip → (0.25, -0.4, 1.05) → 뒤집기 → (-0.25, -0.4, 1.05).
    animator.Play("bend", false);
    for (int i = 0; i < 5; ++i)
        RunTick(world.scene, world.input, world.time);
    moved = Apply(animator.SkinMatrices()[1], vertex.position);
    Check(Near(moved.x, -0.25f) && Near(moved.y, -0.4f) && Near(moved.z, 1.05f),
          "bend at 0.5 s must move the arrowhead vertex to (-0.25, -0.4, 1.05).");
    // root는 bend에서 움직이지 않는다. root를 따르는 정점은 그대로다.
    Check(IsIdentity(animator.SkinMatrices()[0]), "bend must leave the root bone alone.");
}

// 4. 연결: Space 누름 → PlayerControl이 Play 요청 → 같은 tick의 애니메이션 단계에서 시작.
void TestInputConnection(const ModelData& model)
{
    TestWorld world(0.1);
    GameObject& player = world.scene.CreateObject("Player");
    Animator& animator = AddAnimator(player, model);
    player.AddComponent(std::make_unique<PlayerControl>(player, 1.0f, 1000, &animator, "bend"));

    // tick 1: 누름 없음. 재생도 없다.
    RunTick(world.scene, world.input, world.time);
    Check(!animator.LastTick().played, "Without an attack nothing plays.");

    // tick 2: Space를 눌렀다 뗀 짧은 입력. 누른 tick에 바로 시작한다.
    world.input.OnKeyDown(Key::Space);
    world.input.OnKeyUp(Key::Space);
    RunTick(world.scene, world.input, world.time);
    const TickRecord& first = animator.LastTick();
    Check(first.played && first.started && first.clip == model.FindClip("bend"),
          "The attack tick must start the bend clip.");
    Check(Near(first.start, 0.0) && Near(first.end, 0.1), "The start tick plays from 0 to 0.1 s.");
    Check(first.wrapCount == 0 && !first.finished, "The start tick neither wraps nor finishes.");
    Check(animator.IsPlaying() && animator.ClipName() == "bend" && !animator.IsLooping(),
          "After the attack the Animator plays bend once.");

    // tick 3: 이어서 진행. started는 한 tick만 참이다.
    RunTick(world.scene, world.input, world.time);
    const TickRecord& second = animator.LastTick();
    Check(second.played && !second.started && Near(second.start, 0.1) && Near(second.end, 0.2),
          "The next tick continues from 0.1 to 0.2 s without the started flag.");
}

// 5. 반복·되감김: 1초 클립을 0.1초 tick으로 돌리면 10번째 tick에 되감긴다.
// 0.1을 열 번 더하면 1.0보다 아주 조금 작을 수 있다. Animator가 끝 판정에 작은 여유를 두므로 11번째로 밀리지 않아야 한다.
void TestLoopWrap(const ModelData& model)
{
    TestWorld world(0.1);
    GameObject& object = world.scene.CreateObject("loop");
    Animator& animator = AddAnimator(object, model);
    animator.Play("bend", true);
    for (int i = 0; i < 9; ++i)
    {
        RunTick(world.scene, world.input, world.time);
        Check(animator.LastTick().wrapCount == 0, "The first nine ticks do not wrap.");
    }
    RunTick(world.scene, world.input, world.time);
    const TickRecord& wrap = animator.LastTick();
    Check(wrap.played && wrap.wrapCount == 1 && !wrap.finished, "The tenth tick wraps around once.");
    Check(Near(wrap.start, 0.9) && Near(wrap.end, 0.0), "The wrapping tick runs from 0.9 to the end and lands on 0.");
    Check(animator.IsPlaying(), "A looping clip keeps playing after it wraps.");
    RunTick(world.scene, world.input, world.time);
    const TickRecord& after = animator.LastTick();
    Check(after.wrapCount == 0 && Near(after.start, 0.0) && Near(after.end, 0.1), "After the wrap the clip runs from 0 again.");

    // 재생 중 Play: 섞지 않고 새 클립이 0초부터 시작한다. Stop: 그다음 tick부터 진행이 없고 자세는 그대로다.
    animator.Play("swing", false);
    RunTick(world.scene, world.input, world.time);
    Check(animator.LastTick().started && Near(animator.LastTick().start, 0.0) && animator.ClipName() == "swing" &&
              !animator.IsLooping(),
          "Play during playback switches to the new clip from 0.");
    const DirectX::XMFLOAT4X4 poseBeforeStop = animator.SkinMatrices()[1];
    animator.Stop();
    RunTick(world.scene, world.input, world.time);
    Check(!animator.IsPlaying() && !animator.LastTick().played && Near(animator.PlaybackTime(), 0.1),
          "Stop keeps the playback time and plays nothing afterwards.");
    Check(Near(animator.SkinMatrices()[1]._41, poseBeforeStop._41) && Near(animator.SkinMatrices()[1]._43, poseBeforeStop._43),
          "Stop keeps the pose.");
}

// 6. 끝: 반복하지 않는 클립은 끝에 닿은 tick에 finished가 한 번 서고, 그 뒤로는 진행하지 않으며 자세가 그대로다.
void TestFinish(const ModelData& model)
{
    TestWorld world(0.25);
    GameObject& object = world.scene.CreateObject("once");
    Animator& animator = AddAnimator(object, model);
    animator.Play("bend", false);
    for (int i = 0; i < 3; ++i)
        RunTick(world.scene, world.input, world.time);
    Check(animator.IsPlaying() && !animator.LastTick().finished, "Three ticks of 0.25 s do not reach the end.");
    RunTick(world.scene, world.input, world.time);
    const TickRecord& last = animator.LastTick();
    Check(last.played && last.finished && last.wrapCount == 0 && Near(last.start, 0.75) && Near(last.end, 1.0),
          "The fourth tick reaches the end and sets finished.");
    Check(!animator.IsPlaying() && Near(animator.PlaybackTime(), 1.0), "A finished clip stops at its end.");
    const DirectX::XMFLOAT4X4 heldPose = animator.SkinMatrices()[1];

    RunTick(world.scene, world.input, world.time);
    Check(!animator.LastTick().played && !animator.LastTick().finished, "After finishing, ticks play nothing.");
    Check(IsIdentity(animator.SkinMatrices()[1]) == IsIdentity(heldPose) &&
              Near(animator.SkinMatrices()[1]._43, heldPose._43) && Near(animator.SkinMatrices()[1]._42, heldPose._42),
          "The pose stays at the last key.");
    // bend의 마지막 키는 0도라 끝 자세가 기본 자세다.
    Check(IsIdentity(heldPose), "bend ends in the bind pose.");

    // 다시 Play하면 처음부터다.
    animator.Play("bend", false);
    RunTick(world.scene, world.input, world.time);
    Check(animator.LastTick().started && Near(animator.LastTick().start, 0.0), "Play after a finish restarts from 0.");
}

// 7~9. 정지·배속·긴 프레임: Animator는 정지·배속을 모른다(학생 질문 2).
void TestPauseScaleLongFrame(const ModelData& model)
{
    TestWorld world(0.1);
    GameObject& object = world.scene.CreateObject("clock");
    Animator& animator = AddAnimator(object, model);
    animator.Play("bend", true);

    // 배속 1: 실제 0.2초 → 0.2초 진행.
    int ticks = RunFrame(world.scene, world.input, world.time, 0.2);
    Check(ticks == 2 && Near(animator.PlaybackTime(), 0.2), "At scale 1 a 0.2 s frame advances the clip by 0.2 s.");

    // 배속 2: 같은 실제 0.2초 → 0.4초 진행.
    world.time.RequestScale(2.0);
    ticks = RunFrame(world.scene, world.input, world.time, 0.2);
    Check(ticks == 4 && Near(animator.PlaybackTime(), 0.6), "At scale 2 the same frame advances the clip twice as far.");

    // 정지: 재생 시각이 그대로다. 마지막 기록도 그 전 것 그대로다.
    world.time.RequestPause(true);
    const TickRecord beforePause = animator.LastTick();
    ticks = RunFrame(world.scene, world.input, world.time, 0.2);
    Check(ticks == 0 && Near(animator.PlaybackTime(), 0.6) && Near(animator.LastTick().end, beforePause.end),
          "While paused the clip does not advance.");

    // 긴 프레임: 실제 1초지만 tick 상한 5번만 돌고 나머지 0.5초는 Time이 버린다. 0.6 + 0.5 = 1.1 → 되감겨 0.1.
    world.time.RequestPause(false);
    world.time.RequestScale(1.0);
    ticks = RunFrame(world.scene, world.input, world.time, 1.0);
    Check(ticks == 5 && Near(animator.PlaybackTime(), 0.1) && Near(world.time.DroppedTime(), 0.5),
          "A long frame advances the clip by the tick cap only and drops the rest.");
}

// 10. 독립 재생: 같은 원본을 쓰는 두 Animator의 재생 시각이 따로 간다.
void TestIndependentPlayback(const ModelData& model)
{
    TestWorld world(0.1);
    GameObject& a = world.scene.CreateObject("A");
    GameObject& b = world.scene.CreateObject("B");
    Animator& animatorA = AddAnimator(a, model);
    Animator& animatorB = AddAnimator(b, model);
    Check(&animatorA.Model() == &animatorB.Model(), "Both Animators point at the same model source.");

    animatorA.Play("bend", true);
    RunTick(world.scene, world.input, world.time);
    RunTick(world.scene, world.input, world.time);
    animatorB.Play("swing", true);
    RunTick(world.scene, world.input, world.time);
    RunTick(world.scene, world.input, world.time);
    Check(Near(animatorA.PlaybackTime(), 0.4) && Near(animatorB.PlaybackTime(), 0.2),
          "Each Animator keeps its own playback time.");
    Check(animatorA.ClipName() == "bend" && animatorB.ClipName() == "swing", "Each Animator keeps its own clip.");
    // 둘의 자세도 다르다: A는 bend 0.4초(tip이 X축으로 꺾임), B는 swing 0.2초(tip이 Y축으로 돎).
    Check(!Near(animatorA.SkinMatrices()[1]._42, animatorB.SkinMatrices()[1]._42) ||
              !Near(animatorA.SkinMatrices()[1]._41, animatorB.SkinMatrices()[1]._41),
          "Different clips give different poses.");

    // A를 지워도 B는 계속 재생한다. 원본은 AssetManager 것이라 그대로다.
    a.RequestDestroy();
    RunTick(world.scene, world.input, world.time);
    Check(world.scene.ObjectCount() == 1 && Near(animatorB.PlaybackTime(), 0.3), "B keeps playing after A is destroyed.");
}

// 시험 모델의 JSON 글자를 바꾼 사본을 임시 폴더에 만든다. 로더의 다른 경로를 작은 변형으로 시험하기 위해서다.
std::string ReadTestModelText(const std::string& root)
{
    std::ifstream source(std::filesystem::path(root) / TestModelPath);
    return std::string((std::istreambuf_iterator<char>(source)), std::istreambuf_iterator<char>());
}

void ReplaceOnce(std::string& text, const char* from, const char* to)
{
    const std::string::size_type at = text.find(from);
    Check(at != std::string::npos, "The test model text must contain the piece the test replaces.");
    text.replace(at, std::string(from).size(), to);
}

void WriteText(const std::filesystem::path& path, const std::string& text)
{
    std::ofstream file(path);
    file << text;
}

// 11. 뼈 순서와 중간 노드: 파일의 관절 순서가 뒤집혀도 부모가 앞에 오게 다시 매기고 정점의 뼈 번호를 고친다.
// 관절 사이에 관절이 아닌 노드가 끼어 있으면 그 변환을 자식 관절에 합친다.
void TestJointOrderAndParents(const std::string& root)
{
    const TempFolder folder;
    AssetManager tempAssets(folder.path.string());

    // 관절 순서를 [tip, root]로 뒤집는다. 정점의 번호 0은 이제 파일에서 tip을 뜻한다.
    std::string reversed = ReadTestModelText(root);
    ReplaceOnce(reversed, "\"joints\": [\n    0,\n    1\n   ]", "\"joints\": [\n    1,\n    0\n   ]");
    WriteText(folder.path / "reversed.gltf", reversed);
    const ModelData& reversedModel = tempAssets.LoadModel("reversed.gltf");
    Check(reversedModel.bones[0].name == "root" && reversedModel.bones[1].name == "tip" && reversedModel.bones[1].parent == 0,
          "Reversed joints must still be stored parent first.");
    const MeshVertex& bodyVertex = FindVertex(reversedModel.mesh, -0.1f, -0.1f, 0.0f);
    Check(bodyVertex.joints[0] == 1, "A vertex that pointed at file joint 0 (now the tip) must point at engine bone 1.");

    // root와 tip 사이에 관절이 아닌 노드 mid(z로 0.5 이동)를 끼운다. tip의 자기 이동은 1 그대로이고, mid의 0.5는 parentOffset에 남는다.
    // 그래서 tip의 이동 키가 있는 클립을 재생해도 mid의 몫이 사라지지 않는다.
    std::string middle = ReadTestModelText(root);
    ReplaceOnce(middle, "\"children\": [\n    1\n   ]", "\"children\": [\n    4\n   ]");
    ReplaceOnce(middle, "  }\n ],\n \"meshes\"", "  },\n  {\"name\": \"mid\", \"translation\": [0, 0, 0.5], \"children\": [1]}\n ],\n \"meshes\"");
    WriteText(folder.path / "middle.gltf", middle);
    const ModelData& middleModel = tempAssets.LoadModel("middle.gltf");
    Check(middleModel.bones.size() == 2 && middleModel.bones[1].parent == 0, "A node between two joints does not become a bone.");
    Check(Near(middleModel.bones[1].translation.z, 1.0f) && Near(middleModel.bones[1].parentOffset._43, 0.5f),
          "The in-between node's translation is kept apart from the child bone's own translation.");
    Check(IsIdentity(middleModel.bones[0].parentOffset), "A bone whose parent is a joint has no offset.");

    // 기본 자세에서 tip의 모델 공간 위치는 1.5다. "bend"(회전 키만)를 재생해도 그대로다.
    TestWorld world(0.1);
    GameObject& object = world.scene.CreateObject("middle");
    Animator& animator = AddAnimator(object, middleModel);
    Check(Near(Apply(animator.BoneWorldMatrices()[1], {0.0f, 0.0f, 0.0f}).z, 1.5f), "The tip sits at z=1.5 with the in-between node.");
    animator.Play("bend", false);
    for (int i = 0; i < 5; ++i)
        RunTick(world.scene, world.input, world.time);
    Check(Near(Apply(animator.BoneWorldMatrices()[1], {0.0f, 0.0f, 0.0f}).z, 1.5f),
          "Playing a clip keeps the in-between node's part of the tip position.");
}

// 12. 오류: LINEAR가 아닌 보간은 읽지 않는다. 없는 클립 이름은 Play에서 바로 거부한다.
// (같은 시각의 키를 거부하는 검사는 이진 버퍼를 바꿔야 해서 여기서 시험하지 않는다.)
void TestErrors(const std::string& root, const ModelData& model)
{
    const TempFolder folder;
    std::string step = ReadTestModelText(root);
    ReplaceOnce(step, "\"LINEAR\"", "\"STEP\"");
    WriteText(folder.path / "step.gltf", step);
    AssetManager tempAssets(folder.path.string());
    std::string message;
    try { tempAssets.LoadModel("step.gltf"); } catch (const std::runtime_error& error) { message = error.what(); }
    Check(Contains(message, "step.gltf") && Contains(message, "unsupported data") && Contains(message, "LINEAR"),
          "STEP interpolation must be reported as unsupported data.");

    TestWorld world(0.1);
    GameObject& object = world.scene.CreateObject("errors");
    Animator& animator = AddAnimator(object, model);
    message.clear();
    try { animator.Play("walk", true); } catch (const std::invalid_argument& error) { message = error.what(); }
    Check(Contains(message, "walk") && Contains(message, TestModelPath), "An unknown clip name must be rejected with the model path.");
    message.clear();
    try { PlayerControl control(object, 1.0f, 1, &animator, "walk"); } catch (const std::invalid_argument& error) { message = error.what(); }
    Check(Contains(message, "walk"), "PlayerControl must reject an attack clip the model does not have.");
}
}

int RunAnimationTests(const std::string& rootFolder)
{
    try
    {
        AssetManager assets(rootFolder);
        const ModelData& model = assets.LoadModel(TestModelPath);
        TestLoadSkeleton(model);
        TestBindPose(model);
        TestConversion(model);
        TestInputConnection(model);
        TestLoopWrap(model);
        TestFinish(model);
        TestPauseScaleLongFrame(model);
        TestIndependentPlayback(model);
        TestJointOrderAndParents(rootFolder);
        TestErrors(rootFolder, model);
        std::cout << "Animation tests PASS (10 cases)\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "Animation tests FAIL: " << error.what() << '\n';
        return 1;
    }
}
