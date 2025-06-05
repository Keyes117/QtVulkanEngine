#include "RenderManager.h"

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

uint32_t RenderManager::allocateRenderBuffer(const Object::Builder& builder)
{
    return m_BufferPool.allocateBuffer(builder);

}

void RenderManager::renderObjects(const std::vector<Object*>& objects, FrameInfo& frameInfo)
{
    if (objects.empty())
        return;

    //按Chunk分组对象
    std::unordered_map<uint32_t, std::vector<Object*>> objectsByChunk;
    devideObjectByChunks(objectsByChunk, objects);

    renderObjectsByChunk(objectsByChunk, frameInfo);

}

void RenderManager::devideObjectByChunks(std::unordered_map<uint32_t, std::vector<Object*>>& objectsByChunk, const std::vector<Object*>& objects)
{
    for (Object* obj : objects) {
        if (obj && obj->getChunkID() != 0) {
            objectsByChunk[obj->getChunkID()].push_back(obj);
        }
    }
}

void RenderManager::renderObjectsByChunk(const std::unordered_map<uint32_t, std::vector<Object*>>& objectsByChunk, FrameInfo& frameInfo)
{
    for (const auto& [chunkId, chunkObjects] : objectsByChunk)
    {
        if (chunkId != 0 && !chunkObjects.empty())
        {
            renderChunkBatch(chunkId, chunkObjects, frameInfo);
        }
    }
}

void RenderManager::renderChunkBatch(uint32_t chunkId, const std::vector<Object*>& objects, FrameInfo& frameInfo)
{
    if (objects.empty())
        return;

    ModelType type = m_BufferPool.getChunkType(chunkId);
    if (type == ModelType::None)
    {
        return;
    }

    auto* renderSystem = getRenderSystemByType(type);
    if (!renderSystem)
        return;

    renderSystem->bind(frameInfo.commandBuffer);

    uint32_t segmentIndex = m_BufferPool.getChunkBufferIndex(chunkId);
    m_BufferPool.bindBuffersForType(frameInfo.commandBuffer, type, segmentIndex);

    RenderBatch& batch = getOrCreateBatch(chunkId);

    updateInstanceBuffer(batch, objects);
    // 2.7 绑定实例缓冲区
    if (batch.instanceBuffer) {
        VkBuffer instanceBuffer = batch.instanceBuffer->getBuffer();
        VkDeviceSize offsets[] = { 0 };
        vkCmdBindVertexBuffers(frameInfo.commandBuffer, 1, 1, &instanceBuffer, offsets);
    }

    // ====================== 第四步：绑定描述符和推送常量 ======================
    // 2.8 绑定全局描述符集（摄像机、光照等）
    if (frameInfo.globalDescriptorSet != VK_NULL_HANDLE) {
        vkCmdBindDescriptorSets(
            frameInfo.commandBuffer,
            VK_PIPELINE_BIND_POINT_GRAPHICS,
            renderSystem->getPipelineLayout(),
            0, 1, &frameInfo.globalDescriptorSet,
            0, nullptr
        );
    }
    uint32_t instanceCount = static_cast<uint32_t>(objects.size());
    m_BufferPool.drawChunk(frameInfo.commandBuffer, chunkId, instanceCount);

}

RenderManager::RenderBatch& RenderManager::getOrCreateBatch(uint32_t chunkId)
{
    auto it = m_renderBatches.find(chunkId);
    if (it == m_renderBatches.end()) {
        // 创建新的渲染批次
        RenderBatch newBatch;
        newBatch.modelType = m_BufferPool.getChunkType(chunkId);
        newBatch.needsUpdate = true;
        newBatch.bufferCapacity = 0;
        newBatch.instanceCount = 0;

        m_renderBatches[chunkId] = std::move(newBatch);

        qDebug() << "Created new render batch for chunk:" << chunkId;
    }

    return m_renderBatches[chunkId];
}

void RenderManager::updateInstanceBuffer(RenderBatch& batch, const std::vector<Object*>& objects)
{
    if (!needInstanceBufferUpdate(batch, objects))
    {
        return;
    }

    size_t requiredSize = objects.size() * sizeof(InstanceData);

    if (!batch.instanceBuffer || batch.bufferCapacity < requiredSize)
    {
        allocateInstanceBuffer(batch, requiredSize);
    }

    // 3.4 构建实例数据
    std::vector<InstanceData> instanceData;
    instanceData.reserve(objects.size());

    for (Object* obj : objects) {
        if (obj) {
            InstanceData data;
            data.transform = obj->getTransform().mat4f();
            data.color = obj->getColor();
            data.padding = 0.0f;
            instanceData.push_back(data);

            // 清除对象的更新标记
            obj->clearUpdateFlags();
        }
    }

    // 3.5 上传实例数据到GPU
    if (!instanceData.empty()) {
        batch.instanceBuffer->map();
        batch.instanceBuffer->writeToBuffer(instanceData.data(),
            instanceData.size() * sizeof(InstanceData));
        batch.instanceBuffer->unmap();
    }

    // 3.6 更新批次状态
    batch.objects = objects;
    batch.instanceCount = static_cast<uint32_t>(objects.size());
    batch.needsUpdate = false;

    qDebug() << "Updated instance buffer with" << objects.size() << "instances";
}

bool RenderManager::needInstanceBufferUpdate(const RenderBatch& batch, const std::vector<Object*>& objects)
{
    // 检查批次是否标记为需要更新
    if (batch.needsUpdate) {
        return true;
    }

    // 检查对象数量是否变化
    if (batch.objects.size() != objects.size()) {
        return true;
    }

    // 检查对象是否有变化或更新
    for (size_t i = 0; i < objects.size(); ++i) {
        if (i >= batch.objects.size() ||
            batch.objects[i] != objects[i] ||
            (objects[i] && objects[i]->needsUpdate())) {
            return true;
        }
    }

    return false;
}

void RenderManager::allocateInstanceBuffer(RenderBatch& batch, size_t requiredSize)
{
    // 分配比需要的大一些的缓冲区，避免频繁重新分配
    size_t newCapacity = qMax(requiredSize * 2, size_t(100 * sizeof(InstanceData)));

    try {
        batch.instanceBuffer = std::make_unique<VMABuffer>(
            m_device,
            sizeof(InstanceData),
            newCapacity / sizeof(InstanceData),
            VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
            VMA_MEMORY_USAGE_CPU_TO_GPU
        );
        batch.bufferCapacity = newCapacity;

        qDebug() << "Allocated instance buffer with capacity:" << (newCapacity / sizeof(InstanceData));
    }
    catch (const std::exception& e) {
        qDebug() << "Failed to allocate instance buffer:" << e.what();
        batch.instanceBuffer.reset();
        batch.bufferCapacity = 0;
    }
}

RenderSystem* RenderManager::getRenderSystemByType(ModelType type)
{
    switch (type)
    {
    case ModelType::Point:
    return m_pointRenderSystem.get();

    case ModelType::Line:
    return m_lineRenderSystem.get();

    case ModelType::Polygon:
    return m_polygonRenderSystem.get();

    default:
    return nullptr;
    }
}
