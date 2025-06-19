#pragma once

#include <unordered_map>
#include <vector>
#include <memory>
#include <mutex>

#include "Camera.h"
#include "Device.h"
#include "Model.h"
#include "Object.h"
#include "VMABuffer.h"

#define INVALID_CHUNK_ID -1

class BufferPool
{
public:
    struct Chunk
    {
        uint32_t vertexOffset{ 0 };
        uint32_t vertexCount{ 0 };
        uint32_t indexOffset{ 0 };
        uint32_t indexCount{ 0 };
        AABB bounds{ 0,0,0,0 };
        //uint32_t lodLevel;'
        bool    isLoaded{ false };
        uint32_t  segmentIndex{ 0 };
    };

    struct BufferSegment
    {
        std::unique_ptr<VMABuffer> vertexBuffer;
        std::unique_ptr<VMABuffer> indexBuffer;
        std::unique_ptr<VMABuffer> drawCommandBuffer;
        std::unique_ptr<VMABuffer> drawCountBuffer;

        VkDeviceAddress     drawCommandBufferAddress;
        VkDeviceAddress     drawCountBufferAddress;

        uint32_t drawCommandCapacity{ 0 };
        uint32_t vertexCapacity{ 0 };
        uint32_t indexCapacity{ 0 };
        uint32_t usedVertices{ 0 };
        uint32_t usedIndices{ 0 };

        std::vector<uint32_t>   chunks;

        bool isActive{ true };
        bool isCommandsNeedUpdate{ true };

    public:
        VkDeviceAddress getDrawCommandAddress() const { return drawCommandBufferAddress; }
        VkDeviceAddress getDrawCountAddress() const { return drawCountBufferAddress; }
        //uint32_t getMaxDrawCommands() const { return maxDrawCommands; }
        VkBuffer getDrawCommandBuffer() const { return drawCommandBuffer ? drawCommandBuffer->getBuffer() : VK_NULL_HANDLE; }
        VkBuffer getDrawCountBuffer() const { return drawCountBuffer ? drawCountBuffer->getBuffer() : VK_NULL_HANDLE; }
    };

    static constexpr uint32_t INITIAL_DRAW_COMMANDS = 200000;
    static constexpr uint32_t VERTICES_PER_SEGMENGT = 25000000;
    static constexpr uint32_t INDICES_PER_SEGMENT = 150000000;

    BufferPool(Device& device);
    ~BufferPool() = default;

    uint32_t allocateBuffer(const Object::Builder& builder);
    std::vector<uint32_t> getVisibleChunks(const Camera& camera, ModelType type);

    void bindBuffersForType(VkCommandBuffer commandBuffer, ModelType type, uint32_t segmentId = 0);

    void drawGPUDriven(VkCommandBuffer commandBuffer, ModelType type);

    const Chunk* getChunk(uint32_t chunkId) const;
    ModelType getChunkType(uint32_t chunkId) const;
    uint32_t getChunkBufferIndex(uint32_t chunkId) const;

    void printPoolStatus() const;

    const std::unordered_map<ModelType, std::vector<BufferSegment>>& getBufferSegments() const
    {
        return m_bufferSegments;
    }

    void setSegmentUpdateCallback(const std::function<void()>& segmentCallback)
    {
        m_segmentUpdateCallback = segmentCallback;
    }
private:
    void ensureDrawCommandCapacity(BufferSegment& segment, size_t requiredCount);
    void groupChunksBySegment(const std::vector<uint32_t>& chunks,
        std::unordered_map<uint32_t, std::vector<uint32_t>>& groupedChunks) const;


    //  新增：BDA相关函数
    void createDrawCommandBuffers(BufferSegment& segment);
    void updateBufferAddresses(BufferSegment& segment);
    void notifySegmentUpdate();
private:
    Device& m_device;
    mutable std::mutex                                          m_mutex;

    std::unordered_map<ModelType, std::vector<BufferSegment>>   m_bufferSegments;

    std::unordered_map<uint32_t, Chunk>                         m_chunks;
    std::unordered_map<uint32_t, ModelType>                     m_chunkTypes;
    std::unordered_map<uint32_t, uint32_t>                      m_chunkToSegmentIndex;

    uint32_t    m_nextChunkId{ 0 };

    std::function<void()> m_segmentUpdateCallback;

    BufferSegment* getOrCreateSegment(ModelType type);
    bool frustumCull(const AABB& bounds, const Camera& camera);
    void copyDataToSegment(BufferSegment* segement, const std::vector<Model::Vertex>& vertices,
        const std::vector<uint32_t>& indices, uint32_t vertexOffset, uint32_t indexOffset);
};

