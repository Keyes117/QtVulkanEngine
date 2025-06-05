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
        uint32_t vertexCapacity{ 0 };
        uint32_t indexCapacity{ 0 };
        uint32_t usedVertices{ 0 };
        uint32_t usedIndices{ 0 };
        std::vector<uint32_t>   chunks;
        bool isActive{ true };

    };

    static constexpr uint32_t VERTICES_PER_SEGMENGT = 50000000;
    static constexpr uint32_t INDICES_PER_SEGMENT = 150000000;

    BufferPool(Device& device);
    ~BufferPool() = default;

    uint32_t allocateBuffer(const Object::Builder& builder);
    std::vector<uint32_t> getVisibleChunks(const Camera& camera, ModelType type);

    void bindBuffersForType(VkCommandBuffer commandBuffer, ModelType type, uint32_t segmentId = 0);
    void drawChunk(VkCommandBuffer commandBuffer, uint32_t chunkId, uint32_t instanceCount = 1);

    const Chunk* getChunk(uint32_t chunkId) const;
    ModelType getChunkType(uint32_t chunkId) const;
    uint32_t getChunkBufferIndex(uint32_t chunkId) const;

    void printPoolStatus() const;

private:
    Device& m_device;
    mutable std::mutex                                          m_mutex;

    std::unordered_map<ModelType, std::vector<BufferSegment>>   m_bufferPools;

    std::unordered_map<uint32_t, Chunk>                         m_chunks;
    std::unordered_map<uint32_t, ModelType>                     m_chunkTypes;
    std::unordered_map<uint32_t, uint32_t>                      m_chunkToSegmentIndex;

    uint32_t    m_nextChunkId{ 0 };

    BufferSegment* getOrCreateSegment(ModelType type);
    bool frustumCull(const AABB& bounds, const Camera& camera);
    void copyDataToSegment(BufferSegment* segement, const std::vector<Model::Vertex>& vertices,
        const std::vector<uint32_t>& indices, uint32_t vertexOffset, uint32_t indexOffset);
};

