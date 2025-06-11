#include "SceneManager.h"

SceneManager::SceneManager(MyVulkanWindow& window,
    Device& device,
    VkDescriptorSetLayout globalSetLayout,
    const AABB& worldBounds) :
    m_objectManager(),
    m_renderManager(window, device, globalSetLayout),
    m_worldBounds(worldBounds)
{
}

ObjectManager::ObjectHandle SceneManager::addObject(const Object::Builder& builder)
{
    uint32_t chunkId = m_renderManager.allocateRenderBuffer(builder);
    auto handle = m_objectManager.createObject(builder, chunkId);

    AABB bounds = m_objectManager.getBoundingBox(handle);
    m_spatialIndex.insert(handle, bounds);
    return handle;
}

void SceneManager::removeObject(ObjectManager::ObjectHandle handle)
{
    if (!m_objectManager.isValid(handle))
        return;

    m_spatialIndex.remove(handle);

    m_objectManager.destroyObject(handle);

}

void SceneManager::updateObjectTransform(ObjectManager::ObjectHandle handle, const QMatrix4x4& transform)
{
    if (!m_objectManager.isValid(handle))
        return;

    m_objectManager.setTransform(handle, transform);

    updateSpatialIndexForObject(handle);

}

void SceneManager::updateObjectColor(ObjectManager::ObjectHandle handle, const QVector3D& color)
{
    if (!m_objectManager.isValid(handle))
        return;

    m_objectManager.setColor(handle, color);
}

void SceneManager::render(FrameInfo& frameInfo)
{
    m_renderManager.testRenderSegment(frameInfo);
    return;

    auto renderStart = std::chrono::high_resolution_clock::now();

    m_frameCounter++;

    auto activeObjects = m_objectManager.getAllActiveObjects();

    const auto& dataPool = m_objectManager.getDataPool();
    auto chunkGroup = m_objectManager.groupByChunk();

    std::unordered_map<uint32_t, std::vector<uint32_t>> chunkToIndices;
    for (const auto& [chunkId, handles] : chunkGroup)
    {
        for (const auto& handle : handles)
        {
            if (handle.valid && chunkId != 0)
            {
                chunkToIndices[chunkId].push_back(handle.index);
            }
        }
    }

    m_renderManager.renderFromDataPool(dataPool, chunkToIndices, frameInfo);

    m_objectManager.clearUpdateFlags();

    m_sceneStats.totalObjects = static_cast<uint32_t>(activeObjects.size());
    m_sceneStats.activeObjects = static_cast<uint32_t>(activeObjects.size());
    m_sceneStats.visibleObjects = static_cast<uint32_t>(activeObjects.size());

    auto renderEnd = std::chrono::high_resolution_clock::now();
    auto renderTime = std::chrono::duration<float, std::milli>(renderEnd - renderStart).count();

    qDebug() << "Render frame time:" << renderTime << "ms, Objects:" << m_sceneStats.totalObjects;
    /*}
        if (m_frameCounter % 60 == 0) {
           */

}

void SceneManager::renderWithCulling(FrameInfo& frameInfo, const Camera& camera)
{
    auto renderStart = std::chrono::high_resolution_clock::now();

    // 执行裁剪
    auto cullingStart = std::chrono::high_resolution_clock::now();
    auto visibleObjects = performCulling(camera);
    auto cullingEnd = std::chrono::high_resolution_clock::now();

    m_sceneStats.cullingTime = std::chrono::duration<float, std::milli>(cullingEnd - cullingStart).count();

    // 准备可见对象的渲染数据
    std::unordered_map<uint32_t, std::vector<uint32_t>> chunkToIndices;

    for (const auto& handle : visibleObjects) {
        if (handle.valid) {
            uint32_t chunkId = m_objectManager.getChunkId(handle);
            if (chunkId != 0) {
                chunkToIndices[chunkId].push_back(handle.index);
            }
        }
    }

    // 渲染可见对象
    const auto& dataPool = m_objectManager.getDataPool();
    m_renderManager.renderFromDataPool(dataPool, chunkToIndices, frameInfo);

    // 清除更新标志
    m_objectManager.clearUpdateFlags();

    // 更新统计
    m_sceneStats.totalObjects = static_cast<uint32_t>(m_objectManager.getActiveObjectCount());
    m_sceneStats.visibleObjects = static_cast<uint32_t>(visibleObjects.size());
    m_sceneStats.culledObjects = m_sceneStats.totalObjects - m_sceneStats.visibleObjects;

    auto renderEnd = std::chrono::high_resolution_clock::now();
    auto renderTime = std::chrono::duration<float, std::milli>(renderEnd - renderStart).count();

    if (m_frameCounter % 60 == 0) {
        qDebug() << "Render with culling - Frame:" << renderTime << "ms, Culling:" << m_sceneStats.cullingTime
            << "ms, Visible:" << m_sceneStats.visibleObjects << "/" << m_sceneStats.totalObjects;
    }
}

std::vector<ObjectManager::ObjectHandle> SceneManager::getVisibleObjects(const AABB& frustum) const
{
    return m_spatialIndex.query(frustum);
}

std::vector<ObjectManager::ObjectHandle> SceneManager::getObjectsInRadius(const QVector3D& center, float radius) const
{
    return m_spatialIndex.queryRadius(center, radius);
}

std::vector<ObjectManager::ObjectHandle> SceneManager::getObjectsInChunk(uint32_t chunkId) const
{
    return m_objectManager.getObjectsByChunk(chunkId);
}




void SceneManager::SpatialIndex::insert(ObjectManager::ObjectHandle handle, const AABB& bounds)
{
    auto cells = getCellsForBounds(bounds);

    for (uint64_t cellHash : cells)
    {
        m_spatialGrid[cellHash].objects.insert(handle);
    }

    m_objectToCells[handle] = cells;
}

void SceneManager::SpatialIndex::remove(ObjectManager::ObjectHandle handle)
{
    auto it = m_objectToCells.find(handle);
    if (it == m_objectToCells.end())
        return;

    for (uint64_t cellHash : it->second)
    {
        auto gridIt = m_spatialGrid.find(cellHash);
        if (gridIt != m_spatialGrid.end())
        {
            gridIt->second.objects.erase(handle);
            if (gridIt->second.objects.empty())
            {
                m_spatialGrid.erase(gridIt);
            }
        }
    }

    m_objectToCells.erase(it);
}

void SceneManager::SpatialIndex::update(ObjectManager::ObjectHandle handle, const AABB& newBounds)
{
    remove(handle);
    insert(handle, newBounds);
}

std::vector<ObjectManager::ObjectHandle> SceneManager::SpatialIndex::query(const AABB& bounds) const
{
    std::unordered_set<ObjectManager::ObjectHandle> result;
    auto cells = getCellsForBounds(bounds);

    for (uint64_t cellHash : cells)
    {
        auto it = m_spatialGrid.find(cellHash);
        if (it != m_spatialGrid.end())
        {
            for (const auto& handle : it->second.objects)
            {
                result.insert(handle);
            }
        }
    }

    return std::vector<ObjectManager::ObjectHandle>(result.begin(), result.end());
}

std::vector<ObjectManager::ObjectHandle> SceneManager::SpatialIndex::queryRadius(const QVector3D& center, float radius) const
{
    AABB queryBounds(
        center.x() - radius, center.y() - radius,
        center.x() + radius, center.y() + radius
    );

    auto candidates = query(queryBounds);
    std::vector<ObjectManager::ObjectHandle> result;

    for (const auto& handle : candidates)
    {
        result.push_back(handle);
    }

    return result;
}

uint64_t SceneManager::SpatialIndex::hash(const QVector3D& pos) const
{
    int32_t x = static_cast<int32_t>(pos.x() / m_cellSize);
    int32_t y = static_cast<int32_t>(pos.y() / m_cellSize);

    return (static_cast<uint64_t>(x) << 32) | static_cast<uint64_t>(y);
}

std::vector<uint64_t> SceneManager::SpatialIndex::getCellsForBounds(const AABB& bounds) const
{
    std::vector<uint64_t> cells;

    int32_t minX = static_cast<int32_t>(bounds.minX / m_cellSize);
    int32_t maxX = static_cast<int32_t>(bounds.maxX / m_cellSize);
    int32_t minY = static_cast<int32_t>(bounds.minY / m_cellSize);
    int32_t maxY = static_cast<int32_t>(bounds.maxY / m_cellSize);

    for (int32_t x = minX; x <= maxX; ++x) {
        for (int32_t y = minY; y <= maxY; ++y) {
            uint64_t cellHash = (static_cast<uint64_t>(x) << 32) | static_cast<uint64_t>(y);
            cells.push_back(cellHash);
        }
    }

    return cells;
}

void SceneManager::updateSpatialIndex()
{
    auto updateStart = std::chrono::high_resolution_clock::now();

    // 获取需要更新的对象
    auto updatedObjects = m_objectManager.getUpdatedObjects();

    for (const auto& handle : updatedObjects) {
        updateSpatialIndexForObject(handle);
    }

    auto updateEnd = std::chrono::high_resolution_clock::now();
    m_sceneStats.spatialUpdateTime = std::chrono::duration<float, std::milli>(updateEnd - updateStart).count();
}

void SceneManager::optimizeMemory()
{
    // 压缩对象数据
    m_objectManager.compactMemory();

    // 重建空间索引
    m_spatialIndex.clear();
    auto allObjects = m_objectManager.getAllActiveObjects();

    for (const auto& handle : allObjects) {
        AABB bounds = m_objectManager.getBoundingBox(handle);
        m_spatialIndex.insert(handle, bounds);
    }

    qDebug() << "Memory optimized. Active objects:" << allObjects.size();
}

std::vector<ObjectManager::ObjectHandle> SceneManager::performCulling(const Camera& camera)
{
    std::vector<ObjectManager::ObjectHandle> visibleObjects;

    if (!m_cullingConfig.enableFrustumCulling && !m_cullingConfig.enableDistanceCulling) {
        return m_objectManager.getAllActiveObjects();
    }

    // 计算视锥体
    AABB viewFrustum = camera.getViewFrustum();

    // 获取视锥体内的候选对象
    auto candidates = m_spatialIndex.query(viewFrustum);

    QVector3D cameraPos = camera.getPosition();

    for (const auto& handle : candidates) {
        bool visible = true;

        // 距离裁剪
        if (m_cullingConfig.enableDistanceCulling) {
            AABB bounds = m_objectManager.getBoundingBox(handle);
            float distance = calculateDistance(bounds, cameraPos);

            if (distance > m_cullingConfig.maxRenderDistance) {
                visible = false;
            }
        }

        // 视锥体裁剪（这里简化为AABB相交测试）
        if (visible && m_cullingConfig.enableFrustumCulling) {
            AABB bounds = m_objectManager.getBoundingBox(handle);
            if (!bounds.overlaps(viewFrustum)) {
                visible = false;
            }
        }

        if (visible) {
            visibleObjects.push_back(handle);
        }
    }

    return visibleObjects;
}

void SceneManager::updateLOD(const QVector3D& cameraPos)
{
}

void SceneManager::updateSpatialIndexForObject(ObjectManager::ObjectHandle handle)
{
    if (!m_objectManager.isValid(handle))
        return;

    AABB bounds = m_objectManager.getBoundingBox(handle);
    m_spatialIndex.update(handle, bounds);
}

float SceneManager::calculateDistance(const AABB& bounds, const QVector3D& point) const
{
    // 计算点到AABB的距离
    QVector3D center((bounds.minX + bounds.maxX) * 0.5f, (bounds.minY + bounds.maxY) * 0.5f, 0.0f);
    return (center - point).length();
}

uint8_t SceneManager::calculateLODLevel(float distance) const
{
    return 0;
}


