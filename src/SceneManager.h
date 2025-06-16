#pragma once

#include <unordered_set>


#include "ObjectManager.h"
#include "RenderManager.h"
#include "FrameInfo.h"

class SceneManager
{
public:

    SceneManager(MyVulkanWindow& window, Device& device, VkDescriptorSetLayout globalSetLayout, const AABB& worldBounds);
    ~SceneManager() = default;

    /*对象管理*/
    ObjectManager::ObjectHandle addObject(const Object::Builder& builder);
    std::vector<ObjectManager::ObjectHandle> addObjectBatch(const std::vector<Object::Builder>& builders);

    void updateObjectTransform(ObjectManager::ObjectHandle handle, const QMatrix4x4& transform);
    void updateObjectColor(ObjectManager::ObjectHandle handle, const QVector3D& color);

    void culling(FrameInfo& frameInfo);
    void render(FrameInfo& frameInfo);

    ObjectManager& getOBjectManager() { return m_objectManager; }
    RenderManager& getRenderManager() { return m_renderManager; }
private:
    ObjectManager m_objectManager;
    RenderManager m_renderManager;

    uint32_t m_frameCounter = 0;
};

