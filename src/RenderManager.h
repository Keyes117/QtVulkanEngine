#pragma once

#include <memory>
#include <unordered_map>

#include "Renderer.h"
#include "RenderSystem.h"
#include "BufferPool.h"
#include "Object.h"
#include "ObjectManager.h"
#include "FrameInfo.h"
#include "GpuFrustumCuller.h"

class RenderManager
{
public:

    RenderManager(MyVulkanWindow& window, Device& device, VkDescriptorSetLayout globalSetLayout);
    ~RenderManager() = default;

    //=================Swapchain 管理 =========================
    VkCommandBuffer beginFrame();
    void endFrame();
    void beginSwapChainRenderPass(VkCommandBuffer commandBuffer);
    void endSwapChainRenderPass(VkCommandBuffer commandBuffer);

    //=================渲染 逻辑 =========================
    void renderGPUDriven(const ObjectManager::ObjectDataPool& dataPool,
        FrameInfo& frameInfo);
    void executeGPUCulling(const ObjectManager::ObjectDataPool& dataPool, FrameInfo& frameInfo);
    //=================缓冲区 管理 =========================
    uint32_t allocateRenderBuffer(const Object::Builder& builder);
    const BufferPool::Chunk* getChunk(uint32_t chunkId) { return m_BufferPool.getChunk(chunkId); }

    Renderer& getRenderer() { return m_renderer; }
    BufferPool& getBufferPool() { return m_BufferPool; }

    void markDataChanged() { m_gpuDataNeedsUpdate = true; }
private:
    // 辅助方法
    RenderSystem* getRenderSystemByType(ModelType type);
    ModelType getChunkModelType(uint32_t chunkId);

    void initializeGPUCulling(const ObjectManager::ObjectDataPool& dataPool);
    void uploadObjectDataToGPU(const ObjectManager::ObjectDataPool& dataPool);

    void executeGPUDrivenDraw(FrameInfo& frameInfo);

private:
    void drawCommandBufferUpdateCallback();

private:
    Device& m_device;

    Renderer                                        m_renderer;
    BufferPool                                      m_BufferPool;

    std::unique_ptr<RenderSystem>                   m_pointRenderSystem;
    std::unique_ptr<RenderSystem>                   m_lineRenderSystem;
    std::unique_ptr<RenderSystem>                   m_polygonRenderSystem;

    std::unique_ptr<GpuFrustumCuller>               m_gpuCuller;

    bool m_gpuCullingInitialized = false;
    bool m_gpuDataNeedsUpdate = true;

    uint32_t m_currentFrame = 0;

};

