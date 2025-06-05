#pragma once

#include <memory>
#include <unordered_map>

#include "Renderer.h"
#include "RenderSystem.h"
#include "BufferPool.h"
#include "Object.h"
#include "FrameInfo.h"


class RenderManager
{
public:
    struct InstanceData
    {
        QMatrix4x4 transform;
        QVector3D color;
        float padding;
    };

    struct RenderBatch
    {
        uint32_t chunkId{ 0 };
        ModelType modelType{ ModelType::None };
        std::vector<Object*> objects;
        std::unique_ptr<VMABuffer> instanceBuffer;
        size_t bufferCapacity{ 0 };
        uint32_t instanceCount{ 0 };
        bool needsUpdate{ true };

        uint32_t frameLastUsed{ 0 };
        uint32_t renderCallCount{ 0 };
    };

    struct RenderStats
    {
        uint32_t totalObjects{ 0 };
        uint32_t visibleObjects{ 0 };
        uint32_t activeBatches{ 0 };
        uint32_t drawCalls{ 0 };
        uint64_t gpuMemoryUsed{ 0 };
        float frameTile{ 0.f };
        float batchUpdateTime{ 0.f };
        float renderTime{ 0.f };
    };


    RenderManager(MyVulkanWindow& window, Device& device, VkDescriptorSetLayout globalSetLayout);
    ~RenderManager() = default;

    //=================Swapchain 管理 =========================
    VkCommandBuffer beginFrame();
    void endFrame();
    void beginSwapChainRenderPass(VkCommandBuffer commandBuffer);
    void endSwapChainRenderPass(VkCommandBuffer commandBuffer);

    //=================缓冲区 管理 =========================
    uint32_t allocateRenderBuffer(const Object::Builder& builder);
    std::vector<uint32_t> getVisibleChunks(const Camera& camera, ModelType type) { return m_BufferPool.getVisibleChunks(camera, type); }

    const BufferPool::Chunk* getChunk(uint32_t chunkId) { return m_BufferPool.getChunk(chunkId); }
    //TODO: 删除和修改逻辑


   //=================渲染 逻辑 =========================
    void renderObjects(const std::vector<Object*>& objects, FrameInfo& frameInfo);

    Renderer& getRenderer() { return m_renderer; }
    RenderSystem* getRenderSystemByType(ModelType type);
private:
    //将Object 按chunk分组
    void devideObjectByChunks(std::unordered_map<uint32_t, std::vector<Object*>>& objectsByChunk, const std::vector<Object*>& objects);
    //根据Chunk进行渲染
    void renderObjectsByChunk(const std::unordered_map<uint32_t, std::vector<Object*>>& objectsByChunk, FrameInfo& frameInfo);
    //渲染ChunkBatch
    void renderChunkBatch(uint32_t chunkId, const std::vector<Object*>& objects, FrameInfo& frameInfo);

    RenderBatch& getOrCreateBatch(uint32_t chunkId);

    void updateInstanceBuffer(RenderBatch& batch, const std::vector<Object*>& objects);

    bool needInstanceBufferUpdate(const RenderBatch& batch, const std::vector<Object*>& objects);

    void allocateInstanceBuffer(RenderBatch& batch, size_t requiredSize);
private:
    Device& m_device;

    Renderer                                        m_renderer;

    std::unique_ptr<RenderSystem>                   m_pointRenderSystem;
    std::unique_ptr<RenderSystem>                   m_lineRenderSystem;
    std::unique_ptr<RenderSystem>                   m_polygonRenderSystem;


    BufferPool                                      m_BufferPool;
    std::unordered_map<uint32_t, RenderBatch>       m_renderBatches;

    RenderBatch& getBatch(Model* model);
    void updateBatch(RenderBatch& batch, const std::vector<Object*>& objects);
    void recordBatchCommands(RenderBatch& batch, const std::vector<Object*>& objects, VkPipelineLayout pipelineLayout);
};

