#pragma once
#include "GameObject.h"
#include <memory>
#include <string>
#include <vector>

class Scene
{
public:
    ~Scene();
    GameObject& CreateObject(const std::string& name);
    void Update(int frame);
    void Clear();
    std::size_t ObjectCount() const { return m_objects.size(); }

private:
    void RemoveDestroyedObjects();

    bool m_updating = false;
    std::vector<std::unique_ptr<GameObject>> m_objects;
};
