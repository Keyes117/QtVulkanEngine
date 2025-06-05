#pragma once

#include "ObjectManager.h"
#include "RenderManager.h"
#include "FrameInfo.h"

class SceneManager
{
public:
    SceneManager(MyVulkanWindow& window, Device& device, VkDescriptorSetLayout globalSetLayout, const AABB& worldBounds);
    ~SceneManager() = default;

    /*对象管理*/
    Object::ObjectID addObject(const Object::Builder& builder);
    //TODO: 更新和删除接口
    //void removeObject(Object::ObjectID id);
    //void updateObject(Object::ObjectID id,const UpdateFunc& updateFunc);

    void render(FrameInfo& frameInfo);

    ObjectManager& getOBjectManager() { return m_objectManager; }
    RenderManager& getRenderManager() { return m_renderManager; }


private:
    void onObjectChanged(Object* object);

private:
    ObjectManager m_objectManager;
    RenderManager m_renderManager;
};

