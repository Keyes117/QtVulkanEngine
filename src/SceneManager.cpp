#include "SceneManager.h"
#include <chrono>

SceneManager::SceneManager(MyVulkanWindow& window,
    Device& device,
    VkDescriptorSetLayout globalSetLayout,
    const AABB& worldBounds) :
    m_objectManager(device, worldBounds),
    m_renderManager(window, device, globalSetLayout),
    m_gpuCuller(std::make_unique<GpuFrustumCuller>(device))
{
    m_objectManager.setObjectUpdateCallback(
        std::bind(&SceneManager::onObjectChanged,
            this,
            std::placeholders::_1));


}

Object::ObjectID SceneManager::addObject(const Object::Builder& builder)
{

    // 首先renderManager 为 Object 分配缓冲
    uint32_t chunkId = m_renderManager.allocateRenderBuffer(builder);

    // 然后ObjectManager 将Object 实例化并且进行管理 
    Object::ObjectID objectId = m_objectManager.createObject(builder, chunkId);
    //TODO: 这段逻辑 也许直接放在createObject中就行

    return objectId;
}

void SceneManager::render(FrameInfo& frameInfo)
{
    auto startTime = std::chrono::high_resolution_clock::now();


    AABB frustumBounds = frameInfo.camera.getFrustumBound2D();
    std::vector<std::shared_ptr<Object>> visibleObjects;
    std::vector<std::shared_ptr<Object>> allObjects = m_objectManager.getAllObjects();

    if (!m_gpuCuller->isInit())
        m_gpuCuller->initialize(allObjects.size());

    std::vector<uint32_t> visibleChunkIds = m_gpuCuller->performCulling(allObjects, frameInfo.camera);

    auto cullingEndTime = std::chrono::high_resolution_clock::now();
    m_stats.cullingTime = std::chrono::duration<float, std::chrono::milliseconds::period>(
        cullingEndTime - startTime
    ).count();

    // 根据ChunkID筛选对象
    visibleObjects.reserve(visibleChunkIds.size());
    std::unordered_map<uint32_t, std::shared_ptr<Object>> chunkToObject;
    for (const auto& obj : allObjects) {
        if (obj && obj->getChunkID() != 0) {
            chunkToObject[obj->getChunkID()] = obj;
        }
    }

    for (uint32_t chunkId : visibleChunkIds) {
        auto it = chunkToObject.find(chunkId);
        if (it != chunkToObject.end()) {
            visibleObjects.push_back(it->second);
        }
    }

    m_stats.useGPUCulling = true;



    if (visibleObjects.empty())
    {
        m_stats.visibleObjects = 0;
        updateRenderStats(visibleObjects);
        printRenderStats();
        return;
    }

    // 按chunkId分组
    std::unordered_map<uint32_t, std::vector<std::shared_ptr<Object>>> objectsByChunk;
    for (auto& obj : visibleObjects) {
        if (obj && obj->getModel()->type() == ModelType::Line) {
            uint32_t chunkId = obj->getChunkID();
            if (chunkId != 0) {
                objectsByChunk[chunkId].push_back(obj);
            }
        }
    }
    // 批量渲染
    for (const auto& iter : objectsByChunk)
    {
        m_renderManager.renderObjects(iter.second, frameInfo);
    }
    auto renderEndTime = std::chrono::high_resolution_clock::now();
    m_stats.renderTime = std::chrono::duration<float, std::chrono::milliseconds::period>(
        renderEndTime - cullingEndTime
    ).count();

    updateRenderStats(visibleObjects);
    printRenderStats();


    //std::vector<std::shared_ptr<Object>> objectsToRender;
    //auto chunks = m_renderManager.getVisibleChunks(frameInfo.camera, ModelType::Line);

    //for (auto chunkId : chunks)
    //{

    //    const BufferPool::Chunk* chunk = m_renderManager.getChunk(chunkId);
    //    std::vector<std::shared_ptr<Object>> chunkObjects = m_objectManager.getVisibleObjects(chunk->bounds);

    //    for (auto& obj : chunkObjects) {
    //        if (obj &&
    //            obj->getChunkID() == chunkId &&
    //            obj->getModel()->type() == ModelType::Line)
    //        {
    //            objectsToRender.push_back(obj);
    //        }
    //    }

    //    if (!objectsToRender.empty()) {
    //        // 3.1 调用RenderManager进行批量渲染
    //        m_renderManager.renderObjects(objectsToRender, frameInfo);
    //    }
    //}
}

void SceneManager::onObjectChanged(const std::shared_ptr<Object>& object)
{

}

void SceneManager::updateRenderStats(const std::vector<std::shared_ptr<Object>>& visibleObjects)
{
    m_stats.totalObjects = m_objectManager.getAllObjects().size();
    m_stats.visibleObjects = visibleObjects.size();
}

void SceneManager::printRenderStats() const
{
    qDebug() << "=== Scene Manager Stats ===";
    qDebug() << "Total objects:" << m_stats.totalObjects;
    qDebug() << "Visible objects:" << m_stats.visibleObjects;
    qDebug() << "Culling time:" << m_stats.cullingTime << "ms";
    qDebug() << "Render time:" << m_stats.renderTime << "ms";
    qDebug() << "Culling efficiency:" <<
        (m_stats.totalObjects > 0 ?
            100.0f * (1.0f - float(m_stats.visibleObjects) / m_stats.totalObjects) : 0.0f) << "%";

    // 获取RenderManager的统计信息
    m_renderManager.printRenderStats();
}


