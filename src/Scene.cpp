#include "Scene.h"
#include <stdexcept>
#include <utility>

Scene::~Scene()
{
    Clear();
}

GameObject& Scene::CreateObject(const std::string& name)
{
    auto object = std::make_unique<GameObject>(name);
    GameObject& result = *object;
    m_objects.push_back(std::move(object));
    return result;
}

void Scene::Update()
{
    if (m_updating)
        throw std::logic_error("Scene::Update cannot call itself.");

    m_updating = true;
    // 순회 중 생성해도 현재 호출 대상 수는 바뀌지 않는다.
    const std::size_t count = m_objects.size();
    try
    {
        for (std::size_t i = 0; i < count; ++i)
            m_objects[i]->Update();
    }
    catch (...)
    {
        m_updating = false;
        RemoveDestroyedObjects();
        throw;
    }
    m_updating = false;
    RemoveDestroyedObjects();
}

void Scene::RemoveDestroyedObjects()
{
    // Update가 모두 반환한 뒤에만 소유권을 제거한다.
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

    // Component의 Update 안에서 Clear를 요청해도 정리는 뒤로 미룬다.
    if (!m_updating)
        RemoveDestroyedObjects();
}
