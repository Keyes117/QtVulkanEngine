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

uint32_t RenderManager::allocateRenderBuffer(const Object::Builder& builder)
{
    return m_BufferPool.allocateBuffer(builder);

}

void RenderManager::renderObjects(const std::vector<std::shared_ptr<Object>>& objects, FrameInfo& frameInfo)
{
    auto startTime = std::chrono::high_resolution_clock::now();

    m_renderStats = RenderStatistics{};


    auto spatialBatches = createSpatialBatches(objects);

    auto batchingEndTime = std::chrono::high_resolution_clock::now();
    m_renderStats.batchingTime = std::chrono::duration<float, std::chrono::milliseconds::period>(
        batchingEndTime - startTime
    ).count();

    if (spatialBatches.empty())
        return;

    //optimizeBatches(spatialBatches);

    for (auto& batch : spatialBatches)
    {
        renderSpatialBatch(batch, frameInfo);
    }

    auto renderEndTime = std::chrono::high_resolution_clock::now();
    m_renderStats.renderingTime = std::chrono::duration<float, std::chrono::milliseconds::period>(
        renderEndTime - batchingEndTime
    ).count();

    updateRenderStatistics(spatialBatches);

    //if (objects.empty())
    //    return;

    ////按Chunk分组对象
    //std::unordered_map<uint32_t, std::vector<std::shared_ptr<Object>>> objectsByChunk;
    //devideObjectByChunks(objectsByChunk, objects);

    //renderObjectsByChunk(objectsByChunk, frameInfo);

}

void RenderManager::updateChunkObjectMapping(const std::vector<std::shared_ptr<Object>>& objects)
{
    m_chunkIdToObjectMap.clear();
    for (const auto& obj : objects) {
        if (obj && obj->getChunkID() != 0) {
            m_chunkIdToObjectMap[obj->getChunkID()] = obj;
        }
    }
}

void RenderManager::updateRenderStatistics(const std::vector<SpatialRenderBatch>& batches)
{
    m_renderStats.batches = batches.size();
}

std::vector<RenderManager::SpatialRenderBatch> RenderManager::createSpatialBatches(const std::vector<std::shared_ptr<Object>>& visibleObjects)
{
    std::map<std::pair<ModelType, uint32_t>, SpatialRenderBatch> segmentBatches;

    for (auto& object : visibleObjects)
    {
        if (!object || !object->getModel() || object->getChunkID() == 0)
            continue;

        ModelType type = object->getModel()->type();
        uint32_t chunkId = object->getChunkID();
        uint32_t segmentIndex = m_BufferPool.getChunkBufferIndex(chunkId);

        if (segmentIndex == UINT32_MAX)
            continue;

        auto key = std::make_pair(type, segmentIndex);
        auto& batch = segmentBatches[key];

        if (batch.objects.empty())
        {
            batch.type = type;
            batch.combinedBounds = AABB(
                std::numeric_limits<float>::max(),
                std::numeric_limits<float>::max(),
                std::numeric_limits<float>::lowest(),
                std::numeric_limits<float>::lowest()
            );

        }

        batch.objects.push_back(object);
        batch.chunkIds.push_back(chunkId);

        AABB objectBounds = object->getBoundingBox();
        batch.combinedBounds.minX = qMin(batch.combinedBounds.minX, objectBounds.minX);
        batch.combinedBounds.minY = qMin(batch.combinedBounds.minY, objectBounds.minY);
        batch.combinedBounds.maxX = qMax(batch.combinedBounds.maxX, objectBounds.maxX);
        batch.combinedBounds.maxY = qMax(batch.combinedBounds.maxY, objectBounds.maxY);

    }

    std::vector<SpatialRenderBatch> result;
    result.reserve(segmentBatches.size());

    for (auto& [key, batch] : segmentBatches)
    {
        if (!batch.objects.empty())
        {
            result.push_back(std::move(batch));
        }
    }

    return result;
}

void RenderManager::optimizeBatches(std::vector<SpatialRenderBatch>& batches)
{
    // 1. 合并小批次
    for (size_t i = 0; i < batches.size(); ++i) {
        for (size_t j = i + 1; j < batches.size(); ) {
            if (batches[i].type == batches[j].type &&
                batches[i].chunkIds.size() + batches[j].chunkIds.size() <= 20) {

                // 合并批次j到批次i
                batches[i].objects.insert(batches[i].objects.end(),
                    batches[j].objects.begin(), batches[j].objects.end());
                batches[i].chunkIds.insert(batches[i].chunkIds.end(),
                    batches[j].chunkIds.begin(), batches[j].chunkIds.end());

                // 扩展边界框
                AABB& bounds1 = batches[i].combinedBounds;
                const AABB& bounds2 = batches[j].combinedBounds;
                bounds1.minX = qMin(bounds1.minX, bounds2.minX);
                bounds1.minY = qMin(bounds1.minY, bounds2.minY);
                bounds1.maxX = qMax(bounds1.maxX, bounds2.maxX);
                bounds1.maxY = qMax(bounds1.maxY, bounds2.maxY);

                // 移除已合并的批次
                batches.erase(batches.begin() + j);
            }
            else {
                ++j;
            }
        }
    }
    // TODO: 可以添加更多优化策略
}

void RenderManager::renderSpatialBatch(const SpatialRenderBatch& batch, FrameInfo& frameInfo)
{
    if (batch.objects.empty() || batch.chunkIds.empty()) return;

    ModelType type = batch.type;
    auto* renderSystem = getRenderSystemByType(type);
    if (!renderSystem) return;

    // 1. 绑定pipeline（减少状态切换）
    renderSystem->bind(frameInfo.commandBuffer);
    m_renderStats.stateChanges++;

    // 2. 绑定全局资源
    bindGlobalResources(frameInfo.commandBuffer, frameInfo.globalDescriptorSet,
        renderSystem->getPipelineLayout());



    // 3. 创建合并的绘制调用
    auto mergedCalls = createMergedDrawCalls(batch.chunkIds, type);

    // 4. 执行合并绘制
    for (const auto& drawCall : mergedCalls) {
        executeMergedDrawCall(drawCall, batch.objects.size(), frameInfo);
    }

    // 输出大批次统计
    if (batch.objects.size() > 1000) {
        qDebug() << "Large batch rendered - Objects:" << batch.objects.size()
            << "Chunks:" << batch.chunkIds.size()
            << "Merged calls:" << mergedCalls.size();
    }
}

std::vector<RenderManager::MergedDrawCall> RenderManager::createMergedDrawCalls(const std::vector<uint32_t>& chunkIds, ModelType type)
{
    std::vector<MergedDrawCall> result;

    // 按segment分组chunk（RenderManager的渲染优化逻辑）
    std::unordered_map<uint32_t, std::vector<uint32_t>> segmentGroups;

    for (uint32_t chunkId : chunkIds) {
        uint32_t segmentIndex = m_BufferPool.getChunkBufferIndex(chunkId);
        if (segmentIndex != UINT32_MAX) {
            segmentGroups[segmentIndex].push_back(chunkId);
        }
    }

    result.reserve(segmentGroups.size());

    for (auto& [segmentIndex, chunks] : segmentGroups) {
        MergedDrawCall call;
        call.segmentIndex = segmentIndex;
        call.chunks = std::move(chunks);
        call.type = type;

        // 计算总顶点数和索引数
        for (uint32_t chunkId : call.chunks) {
            const auto* chunk = m_BufferPool.getChunk(chunkId);
            if (chunk) {
                call.totalVertices += chunk->vertexCount;
                call.totalIndices += chunk->indexCount;
            }
        }

        result.push_back(std::move(call));
    }

    // 按chunk数量排序，优先渲染大合并调用
    std::sort(result.begin(), result.end(),
        [](const MergedDrawCall& a, const MergedDrawCall& b) {
            return a.chunks.size() > b.chunks.size();
        });

    return result;
}

void RenderManager::executeMergedDrawCall(const MergedDrawCall& drawCall, uint32_t instanceCount, FrameInfo& frameInfo)
{
    if (drawCall.chunks.empty())
        return;

    m_BufferPool.bindBuffersForType(frameInfo.commandBuffer, drawCall.type, drawCall.segmentIndex);
    m_renderStats.bufferBinds++;

    auto* renderSystem = getRenderSystemByType(drawCall.type);
    if (!renderSystem) return;

    for (uint32_t chunkId : drawCall.chunks)
    {
        renderSingleChunkWithTransform(chunkId, renderSystem, frameInfo);

    }
}

void RenderManager::renderSingleChunkWithTransform(uint32_t chunkId, RenderSystem* renderSystem, FrameInfo& frameInfo)
{
    auto object = m_chunkIdToObjectMap[chunkId];
    if (!object)
        return;;

    if (!renderSystem)
        return;

    SimplePushConstantData pushData{};
    pushData.modelMatrix = object->getTransform().mat4f();
    pushData.color = object->getColor();

    vkCmdPushConstants(
        frameInfo.commandBuffer,
        renderSystem->getPipelineLayout(),
        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
        0,
        sizeof(SimplePushConstantData),
        &pushData
    );

    m_BufferPool.drawChunk(frameInfo.commandBuffer, chunkId);
    m_renderStats.drawCalls++;
    m_renderStats.pushConstantUpdates++;
}

void RenderManager::bindGlobalResources(VkCommandBuffer commandBuffer, VkDescriptorSet globalDescriptorSet, VkPipelineLayout pipelineLayout)
{
    if (globalDescriptorSet != VK_NULL_HANDLE) {
        vkCmdBindDescriptorSets(
            commandBuffer,
            VK_PIPELINE_BIND_POINT_GRAPHICS,
            pipelineLayout,
            0, 1, &globalDescriptorSet,
            0, nullptr
        );
    }
}

void RenderManager::devideObjectByChunks(std::unordered_map<uint32_t, std::vector<std::shared_ptr<Object>>>& objectsByChunk, const std::vector<std::shared_ptr<Object>>& objects)
{
    for (auto& obj : objects) {
        if (obj && obj->getChunkID() != 0) {
            objectsByChunk[obj->getChunkID()].push_back(obj);
        }
    }
}

void RenderManager::renderObjectsByChunk(const std::unordered_map<uint32_t, std::vector<std::shared_ptr<Object>>>& objectsByChunk, FrameInfo& frameInfo)
{
    for (const auto& [chunkId, chunkObjects] : objectsByChunk)
    {
        if (chunkId != 0 && !chunkObjects.empty())
        {
            renderChunkBatch(chunkId, chunkObjects, frameInfo);
        }
    }
}

void RenderManager::renderChunkBatch(uint32_t chunkId, const std::vector<std::shared_ptr<Object>>& objects, FrameInfo& frameInfo)
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

    //RenderBatch& batch = getOrCreateBatch(chunkId);

    //updateInstanceBuffer(batch, objects);
    //// 2.7 绑定实例缓冲区
    //if (batch.instanceBuffer) {
    //    VkBuffer instanceBuffer = batch.instanceBuffer->getBuffer();
    //    VkDeviceSize offsets[] = { 0 };
    //    vkCmdBindVertexBuffers(frameInfo.commandBuffer, 1, 1, &instanceBuffer, offsets);
    //}

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

void RenderManager::updateInstanceBuffer(RenderBatch& batch, const std::vector<std::shared_ptr<Object>>& objects)
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

    for (auto& obj : objects) {
        if (obj) {


            InstanceData data;

            QMatrix4x4 transform = obj->getTransform().mat4f();
            memcpy(data.transform, transform.constData(), 16 * sizeof(float));

            QVector3D objColor = obj->getColor();
            data.color[0] = objColor.x();
            data.color[1] = objColor.y();
            data.color[2] = objColor.z();
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

bool RenderManager::needInstanceBufferUpdate(const RenderBatch& batch, const std::vector<std::shared_ptr<Object>>& objects)
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

void RenderManager::printRenderStats() const
{
    qDebug() << "=== Render Manager Stats ===";
    qDebug() << "Render batches:" << m_renderStats.batches;
    qDebug() << "Draw calls:" << m_renderStats.drawCalls;
    qDebug() << "State changes:" << m_renderStats.stateChanges;
    qDebug() << "Buffer binds:" << m_renderStats.bufferBinds;
    qDebug() << "Push constant updates:" << m_renderStats.pushConstantUpdates;  // 新增
    qDebug() << "Batching time:" << m_renderStats.batchingTime << "ms";
    qDebug() << "Rendering time:" << m_renderStats.renderingTime << "ms";

    if (m_renderStats.batches > 0) {
        qDebug() << "Avg draw calls per batch:" <<
            (float(m_renderStats.drawCalls) / m_renderStats.batches);
    }

    if (m_renderStats.drawCalls > 0) {
        qDebug() << "Avg push constants per draw call:" <<
            (float(m_renderStats.pushConstantUpdates) / m_renderStats.drawCalls);
    }
}
