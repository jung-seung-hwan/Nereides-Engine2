#include "RenderTests.h"
#include "CameraComponent.h"
#include "CollectRenderFrame.h"
#include "Input.h"
#include "ModelRenderer.h"
#include "RenderFrame.h"
#include "Scene.h"
#include "Time.h"
#include <cmath>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <utility>

// 렌더 데이터 검증. Window와 Renderer 없이 돈다. 그래서 이 파일은 Renderer.h를 포함하지 않는다.
namespace
{
const DirectX::XMFLOAT4 Red = {1.0f, 0.0f, 0.0f, 1.0f};
const DirectX::XMFLOAT4 Gray = {0.5f, 0.5f, 0.5f, 1.0f};

// 시험용 장면. 단계 2 규칙대로 Input과 Time을 Scene보다 먼저 선언한다.
// 메시 원본은 시험 함수에서 TestWorld보다 먼저 선언해 Scene보다 오래 살게 한다.
struct TestWorld
{
    Input input;
    Time time{0.1, 3};
    Scene scene{input, time};
};

void Check(bool condition, const char* message)
{
    if (!condition)
        throw std::runtime_error(message);
}

bool Near(double a, double b)
{
    return std::fabs(a - b) < 1e-4;
}

// 카메라 객체를 만들고 CameraComponent를 붙인다. 컴포넌트를 넘기기 전에 주소를 기억해 돌려준다.
const CameraComponent& AddCamera(Scene& scene, const Vector3& position, float yaw)
{
    GameObject& object = scene.CreateObject("Camera");
    object.transform.position = position;
    object.transform.rotation.y = yaw;
    auto camera = std::make_unique<CameraComponent>(object, 60.0f, 0.1f, 100.0f);
    const CameraComponent& result = *camera;
    object.AddComponent(std::move(camera));
    return result;
}

void AddModel(GameObject& object, const MeshData& mesh, const DirectX::XMFLOAT4& color)
{
    object.AddComponent(std::make_unique<ModelRenderer>(object, mesh, color));
}

// 월드의 점 하나에 view와 projection을 곱한다. x/w와 y/w는 화면 좌표(-1~1, 가운데가 0), z/w는 깊이(0~1)다.
DirectX::XMFLOAT4 ToClip(const RenderFrame& frame, float x, float y, float z)
{
    const DirectX::XMMATRIX viewProjection =
        DirectX::XMLoadFloat4x4(&frame.view) * DirectX::XMLoadFloat4x4(&frame.projection);
    DirectX::XMFLOAT4 clip;
    DirectX::XMStoreFloat4(&clip, DirectX::XMVector4Transform(DirectX::XMVectorSet(x, y, z, 1.0f), viewProjection));
    return clip;
}

void TestInterpolationAlpha()
{
    Time time(0.1, 3);
    Check(time.BeginFrame(0.15) == 1, "0.15 seconds must run one tick of 0.1 seconds.");
    time.FinishTick();
    Check(Near(time.InterpolationAlpha(), 0.5), "Half a tick must remain after 0.15 seconds.");

    time.RequestPause(true);
    Check(time.BeginFrame(0.3) == 0, "A paused frame must not run a tick.");
    Check(Near(time.InterpolationAlpha(), 0.5), "Pause must keep the alpha.");

    // 남은 0.05초에 1초가 더해지면 tick 10번이 필요하다. 상한 3번만 돌고 나머지는 버린다.
    time.RequestPause(false);
    const int ticks = time.BeginFrame(1.0);
    for (int i = 0; i < ticks; ++i)
        time.FinishTick();
    Check(ticks == 3 && Near(time.InterpolationAlpha(), 0.0),
          "A frame over the catch-up limit must drop the rest and give alpha 0.");
}

void TestPreviousTransformCopy()
{
    TestWorld world;
    GameObject& active = world.scene.CreateObject("Active");
    GameObject& sleeping = world.scene.CreateObject("Sleeping");
    sleeping.SetActive(false);
    Check(!active.HasPreviousTransform(), "A new object must not have a previous transform.");

    sleeping.transform.position.x = 4.0f;
    world.scene.SavePreviousTransforms();
    Check(active.HasPreviousTransform() && sleeping.HasPreviousTransform(),
          "Tick start must copy every object, inactive ones too.");

    // 꺼져 있는 동안 누가 옮겨도, 직전 값은 복사한 때의 값이다.
    sleeping.transform.position.x = 6.0f;
    Check(Near(sleeping.InterpolatedTransform(0.5f).position.x, 5.0),
          "Interpolation must start from the copied transform.");

    GameObject& added = world.scene.CreateObject("Added");
    Check(!added.HasPreviousTransform(), "An object created after the copy must wait for the next tick.");
}

void TestInterpolatedItem()
{
    const MeshData box = MakeBox();
    TestWorld world;
    const CameraComponent& camera = AddCamera(world.scene, {0.0f, 0.0f, -5.0f}, 0.0f);
    GameObject& player = world.scene.CreateObject("Player");
    AddModel(player, box, Red);

    // 직전 값이 없으면 현재 값을 그대로 그린다.
    player.transform.position.x = 1.0f;
    RenderFrame frame = CollectRenderFrame(world.scene, camera, 1.0f, 0.5f);
    Check(frame.items.size() == 1 && Near(frame.items[0].world._41, 1.0),
          "Without a previous transform the current position must be drawn.");

    // 직전 x=0, 현재 x=1, 계수 0.5이면 가운데인 0.5에 그린다. 계수 0이면 직전 위치다.
    player.transform.position.x = 0.0f;
    world.scene.SavePreviousTransforms();
    player.transform.position.x = 1.0f;
    frame = CollectRenderFrame(world.scene, camera, 1.0f, 0.5f);
    Check(Near(frame.items[0].world._41, 0.5), "Alpha 0.5 must draw halfway between the previous and current tick.");
    frame = CollectRenderFrame(world.scene, camera, 1.0f, 0.0f);
    Check(Near(frame.items[0].world._41, 0.0), "Alpha 0 must draw the previous tick.");
    frame = CollectRenderFrame(world.scene, camera, 1.0f, 1.0f);
    Check(Near(frame.items[0].world._41, 1.0), "Alpha 1 must draw the current tick.");

    // 회전과 크기도 같은 계수로 섞는다. 직전 회전 0·크기 1, 현재 회전 1·크기 3이면 계수 0.5에서 회전 0.5·크기 2다.
    world.scene.SavePreviousTransforms();
    player.transform.rotation.y = 1.0f;
    player.transform.scale = {3.0f, 3.0f, 3.0f};
    frame = CollectRenderFrame(world.scene, camera, 1.0f, 0.5f);
    // 크기 2와 y축 회전 0.5를 곱한 행렬의 첫 행은 (2cos0.5, 0, -2sin0.5), 둘째 행은 (0, 2, 0)이다.
    const DirectX::XMFLOAT4X4& matrix = frame.items[0].world;
    Check(Near(matrix._11, 2.0 * std::cos(0.5)) && Near(matrix._13, -2.0 * std::sin(0.5)) && Near(matrix._22, 2.0),
          "Rotation and scale must be interpolated with the same alpha.");
}

void TestCollectTargets()
{
    const MeshData box = MakeBox();
    TestWorld world;
    const CameraComponent& camera = AddCamera(world.scene, {0.0f, 0.0f, -5.0f}, 0.0f);

    GameObject& shown = world.scene.CreateObject("Shown");
    AddModel(shown, box, Red);
    shown.transform.position.x = 7.0f;
    GameObject& hidden = world.scene.CreateObject("Hidden");
    AddModel(hidden, box, Red);
    hidden.SetActive(false);
    GameObject& switchedOff = world.scene.CreateObject("SwitchedOff");
    auto renderer = std::make_unique<ModelRenderer>(switchedOff, box, Red);
    renderer->SetEnabled(false);
    switchedOff.AddComponent(std::move(renderer));
    world.scene.CreateObject("Empty"); // ModelRenderer가 없다
    GameObject& leaving = world.scene.CreateObject("Leaving");
    AddModel(leaving, box, Red);
    leaving.RequestDestroy(); // 삭제를 기다린다. 다음 Scene.FixedUpdate나 Scene.Update가 끝날 때 지워진다

    const RenderFrame frame = CollectRenderFrame(world.scene, camera, 1.0f, 0.0f);
    Check(frame.items.size() == 1 && Near(frame.items[0].world._41, 7.0),
          "Only an enabled ModelRenderer on an active object that is not being destroyed must be drawn.");
}

void TestItemContents()
{
    const MeshData box = MakeBox();
    const MeshData floor = MakeFloor();
    TestWorld world;
    const CameraComponent& camera = AddCamera(world.scene, {0.0f, 0.0f, -5.0f}, 0.0f);
    GameObject& first = world.scene.CreateObject("First");
    AddModel(first, box, Red);
    GameObject& second = world.scene.CreateObject("Second");
    AddModel(second, box, Red);
    GameObject& ground = world.scene.CreateObject("Ground");
    AddModel(ground, floor, Gray);

    const RenderFrame frame = CollectRenderFrame(world.scene, camera, 1.0f, 0.0f);
    Check(frame.items.size() == 3, "Each ModelRenderer must add one item.");
    // 메시는 복사하지 않고 원본 주소만 담는다. 같은 원본을 쓰는 객체는 같은 주소를 가리킨다.
    Check(frame.items[0].mesh == &box && frame.items[1].mesh == &box && frame.items[2].mesh == &floor,
          "Items must point at the shared mesh source without copying it.");
    const DirectX::XMFLOAT4& color = frame.items[2].color;
    Check(Near(color.x, 0.5) && Near(color.y, 0.5) && Near(color.z, 0.5) && Near(color.w, 1.0),
          "An item must carry the ModelRenderer color.");
}

void TestWorldMatrixOrder()
{
    Transform transform;
    transform.scale = {2.0f, 2.0f, 2.0f};
    transform.rotation.y = DirectX::XM_PIDIV2;
    transform.position = {10.0f, 0.0f, 0.0f};

    // 점 (0,0,1): 크기 2배 → (0,0,2), y축으로 90도 → (2,0,0), 이동 → (12,0,0)
    DirectX::XMFLOAT3 point;
    DirectX::XMStoreFloat3(&point, DirectX::XMVector3TransformCoord(DirectX::XMVectorSet(0.0f, 0.0f, 1.0f, 1.0f),
                                                                     WorldMatrix(transform)));
    Check(Near(point.x, 12.0) && Near(point.y, 0.0) && Near(point.z, 0.0),
          "The world matrix must scale, then rotate, then translate.");
}

void TestCameraProjection()
{
    TestWorld world;
    const CameraComponent& camera = AddCamera(world.scene, {0.0f, 2.0f, -5.0f}, 0.0f);
    const RenderFrame frame = CollectRenderFrame(world.scene, camera, 1.0f, 0.0f);

    // 카메라 정면으로 10만큼 앞의 점은 화면 가운데에 놓이고, 깊이는 0과 1 사이다.
    const DirectX::XMFLOAT4 ahead = ToClip(frame, 0.0f, 2.0f, 5.0f);
    Check(ahead.w > 0.0f && Near(ahead.x / ahead.w, 0.0) && Near(ahead.y / ahead.w, 0.0),
          "A point straight ahead must land at the screen center.");
    Check(ahead.z / ahead.w > 0.0f && ahead.z / ahead.w < 1.0f, "Depth must be between 0 and 1.");

    // 오른쪽(+X)과 위(+Y)에 있는 점은 화면의 오른쪽과 위에 놓인다.
    const DirectX::XMFLOAT4 rightUp = ToClip(frame, 1.0f, 3.0f, 5.0f);
    Check(rightUp.x / rightUp.w > 0.0f && rightUp.y / rightUp.w > 0.0f, "+X and +Y must appear right and up.");

    // 화면 비율이 두 배로 넓어지면 가로 배율이 절반이 된다.
    const RenderFrame wide = CollectRenderFrame(world.scene, camera, 2.0f, 0.0f);
    Check(Near(wide.projection._11 * 2.0f, frame.projection._11), "A wider screen must halve the horizontal scale.");

    // 카메라를 오른쪽으로 90도 돌리면 +X 방향이 정면이 된다.
    TestWorld turnedWorld;
    const CameraComponent& turned = AddCamera(turnedWorld.scene, {0.0f, 2.0f, -5.0f}, DirectX::XM_PIDIV2);
    const RenderFrame turnedFrame = CollectRenderFrame(turnedWorld.scene, turned, 1.0f, 0.0f);
    const DirectX::XMFLOAT4 side = ToClip(turnedFrame, 10.0f, 2.0f, -5.0f);
    Check(side.w > 0.0f && Near(side.x / side.w, 0.0) && Near(side.y / side.w, 0.0),
          "A turned camera must look along its rotated +Z.");

    // 카메라도 다른 객체처럼 보간한다. 직전 z=-5, 현재 z=-3이면 계수 0.5에서 z=-4에 있는 카메라로 본다.
    // 회전이 없는 view 행렬의 마지막 행은 카메라 위치의 반대 방향 이동이라 _43이 4가 된다.
    TestWorld movingWorld;
    const CameraComponent& moving = AddCamera(movingWorld.scene, {0.0f, 0.0f, -5.0f}, 0.0f);
    movingWorld.scene.SavePreviousTransforms();
    moving.GetOwner().transform.position.z = -3.0f;
    const RenderFrame movingFrame = CollectRenderFrame(movingWorld.scene, moving, 1.0f, 0.5f);
    Check(Near(movingFrame.view._43, 4.0), "The camera must be interpolated like other objects.");
}

void TestMeshWinding()
{
    const MeshData box = MakeBox();
    const MeshData floor = MakeFloor();
    Check(box.vertices.size() == 24 && box.indices.size() == 36, "A box must have 24 vertices and 36 indices.");
    Check(floor.vertices.size() == 4 && floor.indices.size() == 6, "A floor must have 4 vertices and 6 indices.");

    // 바깥에서 볼 때 시계 방향이 앞면이다. 이 순서면 삼각형 (a, b, c)의 (b-a)×(c-a)가 바깥, 곧 법선 쪽을 향한다.
    const MeshData* meshes[] = {&box, &floor};
    for (const MeshData* mesh : meshes)
    {
        for (std::size_t i = 0; i + 2 < mesh->indices.size(); i += 3)
        {
            const MeshVertex& a = mesh->vertices[mesh->indices[i]];
            const MeshVertex& b = mesh->vertices[mesh->indices[i + 1]];
            const MeshVertex& c = mesh->vertices[mesh->indices[i + 2]];
            const DirectX::XMVECTOR pa = DirectX::XMLoadFloat3(&a.position);
            const DirectX::XMVECTOR pb = DirectX::XMLoadFloat3(&b.position);
            const DirectX::XMVECTOR pc = DirectX::XMLoadFloat3(&c.position);
            const DirectX::XMVECTOR face = DirectX::XMVector3Cross(DirectX::XMVectorSubtract(pb, pa),
                                                                   DirectX::XMVectorSubtract(pc, pa));
            const float facing = DirectX::XMVectorGetX(DirectX::XMVector3Dot(face, DirectX::XMLoadFloat3(&a.normal)));
            Check(facing > 0.0f, "Every triangle must be clockwise when seen from outside.");
        }
    }
}

void TestInvalidInputs()
{
    const MeshData empty{};
    MeshData verticesOnly = MakeBox();
    verticesOnly.indices.clear();
    MeshData indicesOnly = MakeBox();
    indicesOnly.vertices.clear();
    TestWorld world;
    GameObject& object = world.scene.CreateObject("Target");
    int rejected = 0;
    try { CameraComponent(object, 0.0f, 0.1f, 100.0f); } catch (const std::invalid_argument&) { ++rejected; }
    try { CameraComponent(object, 180.0f, 0.1f, 100.0f); } catch (const std::invalid_argument&) { ++rejected; }
    try { CameraComponent(object, 60.0f, 0.0f, 100.0f); } catch (const std::invalid_argument&) { ++rejected; }
    try { CameraComponent(object, 60.0f, 10.0f, 10.0f); } catch (const std::invalid_argument&) { ++rejected; }
    try { ModelRenderer(object, empty, Red); } catch (const std::invalid_argument&) { ++rejected; }
    try { ModelRenderer(object, verticesOnly, Red); } catch (const std::invalid_argument&) { ++rejected; }
    try { ModelRenderer(object, indicesOnly, Red); } catch (const std::invalid_argument&) { ++rejected; }

    const CameraComponent& camera = AddCamera(world.scene, {0.0f, 0.0f, -5.0f}, 0.0f);
    try { CollectRenderFrame(world.scene, camera, 0.0f, 0.0f); } catch (const std::invalid_argument&) { ++rejected; }
    try { CollectRenderFrame(world.scene, camera, 1.0f, -0.1f); } catch (const std::invalid_argument&) { ++rejected; }
    try { CollectRenderFrame(world.scene, camera, 1.0f, 1.1f); } catch (const std::invalid_argument&) { ++rejected; }
    // NaN은 어떤 비교도 거짓이라, 범위 검사만으로는 걸러지지 않는다.
    try { CollectRenderFrame(world.scene, camera, 1.0f, std::nanf("")); } catch (const std::invalid_argument&) { ++rejected; }
    try { CameraComponent(object, std::nanf(""), 0.1f, 100.0f); } catch (const std::invalid_argument&) { ++rejected; }
    Check(rejected == 12, "Invalid camera, mesh, aspect or alpha values were accepted.");
}
}

int RunRenderTests()
{
    try
    {
        TestInterpolationAlpha();
        TestPreviousTransformCopy();
        TestInterpolatedItem();
        TestCollectTargets();
        TestItemContents();
        TestWorldMatrixOrder();
        TestCameraProjection();
        TestMeshWinding();
        TestInvalidInputs();
        std::cout << "Render tests PASS (9 cases)\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "Render tests FAIL: " << error.what() << '\n';
        return 1;
    }
}
