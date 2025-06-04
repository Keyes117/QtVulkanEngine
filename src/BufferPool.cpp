#include "BufferPool.h"

BufferPool::BufferPool(Device& device) :
    m_device(device),
    m_nextChunkId(1)
{
}

uint32_t BufferPool::allocateBuffer(const Object& object)
{
    auto vertices = object.getModel()->getVerticeRef();
    auto indices = object.getModel()->getIndicesRef();
    auto type = object.getModel()->type();

    if (vertices.empty())
        return 0;

    auto* segment = getOrCreateSegment(type);

    //检查当前段是否有足够的空间
    if (segment->usedVertices + vertices.size() > segment->vertexCapacity ||
        segment->usedIndices + indices.size() > segment->indexCapacity)
    {
        qDebug() << "Buffer Segment full, creating new segment for type: "
            << static_cast<int>(type);

        m_bufferPools[type].emplace_back();
        auto& newSegment = m_bufferPools[type].back();

        newSegment.vertexCapacity = VERTICES_PER_SEGMENGT;
        newSegment.indexCapacity = INDICES_PER_SEGMENT;
        newSegment.usedVertices = 0;
        newSegment.usedIndices = 0;

        //创建GPU缓冲区
        newSegment.vertexBuffer = std::make_unique<VMABuffer>(
            m_device,
            sizeof(Model::Vertex),
            newSegment.vertexCapacity,
            VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            VMA_MEMORY_USAGE_GPU_ONLY
        );

        newSegment.indexBuffer = std::make_unique<VMABuffer>(
            m_device,
            sizeof(uint32_t),
            newSegment.indexCapacity,
            VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            VMA_MEMORY_USAGE_GPU_ONLY
        );

        //TODO 实现BufferSegment的 Copy-Constructor
        segment = &newSegment;
    }

    Chunk chunk;
    chunk.vertexOffset = segment->usedVertices;
    chunk.vertexCount = vertices.size();
    chunk.indexOffset = segment->usedIndices;
    chunk.indexCount = indices.size();
    chunk.isLoaded = true;

    copyDataToSegment(segment, vertices, indices, chunk.vertexOffset, chunk.indexOffset);

    segment->usedVertices += vertices.size();
    segment->usedIndices += indices.size();

    uint32_t chunkId = m_nextChunkId++;
    m_chunks[chunkId] = chunk;
    m_chunkTypes[chunkId] = type;
    m_chunkToSegmentIndex[chunkId] = m_bufferPools[type].size() - 1;

    segment->chunks.push_back(chunkId);

    qDebug() << "allocated geometry chunk" << chunkId
        << " with " << vertices.size() << " vertices ";

    return chunkId;
}

std::vector<uint32_t> BufferPool::getVisibleChunks(const Camera& camera, ModelType type)
{
    std::vector<uint32_t> visibleChunks;

    auto typeIt = m_bufferPools.find(type);
    if (typeIt != m_bufferPools.end())
    {
        for (const auto& segment : typeIt->second)
        {
            for (uint32_t chunkId : segment.chunks)
            {
                const auto& chunk = m_chunks[chunkId];
                if (chunk.isLoaded && !frustumCull(chunk.bounds, camera))
                {
                    visibleChunks.push_back(chunkId);
                }
            }
        }
    }

    return  visibleChunks;
}

void BufferPool::bindBuffersForType(VkCommandBuffer commandBuffer, ModelType type, uint32_t segmentId)
{
    auto typeIt = m_bufferPools.find(type);
    if (typeIt == m_bufferPools.end() || segmentId >= typeIt->second.size())
        return;


    const auto& segment = typeIt->second[segmentId];

    VkBuffer vertexBuffers[] = { segment.vertexBuffer->getBuffer() };
    VkDeviceSize offsets[] = { 0 };
    vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);

    if (segment.indexBuffer)
    {
        vkCmdBindIndexBuffer(commandBuffer, segment.indexBuffer->getBuffer(), 0, VK_INDEX_TYPE_UINT32);
    }
}

void BufferPool::drawChunk(VkCommandBuffer commandBuffer, uint32_t chunkId, uint32_t instanceCount)
{
    auto it = m_chunks.find(chunkId);
    if (it == m_chunks.end())
        return;

    const auto& chunk = it->second;

    if (chunk.indexCount > 0)
    {
        vkCmdDrawIndexed(commandBuffer,
            chunk.indexCount,
            instanceCount,
            chunk.indexOffset,
            chunk.vertexOffset,
            0);
    }
    else
    {
        vkCmdDraw(commandBuffer,
            chunk.vertexCount,
            instanceCount,
            chunk.vertexOffset,
            0);
    }
}

void BufferPool::printPoolStatus() const
{
    qDebug() << "=== Geometry Buffer Pool Statistics ===";

    size_t totalVertices = 0;
    size_t totalIndices = 0;
    size_t totalSegments = 0;
    size_t totalChunks = 0;

    for (const auto& [type, segments] : m_bufferPools) {
        qDebug() << "Type " << static_cast<int>(type) << ": " << segments.size() << " segments";

        for (const auto& segment : segments) {
            totalVertices += segment.usedVertices;
            totalIndices += segment.usedIndices;
            totalSegments++;
            totalChunks += segment.chunks.size();

            qDebug() << "  Segment: " << segment.usedVertices << "/" << segment.vertexCapacity
                << " vertices, " << segment.usedIndices << "/" << segment.indexCapacity
                << " indices, " << segment.chunks.size() << " chunks";
        }
    }

    qDebug() << "Total: " << totalVertices << " vertices, " << totalIndices
        << " indices, " << totalSegments << " segments, " << totalChunks << " chunks";

    float memoryMB = (totalVertices * sizeof(Model::Vertex) + totalIndices * sizeof(uint32_t)) / (1024.0f * 1024.0f);
    qDebug() << "Estimated GPU Memory: " << memoryMB << " MB";
    qDebug() << "VMABuffer objects created: " << totalSegments * 2;
}

BufferPool::BufferSegment* BufferPool::getOrCreateSegment(ModelType type)
{
    auto& segments = m_bufferPools[type];

    //如果当前缓存池中没有已经分配的缓冲端，则分配一个
    if (segments.empty())
    {
        qDebug() << "Creating first buffer segment for type " << static_cast<int>(type);

        segments.emplace_back();
        auto& segment = segments.back();

        segment.vertexCapacity = VERTICES_PER_SEGMENGT;
        segment.indexCapacity = INDICES_PER_SEGMENT;
        segment.usedVertices = 0;
        segment.usedIndices = 0;

        segment.vertexBuffer = std::make_unique<VMABuffer>(
            m_device,
            sizeof(Model::Vertex),
            segment.vertexCapacity,
            VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            VMA_MEMORY_USAGE_GPU_ONLY
        );

        segment.indexBuffer = std::make_unique<VMABuffer>(
            m_device,
            sizeof(uint32_t),
            segment.indexCapacity,
            VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            VMA_MEMORY_USAGE_GPU_ONLY
        );
    }

    return &segments.back();
}

bool BufferPool::frustumCull(const AABB& bounds, const Camera& camera)
{
    auto frustum = camera.getFrustum2D();
    return !frustum.insersects(bounds);
}

void BufferPool::copyDataToSegment(BufferSegment* segment,
    const std::vector<Model::Vertex>& vertices,
    const std::vector<uint32_t>& indices,
    uint32_t vertexOffset,
    uint32_t indexOffset)
{
    VkDeviceSize vertexDataSize = vertices.size() * sizeof(Model::Vertex);

    VMABuffer vertexStagingBuffer(
        m_device,
        sizeof(Model::Vertex),
        vertices.size(),
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VMA_MEMORY_USAGE_CPU_TO_GPU
    );

    vertexStagingBuffer.map();
    vertexStagingBuffer.writeToBuffer(const_cast<void*>(static_cast<const void*>(vertices.data())));

    m_device.copyBuffer(vertexStagingBuffer.getBuffer(), segment->vertexBuffer->getBuffer(), vertexDataSize);

    if (!indices.empty())
    {
        VkDeviceSize indexDataSize = indices.size() * sizeof(uint32_t);

        VMABuffer indexStagingBuffer(
            m_device,
            sizeof(uint32_t),
            indices.size(),
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            VMA_MEMORY_USAGE_CPU_TO_GPU
        );

        indexStagingBuffer.map();
        indexStagingBuffer.writeToBuffer(const_cast<void*>(static_cast<const void*>(indices.data())));

        m_device.copyBuffer(indexStagingBuffer.getBuffer(), segment->indexBuffer->getBuffer(), indexDataSize);
    }

}
