#pragma once

#include <memory>
#include <unordered_map>

#include"const.h"
#include "Renderer.h"
#include "RenderSystem.h"
#include "BufferPool.h"
#include "Object.h"
#include "FrameInfo.h"


class RenderManager
{
public:
    struct RenderBatch
    {
        uint32_t chunkId{ 0 };
        ModelType modelType{ ModelType::None };
        std::vector<std::shared_ptr<Object>> objects;
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
    void renderObjects(const std::vector<std::shared_ptr<Object>>& objects, FrameInfo& frameInfo);

    Renderer& getRenderer() { return m_renderer; }
    RenderSystem* getRenderSystemByType(ModelType type);

    void printRenderStats() const;

    void setChunksObjectMap(uint32_t chunkid, std::shared_ptr<Object> objectId)
    {
        m_chunkIdToObjectMap[chunkid] = objectId;
    }


    //新增优化的方法
private:
    struct RenderStatistics {
        size_t batches = 0;
        size_t drawCalls = 0;
        size_t stateChanges = 0;
        size_t bufferBinds = 0;
        size_t pushConstantUpdates = 0;
        float batchingTime = 0.0f;
        float renderingTime = 0.0f;

    };

    struct MergedDrawCall {
        uint32_t segmentIndex;
        std::vector<uint32_t> chunks;
        uint32_t totalVertices = 0;
        uint32_t totalIndices = 0;
        ModelType type;
    };
    struct SpatialRenderBatch
    {
        ModelType type;
        std::vector<uint32_t> chunkIds;
        std::vector<std::shared_ptr<Object>> objects;
        AABB combinedBounds;

        SpatialRenderBatch()
        {
            combinedBounds = AABB(
                std::numeric_limits<float>::infinity(),
                std::numeric_limits<float>::infinity(),
                -std::numeric_limits<float>::infinity(),
                -std::numeric_limits<float>::infinity()
            );
        };
    };

    void updateChunkObjectMapping(const std::vector<std::shared_ptr<Object>>& objects);

    void updateRenderStatistics(const std::vector<SpatialRenderBatch>& batches);

    std::vector<SpatialRenderBatch> createSpatialBatches(
        const std::vector<std::shared_ptr<Object>>& visibleObjects);
    void optimizeBatches(std::vector<SpatialRenderBatch>& batches);
    void renderSpatialBatch(const SpatialRenderBatch& batch, FrameInfo& frameInfo);

    std::vector<MergedDrawCall> createMergedDrawCalls(
        const std::vector<uint32_t>& chunkIds, ModelType type);

    void executeMergedDrawCall(const MergedDrawCall& drawCall,
        uint32_t instanceCount, FrameInfo& frameInfo);

    void renderSingleChunkWithTransform(uint32_t chunkId, RenderSystem* renderSystem, FrameInfo& frameInfo);

    bool areChunksContiguous(uint32_t chunkA, uint32_t chunkB);

    std::vector<SpatialRenderBatch> createOptimizedBatches(const std::vector<std::shared_ptr<Object>>& visibleObjects);

private:
    void bindGlobalResources(VkCommandBuffer commandBuffer,
        VkDescriptorSet globalDescriptorSet, VkPipelineLayout pipelineLayout);

    //将Object 按chunk分组
    void devideObjectByChunks(std::unordered_map<uint32_t, std::vector<std::shared_ptr<Object>>>& objectsByChunk, const std::vector<std::shared_ptr<Object>>& objects);
    //根据Chunk进行渲染
    void renderObjectsByChunk(const std::unordered_map<uint32_t, std::vector<std::shared_ptr<Object>>>& objectsByChunk, FrameInfo& frameInfo);
    //渲染ChunkBatch
    void renderChunkBatch(uint32_t chunkId, const std::vector<std::shared_ptr<Object>>& objects, FrameInfo& frameInfo);

    RenderBatch& getOrCreateBatch(uint32_t chunkId);

    void updateInstanceBuffer(RenderBatch& batch, const std::vector<std::shared_ptr<Object>>& objects);

    bool needInstanceBufferUpdate(const RenderBatch& batch, const std::vector<std::shared_ptr<Object>>& objects);

    void allocateInstanceBuffer(RenderBatch& batch, size_t requiredSize);
private:
    Device& m_device;

    Renderer                                        m_renderer;

    mutable RenderStatistics                        m_renderStats;

    std::unique_ptr<RenderSystem>                   m_pointRenderSystem;
    std::unique_ptr<RenderSystem>                   m_lineRenderSystem;
    std::unique_ptr<RenderSystem>                   m_polygonRenderSystem;


    BufferPool                                      m_BufferPool;
    std::unordered_map<uint32_t, RenderBatch>       m_renderBatches;
    std::unordered_map<uint32_t, std::shared_ptr<Object>>   m_chunkIdToObjectMap;

    RenderBatch& getBatch(Model* model);
    void updateBatch(RenderBatch& batch, const std::vector<Object*>& objects);
    void recordBatchCommands(RenderBatch& batch, const std::vector<Object*>& objects, VkPipelineLayout pipelineLayout);
};

