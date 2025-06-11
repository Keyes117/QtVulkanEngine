#pragma once

#include <memory>
#include <unordered_map>

#include "Renderer.h"
#include "RenderSystem.h"
#include "BufferPool.h"
#include "Object.h"
#include "ObjectManager.h"
#include "FrameInfo.h"


class RenderManager
{
public:

    struct RenderBatch
    {
        uint32_t chunkId;
        ModelType modelType;
        size_t instanceCount = 0;

        // ���ϵͳ
        bool needsUpdate = true;
        bool hasValidData = false;
        std::vector<uint32_t> lastIndices;  // �����ϴε�����

        // GPU������
        std::unique_ptr<VMABuffer> transformBuffer;
        std::unique_ptr<VMABuffer> colorBuffer;

        // ��������
        size_t transformCapacity = 0;
        size_t colorCapacity = 0;

        // ���ܼ��
        float lastUpdateTime = 0.0f;
        uint32_t frameLastUsed = 0;
    };

    struct RenderStats
    {
        uint32_t totalBatches = 0;
        uint32_t activeBatches = 0;
        uint32_t drawCalls = 0;
        uint32_t instancesRendered = 0;

        float frameTime = 0.0f;
        float updateTime = 0.0f;
        float uploadTime = 0.0f;
        float renderTime = 0.0f;

        uint64_t gpuMemoryUsed = 0;

        void reset() { *this = RenderStats{}; }
    };


    RenderManager(MyVulkanWindow& window, Device& device, VkDescriptorSetLayout globalSetLayout);
    ~RenderManager() = default;

    //=================Swapchain ���� =========================
    VkCommandBuffer beginFrame();
    void endFrame();
    void beginSwapChainRenderPass(VkCommandBuffer commandBuffer);
    void endSwapChainRenderPass(VkCommandBuffer commandBuffer);

    //=================��Ⱦ �߼� =========================
    void renderFromDataPool(const ObjectManager::ObjectDataPool& dataPool,
        const std::unordered_map<uint32_t, std::vector<uint32_t>>& chunkToIndices,
        FrameInfo& frameInfo);
    void testRenderSegment(FrameInfo& frameInfo);

    //=================������ ���� =========================
    uint32_t allocateRenderBuffer(const Object::Builder& builder);
    std::vector<uint32_t> getVisibleChunks(const Camera& camera, ModelType type) { return m_BufferPool.getVisibleChunks(camera, type); }

    const BufferPool::Chunk* getChunk(uint32_t chunkId) { return m_BufferPool.getChunk(chunkId); }

    Renderer& getRenderer() { return m_renderer; }
    BufferPool& getBufferPool() { return m_BufferPool; }
    const RenderStats getRenderStats() const { return m_stats; }
private:

    // ������Ⱦ����
    void updateBatch(RenderBatch& batch, const ObjectManager::ObjectDataPool& dataPool,
        const std::vector<uint32_t>& indices);
    void uploadBatchToGPU(RenderBatch& batch, const ObjectManager::ObjectDataPool& dataPool,
        const std::vector<uint32_t>& indices);
    void renderBatch(const RenderBatch& batch, FrameInfo& frameInfo);

    // ���ι���
    RenderBatch& getOrCreateBatch(uint32_t chunkId, ModelType modelType);

    // GPU����������
    void allocateBuffers(RenderBatch& batch, size_t instanceCount);
    void updateBuffers(RenderBatch& batch, const float* transformData, const float* colorData, size_t instanceCount);

    // ��������
    RenderSystem* getRenderSystemByType(ModelType type);
    ModelType getChunkModelType(uint32_t chunkId);
    void updateRenderStats();

private:
    Device& m_device;

    Renderer                                        m_renderer;
    BufferPool                                      m_BufferPool;

    std::unique_ptr<RenderSystem>                   m_pointRenderSystem;
    std::unique_ptr<RenderSystem>                   m_lineRenderSystem;
    std::unique_ptr<RenderSystem>                   m_polygonRenderSystem;

    std::unordered_map<uint32_t, RenderBatch>       m_renderBatches;

    RenderStats m_stats;
    uint32_t m_currentFrame = 0;

};

