#include "Scene.h"
#include <stdexcept>
#include <utility>

Scene::Scene(const Input& input, const Time& time) : m_input(input), m_time(time)
{
}

Scene::~Scene()
{
    Clear();
}

GameObject& Scene::CreateObject(const std::string& name)
{
    // 새 GameObject에 자기 자신을 넘긴다. 컴포넌트는 이 참조를 거쳐 Input과 Time을 읽는다.
    auto object = std::make_unique<GameObject>(*this, name);
    GameObject& result = *object;
    // tick 안이거나 갱신 도중이면 목록을 바로 바꾸지 않는다. 반영 시점까지 대기 목록에 둔다.
    // unique_ptr이 가리키는 객체는 벡터 사이를 옮겨도 주소가 그대로라, 돌려준 참조는 계속 유효하다.
    if (m_inTick || m_updating)
        m_pendingObjects.push_back(std::move(object));
    else
        m_objects.push_back(std::move(object));
    return result;
}

void Scene::BeginTick()
{
    if (m_inTick)
        throw std::logic_error("Scene cannot begin a tick while another tick is running.");
    m_inTick = true;
}

void Scene::EndTick()
{
    if (!m_inTick)
        throw std::logic_error("Scene cannot end a tick that was not begun.");
    m_inTick = false;
    // tick 안의 모든 단계가 끝났다. 그동안 미뤄 둔 생성·삭제를 여기서 한 번에 반영한다.
    ApplyPendingChanges();
}

void Scene::FixedUpdate()
{
    RunObjects(true);
}

void Scene::Update()
{
    RunObjects(false);
}

void Scene::RunObjects(bool fixedStep)
{
    // FixedUpdate 도중 Update를 부르는 것처럼, 갱신 도중에는 어느 갱신도 다시 시작할 수 없다.
    if (m_updating)
        throw std::logic_error("Scene cannot start an update while another update is running.");

    m_updating = true;
    // 순회 중 생성해도 현재 호출 대상 수는 바뀌지 않는다. 새 객체는 대기 목록에 들어간다.
    const std::size_t count = m_objects.size();
    try
    {
        for (std::size_t i = 0; i < count; ++i)
        {
            if (fixedStep)
                m_objects[i]->FixedUpdate();
            else
                m_objects[i]->Update();
        }
    }
    catch (...)
    {
        // 갱신이 실패해도 Scene을 다시 쓸 수 있는 상태로 둔다. 요청은 버리지 않는다.
        // tick 밖이면 지금 반영하고, tick 안이면 반영 경계는 EndTick 하나이므로 그때까지 기다린다.
        m_updating = false;
        if (!m_inTick)
            ApplyPendingChanges();
        throw;
    }
    m_updating = false;
    // tick 안이면 EndTick이 반영한다. tick 밖(Update, 시험 코드)에서는 갱신이 끝난 지금 반영한다.
    if (!m_inTick)
        ApplyPendingChanges();
}

void Scene::SavePreviousTransforms()
{
    // 비활성 객체도 복사한다. 꺼져 있는 동안 옮겨진 뒤 다시 켜져도 끌려오지 않게 하기 위해서다.
    for (const auto& object : m_objects)
        object->SavePreviousTransform();
}

const GameObject& Scene::ObjectAt(std::size_t index) const
{
    // at은 범위를 벗어나면 std::out_of_range 예외를 던진다.
    return *m_objects.at(index);
}

void Scene::ApplyPendingChanges()
{
    // 1. 대기 목록의 객체를 객체 목록 뒤에 붙인다. 기다리는 사이에 삭제가 요청된 객체는 넣지 않고 그대로 지운다.
    for (auto& object : m_pendingObjects)
    {
        if (!object->IsDestroyRequested())
            m_objects.push_back(std::move(object));
    }
    m_pendingObjects.clear();

    // 2. 삭제 요청된 객체를 목록에서 지운다. FixedUpdate나 Update가 모두 반환한 뒤에만 소유권을 제거한다.
    std::size_t i = 0;
    while (i < m_objects.size())
    {
        if (m_objects[i]->IsDestroyRequested())
            m_objects.erase(m_objects.begin() + i);
        else
            ++i;
    }
}

void Scene::Clear()
{
    for (const auto& object : m_objects)
        object->RequestDestroy();
    for (const auto& object : m_pendingObjects)
        object->RequestDestroy();

    // tick 안이거나 Component의 FixedUpdate·Update 안에서 Clear를 요청해도 정리는 뒤로 미룬다.
    if (!m_updating && !m_inTick)
        ApplyPendingChanges();
}
