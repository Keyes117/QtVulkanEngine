#include "RenderManager.h"

#ifdef max
#undef max
#endif

RenderManager::RenderManager(MyVulkanWindow& window, Device& device, VkDescriptorSetLayout globalSetLayout) :
    m_device(device),
    m_renderer(window, device),
    m_BufferPool(device)
{
    m_pointRenderSystem = std::make_unique<RenderSystem>(
        device,
        m_renderer.getSwapChainRenderPass(),
        globalSetLayout,
        VK_PRIMITIVE_TOPOLOGY_POINT_LIST
    );

    m_lineRenderSystem = std::make_unique<RenderSystem>(
        device,
        m_renderer.getSwapChainRenderPass(),
        globalSetLayout,
        VK_PRIMITIVE_TOPOLOGY_LINE_LIST
    );

    m_polygonRenderSystem = std::make_unique<RenderSystem>(
        device,
        m_renderer.getSwapChainRenderPass(),
        globalSetLayout,
        VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST
    );

}

VkCommandBuffer RenderManager::beginFrame()
{
    return m_renderer.beginFrame();
}

void RenderManager::endFrame()
{
    m_renderer.endFrame();
}

void RenderManager::beginSwapChainRenderPass(VkCommandBuffer commandBuffer)
{
    m_renderer.beginSwapChainRenderPass(commandBuffer);
}

void RenderManager::endSwapChainRenderPass(VkCommandBuffer commandBuffer)
{
    m_renderer.endSwapChainRenderPass(commandBuffer);
}

void RenderManager::renderFromDataPool(const ObjectManager::ObjectDataPool& dataPool, const std::unordered_map<uint32_t, std::vector<uint32_t>>& chunkToIndices, FrameInfo& frameInfo)
{
    auto frameStart = std::chrono::high_resolution_clock::now();

    m_stats.reset();
    if (chunkToIndices.empty())
    {
        return;
    }

    for (const auto& [chunkId, indices] : chunkToIndices)
    {
        if (chunkId == 0 || indices.empty())
            continue;

        ModelType modelType = getChunkModelType(chunkId);
        if (modelType == ModelType::None)
            continue;

        RenderBatch& batch = getOrCreateBatch(chunkId, modelType);

        updateBatch(batch, dataPool, indices);

        uploadBatchToGPU(batch, dataPool, indices);

        renderBatch(batch, frameInfo);

        m_stats.activeBatches++;
        m_stats.instancesRendered += static_cast<uint32_t>(indices.size());

    }

    m_currentFrame++;
    auto frameEnd = std::chrono::high_resolution_clock::now();
    m_stats.frameTime = std::chrono::duration<float, std::milli>(frameEnd - frameStart).count();


    qDebug() << "SOARender - Frame:" << m_stats.frameTime << "ms, Batches:" << m_stats.activeBatches
        << ", Instances:" << m_stats.instancesRendered << ", Draw calls:" << m_stats.drawCalls;
    if (m_currentFrame % 60 == 0)
    {

    }

}

uint32_t RenderManager::allocateRenderBuffer(const Object::Builder& builder)
{
    return m_BufferPool.allocateBuffer(builder);

}

void RenderManager::testRenderSegment(FrameInfo& frameInfo)
{

    for (int i = 0; i < 4; ++i) {
        ModelType type = static_cast<ModelType>(i);

        auto* renderSystem = getRenderSystemByType(type);
        if (!renderSystem) continue;

        renderSystem->bind(frameInfo.commandBuffer);

        // 🚀 绑定全局描述符集（相机矩阵等）
        if (frameInfo.globalDescriptorSet != VK_NULL_HANDLE) {
            vkCmdBindDescriptorSets(
                frameInfo.commandBuffer,
                VK_PIPELINE_BIND_POINT_GRAPHICS,
                renderSystem->getPipelineLayout(),
                0, 1, &frameInfo.globalDescriptorSet,
                0, nullptr
            );
        }

        // 🚀 一次性渲染整个Segment
        m_BufferPool.drawSegment(frameInfo.commandBuffer, type, 0);
    }
}

void RenderManager::updateBatch(RenderBatch& batch, const ObjectManager::ObjectDataPool& dataPool, const std::vector<uint32_t>& indices)
{
    auto updateStart = std::chrono::high_resolution_clock::now();

    // ✅ 智能脏检测 - 只有真正改变时才更新
    bool needsUpdate = false;

    // 检查实例数量变化
    if (batch.instanceCount != indices.size()) {
        needsUpdate = true;
        batch.instanceCount = indices.size();
    }

    // 检查索引是否变化
    if (batch.lastIndices != indices) {
        needsUpdate = true;
        batch.lastIndices = indices;
    }

    // 检查数据是否有更新标记
    for (uint32_t index : indices) {
        if (index < dataPool.m_count && dataPool.m_updateFlags[index] != 0) {
            needsUpdate = true;
            break;
        }
    }

    batch.needsUpdate = needsUpdate;
    batch.frameLastUsed = m_currentFrame;

    auto updateEnd = std::chrono::high_resolution_clock::now();
    batch.lastUpdateTime = std::chrono::duration<float, std::milli>(updateEnd - updateStart).count();
}

void RenderManager::uploadBatchToGPU(RenderBatch& batch, const ObjectManager::ObjectDataPool& dataPool, const std::vector<uint32_t>& indices)
{
    if (!batch.needsUpdate) {
        return;
    }
    auto uploadStart = std::chrono::high_resolution_clock::now();

    if (indices.empty())
        return;

    size_t instanceCount = indices.size();
    allocateBuffers(batch, instanceCount);

    // 预分配
    static thread_local std::vector<float> transformData;
    static thread_local std::vector<float> colorData;

    transformData.clear();
    colorData.clear();
    transformData.reserve(instanceCount * ObjectManager::FLOAT_NUM_PER_TRANSFORM);
    colorData.reserve(instanceCount * ObjectManager::FLOAT_NUM_PER_COLOR);

    //数据拷贝
    const float* transformBase = dataPool.m_transforms.data();
    const float* colorBase = dataPool.m_colors.data();

    for (uint32_t index : indices) {
        if (index < dataPool.m_count) {
            // 变换数据
            const float* transformSrc = transformBase + index * ObjectManager::FLOAT_NUM_PER_TRANSFORM;
            transformData.insert(transformData.end(), transformSrc, transformSrc + ObjectManager::FLOAT_NUM_PER_TRANSFORM);

            // 颜色数据
            const float* colorSrc = colorBase + index * ObjectManager::FLOAT_NUM_PER_COLOR;
            colorData.insert(colorData.end(), colorSrc, colorSrc + ObjectManager::FLOAT_NUM_PER_COLOR);
        }
    }

    updateBuffers(batch, transformData.data(), colorData.data(), instanceCount);
    batch.needsUpdate = false;
    batch.hasValidData = true;

    auto uploadEnd = std::chrono::high_resolution_clock::now();
    m_stats.updateTime += std::chrono::duration<float, std::milli>(uploadEnd - uploadStart).count();
}

void RenderManager::renderBatch(const RenderBatch& batch, FrameInfo& frameInfo)
{

    auto renderStart = std::chrono::high_resolution_clock::now();

    if (batch.instanceCount == 0)
        return;

    auto* renderSystem = getRenderSystemByType(batch.modelType);

    if (!renderSystem)
        return;

    renderSystem->bind(frameInfo.commandBuffer);

    uint32_t segmentIndex = m_BufferPool.getChunkBufferIndex(batch.chunkId);
    m_BufferPool.bindBuffersForType(frameInfo.commandBuffer, batch.modelType, segmentIndex);

    // 绑定SOA实例数据
    if (batch.transformBuffer && batch.colorBuffer) {
        VkBuffer instanceBuffers[] = {
            batch.transformBuffer->getBuffer(),
            batch.colorBuffer->getBuffer()
        };
        VkDeviceSize offsets[] = { 0, 0 };

        // 绑定到顶点输入位置1和2
        vkCmdBindVertexBuffers(frameInfo.commandBuffer, 1, 2, instanceBuffers, offsets);
    }
    // 绑定描述符集
    if (frameInfo.globalDescriptorSet != VK_NULL_HANDLE) {
        vkCmdBindDescriptorSets(
            frameInfo.commandBuffer,
            VK_PIPELINE_BIND_POINT_GRAPHICS,
            renderSystem->getPipelineLayout(),
            0, 1, &frameInfo.globalDescriptorSet,
            0, nullptr
        );
    }


    m_BufferPool.drawChunk(frameInfo.commandBuffer, batch.chunkId, static_cast<uint32_t>(batch.instanceCount));

    m_stats.drawCalls++;

    auto renderEnd = std::chrono::high_resolution_clock::now();
    float renderTime = std::chrono::duration<float, std::milli>(renderEnd - renderStart).count();
    m_stats.renderTime += renderTime;
}

RenderManager::RenderBatch& RenderManager::getOrCreateBatch(uint32_t chunkId, ModelType modelType)
{
    auto it = m_renderBatches.find(chunkId);
    if (it == m_renderBatches.end()) {
        RenderBatch newBatch;
        newBatch.chunkId = chunkId;
        newBatch.modelType = modelType;
        newBatch.needsUpdate = true;

        m_renderBatches[chunkId] = std::move(newBatch);
        qDebug() << "Created new SOA render batch for chunk:" << chunkId;
    }

    return m_renderBatches[chunkId];
}

void RenderManager::allocateBuffers(RenderBatch& batch, size_t instanceCount)
{
    size_t transformSize = instanceCount * ObjectManager::FLOAT_NUM_PER_TRANSFORM * sizeof(float);
    size_t colorSize = instanceCount * ObjectManager::FLOAT_NUM_PER_COLOR * sizeof(float);

    // 分配变换矩阵缓冲区
    if (!batch.transformBuffer || batch.transformCapacity < transformSize) {
        size_t newCapacity = static_cast<size_t>(instanceCount * 1.3f); // 预留30%空间

        try {
            batch.transformBuffer = std::make_unique<VMABuffer>(
                m_device,
                sizeof(float) * ObjectManager::FLOAT_NUM_PER_TRANSFORM,
                newCapacity,
                VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                VMA_MEMORY_USAGE_CPU_TO_GPU
            );
            batch.transformCapacity = newCapacity * ObjectManager::FLOAT_NUM_PER_TRANSFORM * sizeof(float);
        }
        catch (const std::exception& e) {
            qDebug() << "Failed to allocate transform buffer:" << e.what();
            return;
        }
    }

    // 分配颜色缓冲区
    if (!batch.colorBuffer || batch.colorCapacity < colorSize) {
        size_t newCapacity = static_cast<size_t>(instanceCount * 1.3f);

        try {
            batch.colorBuffer = std::make_unique<VMABuffer>(
                m_device,
                sizeof(float) * ObjectManager::FLOAT_NUM_PER_COLOR,
                newCapacity,
                VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                VMA_MEMORY_USAGE_CPU_TO_GPU
            );
            batch.colorCapacity = newCapacity * ObjectManager::FLOAT_NUM_PER_COLOR * sizeof(float);
        }
        catch (const std::exception& e) {
            qDebug() << "Failed to allocate color buffer:" << e.what();
            return;
        }
    }
}

void RenderManager::updateBuffers(RenderBatch& batch, const float* transformData, const float* colorData, size_t instanceCount)
{
    // 上传变换数据
    if (batch.transformBuffer && transformData) {
        size_t dataSize = instanceCount * ObjectManager::FLOAT_NUM_PER_TRANSFORM * sizeof(float);
        batch.transformBuffer->map();
        batch.transformBuffer->writeToBuffer(const_cast<float*>(transformData), dataSize);
    }

    // 上传颜色数据
    if (batch.colorBuffer && colorData) {
        size_t dataSize = instanceCount * ObjectManager::FLOAT_NUM_PER_COLOR * sizeof(float);
        batch.colorBuffer->map();
        batch.colorBuffer->writeToBuffer(const_cast<float*>(colorData), dataSize);
    }
}

RenderSystem* RenderManager::getRenderSystemByType(ModelType type)
{
    switch (type) {
    case ModelType::Point:   return m_pointRenderSystem.get();
    case ModelType::Line:    return m_lineRenderSystem.get();
    case ModelType::Polygon: return m_polygonRenderSystem.get();
    default: return nullptr;
    }
}

ModelType RenderManager::getChunkModelType(uint32_t chunkId)
{
    return m_BufferPool.getChunkType(chunkId);
}

void RenderManager::updateRenderStats()
{
    m_stats.totalBatches = static_cast<uint32_t>(m_renderBatches.size());

    // 计算GPU内存使用
    uint64_t totalMemory = 0;
    for (const auto& [id, batch] : m_renderBatches) {
        if (batch.transformBuffer) {
            totalMemory += batch.transformBuffer->getBufferSize();
        }
        if (batch.colorBuffer) {
            totalMemory += batch.colorBuffer->getBufferSize();
        }
    }
    m_stats.gpuMemoryUsed = totalMemory;
}


