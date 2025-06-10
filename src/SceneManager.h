#pragma once

#include "ObjectManager.h"
#include "RenderManager.h"
#include "FrameInfo.h"

#include "GpuFrustumCuller.h"

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
    void onObjectChanged(const std::shared_ptr<Object>& object);

private:


    ObjectManager m_objectManager;
    RenderManager m_renderManager;

    std::unique_ptr<GpuFrustumCuller> m_gpuCuller;

    struct RenderStatus
    {
        size_t totalObjects = 0;
        size_t visibleObjects = 0;
        float cullingTime = 0.f;
        float renderTime = 0.0f;

        bool useGPUCulling = false;
        bool useCPUCulling = true;
    };

    mutable RenderStatus m_stats;
    void updateRenderStats(const std::vector<std::shared_ptr<Object>>& visibleObjects);
    void printRenderStats() const;
};

