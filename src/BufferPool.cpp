#include "BufferPool.h"

BufferPool::BufferPool(Device& device) :
    m_device(device),
    m_nextChunkId(1)
{
}

uint32_t BufferPool::allocateBuffer(const Object::Builder& builder)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    auto vertices = builder.vertices;
    auto indices = builder.indices;
    auto type = builder.type;

    if (vertices.empty())
    {
        qWarning() << "model has no vertices";
        return INVALID_CHUNK_ID;
    }

    auto* segment = getOrCreateSegment(type);
    if (!segment)
    {
        qWarning() << "failed to get or create segment for type " << static_cast<int>(type);
        return INVALID_CHUNK_ID;
    }

    //检查当前段是否有足够的空间
    if (segment->usedVertices + vertices.size() > segment->vertexCapacity ||
        segment->usedIndices + indices.size() > segment->indexCapacity)
    {
        qDebug() << "Buffer Segment full, creating new segment for type: "
            << static_cast<int>(type);

        m_bufferSegments[type].emplace_back();
        auto& newSegment = m_bufferSegments[type].back();

        newSegment.vertexCapacity = VERTICES_PER_SEGMENGT;
        newSegment.indexCapacity = INDICES_PER_SEGMENT;
        newSegment.usedVertices = 0;
        newSegment.usedIndices = 0;
        newSegment.isActive = true;
        newSegment.isCommandsNeedUpdate = true;

        try
        {
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

            createDrawCommandBuffers(newSegment);
            notifySegmentUpdate();
        }
        catch (const std::exception& e)
        {
            qWarning() << "failed to create buffers: " << e.what();
            m_bufferSegments[type].pop_back();
            return INVALID_CHUNK_ID;
        }


        //TODO 实现BufferSegment的 Copy-Constructor
        segment = &newSegment;
    }

    Chunk chunk;
    chunk.vertexOffset = segment->usedVertices;
    chunk.vertexCount = vertices.size();
    chunk.indexOffset = segment->usedIndices;
    chunk.indexCount = indices.size();
    chunk.isLoaded = true;
    chunk.bounds = builder.bounds;
    chunk.segmentIndex = m_bufferSegments[type].size() - 1;

    copyDataToSegment(segment, vertices, indices, chunk.vertexOffset, chunk.indexOffset);

    segment->usedVertices += vertices.size();
    segment->usedIndices += indices.size();

    uint32_t chunkId = m_nextChunkId++;
    m_chunks[chunkId] = chunk;
    m_chunkTypes[chunkId] = type;
    m_chunkToSegmentIndex[chunkId] = m_bufferSegments[type].size() - 1;

    segment->chunks.push_back(chunkId);
    segment->isCommandsNeedUpdate = true;

    qDebug() << "allocated geometry chunk" << chunkId
        << " with " << vertices.size() << " vertices ";

    return chunkId;
}

std::vector<uint32_t> BufferPool::getVisibleChunks(const Camera& camera, ModelType type)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    std::vector<uint32_t> visibleChunks;

    auto typeIt = m_bufferSegments.find(type);
    if (typeIt != m_bufferSegments.end())
    {
        for (const auto& segment : typeIt->second)
        {
            if (!segment.isActive)
                continue;

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
    //std::lock_guard<std::mutex> lock(m_mutex);

    auto typeIt = m_bufferSegments.find(type);
    if (typeIt == m_bufferSegments.end() || segmentId >= typeIt->second.size())
        return;


    const auto& segment = typeIt->second[segmentId];

    if (!segment.isActive || !segment.vertexBuffer)
        return;

    VkBuffer vertexBuffers[] = { segment.vertexBuffer->getBuffer() };
    VkDeviceSize offsets[] = { 0 };
    vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);

    if (segment.indexBuffer)
    {
        vkCmdBindIndexBuffer(commandBuffer, segment.indexBuffer->getBuffer(), 0, VK_INDEX_TYPE_UINT32);
    }
}

void BufferPool::drawGPUDriven(VkCommandBuffer commandBuffer, ModelType type)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    auto typeIt = m_bufferSegments.find(type);
    if (typeIt == m_bufferSegments.end() || typeIt->second.empty()) {
        return;
    }

    uint32_t drawCallOffset = 0;
    // 遍历该类型的所有segments
    for (uint32_t segmentIndex = 0; segmentIndex < typeIt->second.size(); ++segmentIndex)
    {
        const auto& segment = typeIt->second[segmentIndex];

        if (!segment.isActive || segment.chunks.empty()) {
            continue;
        }

        // 绑定几何缓冲区
        bindBuffersForType(commandBuffer, type, segmentIndex);
        uint32_t count = segment.chunks.size();
        // GPU驱动的间接绘制
        vkCmdDrawIndexedIndirectCount(
            commandBuffer,
            segment.drawCommandBuffer->getBuffer(),
            0,
            segment.drawCountBuffer->getBuffer(),
            0,
            segment.chunks.size(),
            sizeof(VkDrawIndexedIndirectCommand)
        );
        qDebug() << "[GPU-Driven] Segment" << segmentIndex;
    }
}

const BufferPool::Chunk* BufferPool::getChunk(uint32_t chunkId) const
{
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_chunks.find(chunkId);
    return (it != m_chunks.end()) ? &it->second : nullptr;
}

ModelType BufferPool::getChunkType(uint32_t chunkId) const
{
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_chunkTypes.find(chunkId);
    return (it != m_chunkTypes.end()) ? it->second : ModelType::None;
}

uint32_t BufferPool::getChunkBufferIndex(uint32_t chunkId) const
{
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_chunkToSegmentIndex.find(chunkId);
    return (it != m_chunkToSegmentIndex.end()) ? it->second : UINT32_MAX;
}

void BufferPool::printPoolStatus() const
{
    qDebug() << "=== Geometry Buffer Pool Statistics ===";

    size_t totalVertices = 0;
    size_t totalIndices = 0;
    size_t totalSegments = 0;
    size_t totalChunks = 0;

    for (const auto& [type, segments] : m_bufferSegments) {
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

void BufferPool::ensureDrawCommandCapacity(BufferSegment& segment, size_t requiredCount)
{
    if (requiredCount <= segment.drawCommandCapacity)
        return;

    // 扩展容量（增加50%）
    size_t newCapacity = static_cast<size_t>(requiredCount * 1.5f);

    try {
        auto newDrawCommandBuffer = std::make_unique<VMABuffer>(
            m_device,
            sizeof(VkDrawIndexedIndirectCommand),
            newCapacity,
            VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
            VMA_MEMORY_USAGE_CPU_TO_GPU
        );

        segment.drawCommandBuffer = std::move(newDrawCommandBuffer);
        segment.drawCommandCapacity = newCapacity;
        updateBufferAddresses(segment);
        qDebug() << "Expanded draw command buffer capacity to" << newCapacity;
    }
    catch (const std::exception& e) {
        qWarning() << "Failed to expand draw command buffer:" << e.what();
    }
}

void BufferPool::groupChunksBySegment(const std::vector<uint32_t>& chunks, std::unordered_map<uint32_t, std::vector<uint32_t>>& groupedChunks) const
{
    groupedChunks.clear();

    for (uint32_t chunkId : chunks) {
        auto it = m_chunkToSegmentIndex.find(chunkId);
        if (it != m_chunkToSegmentIndex.end()) {
            groupedChunks[it->second].push_back(chunkId);
        }
    }
}

void BufferPool::createDrawCommandBuffers(BufferSegment& segment)
{
    segment.drawCommandBuffer = std::make_unique<VMABuffer>(
        m_device,
        sizeof(VkDrawIndexedIndirectCommand),
        INITIAL_DRAW_COMMANDS,
        VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        VMA_MEMORY_USAGE_GPU_ONLY
    );

    segment.drawCountBuffer = std::make_unique<VMABuffer>(
        m_device,
        sizeof(uint32_t),
        1,
        VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VMA_MEMORY_USAGE_GPU_ONLY
    );

    updateBufferAddresses(segment);

    qDebug() << "[BufferPool] Created draw command buffers for segment, max commands:" << INITIAL_DRAW_COMMANDS;
}

BufferPool::BufferSegment* BufferPool::getOrCreateSegment(ModelType type)
{
    auto& segments = m_bufferSegments[type];

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

        createDrawCommandBuffers(segment);
    }

    return &segments.back();
}

void BufferPool::updateBufferAddresses(BufferSegment& segment)
{
    VkBufferDeviceAddressInfo addressInfo{};
    addressInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
    addressInfo.pNext = nullptr;

    if (segment.drawCommandBuffer) {
        addressInfo.buffer = segment.drawCommandBuffer->getBuffer();
        segment.drawCommandBufferAddress = vkGetBufferDeviceAddress(m_device.device(), &addressInfo);
    }

    if (segment.drawCountBuffer) {
        addressInfo.buffer = segment.drawCountBuffer->getBuffer();
        segment.drawCountBufferAddress = vkGetBufferDeviceAddress(m_device.device(), &addressInfo);
    }

    notifySegmentUpdate();
    qDebug() << "[BufferPool] Updated buffer addresses:"
        << "DrawCommand=" << segment.drawCommandBufferAddress
        << "DrawCount=" << segment.drawCountBufferAddress;
}

bool BufferPool::frustumCull(const AABB& bounds, const Camera& camera)
{
    auto frustum = camera.getFrustum2D();
    return !frustum.insersects(bounds);
}

void BufferPool::notifySegmentUpdate()
{
    if (m_segmentUpdateCallback) {
        m_segmentUpdateCallback();
    }
}

void BufferPool::copyDataToSegment(BufferSegment* segment,
    const std::vector<Model::Vertex>& vertices,
    const std::vector<uint32_t>& indices,
    uint32_t vertexOffset,
    uint32_t indexOffset)
{

    //TODO: 验证segment

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

    VkBufferCopy copyRegion{};
    copyRegion.srcOffset = 0;
    copyRegion.dstOffset = vertexOffset * sizeof(Model::Vertex);  // 使用正确的偏移量
    copyRegion.size = vertexDataSize;

    m_device.copyBufferWithInfo(vertexStagingBuffer.getBuffer(), segment->vertexBuffer->getBuffer(), copyRegion);

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

        VkBufferCopy indexCopyRegion{};
        indexCopyRegion.srcOffset = 0;
        indexCopyRegion.dstOffset = indexOffset * sizeof(uint32_t);  // 使用正确的偏移量
        indexCopyRegion.size = indexDataSize;

        m_device.copyBufferWithInfo(indexStagingBuffer.getBuffer(), segment->indexBuffer->getBuffer(), indexCopyRegion);
    }


}
