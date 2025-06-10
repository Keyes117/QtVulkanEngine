#include "HierarchicalSpatialIndex.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <limits>

// SpatialCell 实现
void SpatialCell::addObject(const std::shared_ptr<Object>& obj)
{
    if (!obj || !obj->getModel()) return;
    
    // 根据对象类型分类存储
    if (obj->isStatic()) {
        staticObjects.push_back(obj);
    } else {
        dynamicObjects.push_back(obj);
    }
    
    objects.push_back(obj);
    isDirty = true;
}

void SpatialCell::removeObject(const std::shared_ptr<Object>& obj)
{
    if (!obj) return;
    
    // 从所有容器中移除
    auto removeFromVector = [](std::vector<std::shared_ptr<Object>>& vec, const std::shared_ptr<Object>& obj) {
        auto it = std::find(vec.begin(), vec.end(), obj);
        if (it != vec.end()) {
            vec.erase(it);
            return true;
        }
        return false;
    };
    
    removeFromVector(objects, obj);
    removeFromVector(staticObjects, obj);
    removeFromVector(dynamicObjects, obj);
    
    isDirty = true;
}

void SpatialCell::updateObject(const std::shared_ptr<Object>& obj)
{
    if (!obj) return;
    
    // 检查对象是否仍在此单元范围内
    AABB objBounds = obj->getBoundingBox();
    if (!bounds.overlaps(objBounds)) {
        removeObject(obj);
    }
    
    isDirty = true;
}

// QuadTreeNode 实现
bool QuadTreeNode::needsSplit() const
{
    return objects.size() > SpatialIndexConfig::MAX_OBJECTS_PER_NODE &&
           level < SpatialIndexConfig::MAX_DEPTH &&
           objects.size() >= SpatialIndexConfig::MIN_OBJECTS_FOR_SPLIT;
}

bool QuadTreeNode::canMerge() const
{
    if (isLeaf()) return false;
    
    size_t totalChildObjects = 0;
    for (const auto& child : children) {
        if (child) {
            totalChildObjects += child->objectCount;
            if (!child->isLeaf()) return false; // 只有叶子节点的子节点才能合并
        }
    }
    
    return totalChildObjects < SpatialIndexConfig::MIN_OBJECTS_FOR_SPLIT;
}

void QuadTreeNode::split()
{
    if (level >= SpatialIndexConfig::MAX_DEPTH || isLeaf() == false) {
        return;
    }
    
    float halfWidth = (bounds.maxX - bounds.minX) * 0.5f;
    float halfHeight = (bounds.maxY - bounds.minY) * 0.5f;
    float centerX = bounds.minX + halfWidth;
    float centerY = bounds.minY + halfHeight;
    
    // 创建子节点
    children[0] = std::make_unique<QuadTreeNode>(
        AABB(bounds.minX, centerY, centerX, bounds.maxY), level + 1);      // 左上
    children[1] = std::make_unique<QuadTreeNode>(
        AABB(centerX, centerY, bounds.maxX, bounds.maxY), level + 1);      // 右上
    children[2] = std::make_unique<QuadTreeNode>(
        AABB(bounds.minX, bounds.minY, centerX, centerY), level + 1);      // 左下
    children[3] = std::make_unique<QuadTreeNode>(
        AABB(centerX, bounds.minY, bounds.maxX, centerY), level + 1);      // 右下
    
    redistributeObjects();
}

void QuadTreeNode::merge()
{
    if (isLeaf()) return;
    
    // 收集所有子节点的对象
    std::vector<std::shared_ptr<Object>> allObjects;
    for (auto& child : children) {
        if (child) {
            for (auto& obj : child->objects) {
                allObjects.push_back(obj);
            }
        }
    }
    
    // 清除子节点
    for (auto& child : children) {
        child.reset();
    }
    
    // 将所有对象移到当前节点
    objects = std::move(allObjects);
    objectCount = objects.size();
}

void QuadTreeNode::redistributeObjects()
{
    auto objectsToRedistribute = std::move(objects);
    objects.clear();
    objectCount = 0;
    
    for (auto& obj : objectsToRedistribute) {
        if (!obj || !obj->getModel()) continue;
        
        AABB objBounds = obj->getBoundingBox();
        std::vector<int> overlappingChildren;
        
        // 找到所有重叠的子节点
        for (int i = 0; i < 4; ++i) {
            if (children[i] && objBounds.overlaps(children[i]->bounds)) {
                overlappingChildren.push_back(i);
            }
        }
        
        if (overlappingChildren.size() == 1) {
            // 只与一个子节点重叠，放入该子节点
            int childIndex = overlappingChildren[0];
            children[childIndex]->objects.push_back(obj);
            children[childIndex]->objectCount++;
            
            // 递归检查子节点是否需要分割
            if (children[childIndex]->needsSplit()) {
                children[childIndex]->split();
            }
        } else {
            // 与多个子节点重叠，保留在当前节点
            objects.push_back(obj);
            objectCount++;
        }
    }
    
    // 更新密度信息
    float area = (bounds.maxX - bounds.minX) * (bounds.maxY - bounds.minY);
    density = area > 0 ? static_cast<float>(objectCount) / area : 0.0f;
}

bool QuadTreeNode::isQueryCacheValid(const AABB& queryBounds, uint32_t currentFrame) const
{
    const uint32_t CACHE_VALIDITY_FRAMES = 3; // 缓存有效帧数
    return (currentFrame - lastQueryFrame) <= CACHE_VALIDITY_FRAMES &&
           lastQueryBounds.minX == queryBounds.minX &&
           lastQueryBounds.minY == queryBounds.minY &&
           lastQueryBounds.maxX == queryBounds.maxX &&
           lastQueryBounds.maxY == queryBounds.maxY;
}

void QuadTreeNode::updateQueryCache(const AABB& queryBounds, uint32_t currentFrame,
                                   const std::vector<std::shared_ptr<Object>>& results)
{
    lastQueryFrame = currentFrame;
    lastQueryBounds = queryBounds;
    cachedVisibleObjects = results;
}

// HierarchicalSpatialIndex 实现
HierarchicalSpatialIndex::HierarchicalSpatialIndex(const AABB& worldBounds)
    : m_root(std::make_unique<QuadTreeNode>(worldBounds, 0))
    , m_gridCellSize(100.0f) // 默认网格大小
{
    initializeSpatialGrid();
}

void HierarchicalSpatialIndex::initializeSpatialGrid()
{
    if (!m_root) return;
    
    float worldWidth = m_root->bounds.maxX - m_root->bounds.minX;
    float worldHeight = m_root->bounds.maxY - m_root->bounds.minY;
    
    m_gridWidth = static_cast<int>(std::ceil(worldWidth / m_gridCellSize));
    m_gridHeight = static_cast<int>(std::ceil(worldHeight / m_gridCellSize));
    
    m_spatialGrid.resize(m_gridHeight);
    for (int y = 0; y < m_gridHeight; ++y) {
        m_spatialGrid[y].resize(m_gridWidth);
        for (int x = 0; x < m_gridWidth; ++x) {
            AABB cellBounds;
            cellBounds.minX = m_root->bounds.minX + x * m_gridCellSize;
            cellBounds.minY = m_root->bounds.minY + y * m_gridCellSize;
            cellBounds.maxX = cellBounds.minX + m_gridCellSize;
            cellBounds.maxY = cellBounds.minY + m_gridCellSize;
            
            uint32_t cellId = y * m_gridWidth + x;
            m_spatialGrid[y][x] = std::make_shared<SpatialCell>(cellBounds, cellId, 0);
        }
    }
}

void HierarchicalSpatialIndex::insert(const std::shared_ptr<Object>& object)
{
    if (!object || !object->getModel() || !m_root) return;
    
    insertNode(m_root.get(), object);
    m_objectIndex[object->getId()] = object;
    
    // 同时插入到空间网格
    AABB objBounds = object->getBoundingBox();
    auto [minX, minY] = getGridCoordinates(QVector2D(objBounds.minX, objBounds.minY));
    auto [maxX, maxY] = getGridCoordinates(QVector2D(objBounds.maxX, objBounds.maxY));
    
    for (int y = minY; y <= maxY; ++y) {
        for (int x = minX; x <= maxX; ++x) {
            auto cell = getCell(x, y);
            if (cell) {
                cell->addObject(object);
            }
        }
    }
}

void HierarchicalSpatialIndex::remove(const std::shared_ptr<Object>& object)
{
    if (!object || !m_root) return;
    
    removeNode(m_root.get(), object);
    m_objectIndex.erase(object->getId());
    
    // 从空间网格中移除
    AABB objBounds = object->getBoundingBox();
    auto [minX, minY] = getGridCoordinates(QVector2D(objBounds.minX, objBounds.minY));
    auto [maxX, maxY] = getGridCoordinates(QVector2D(objBounds.maxX, objBounds.maxY));
    
    for (int y = minY; y <= maxY; ++y) {
        for (int x = minX; x <= maxX; ++x) {
            auto cell = getCell(x, y);
            if (cell) {
                cell->removeObject(object);
            }
        }
    }
}

void HierarchicalSpatialIndex::update(const std::shared_ptr<Object>& object)
{
    if (!object) return;
    
    // 高效的增量更新，而不是删除后重新插入
    auto it = m_objectIndex.find(object->getId());
    if (it != m_objectIndex.end()) {
        auto oldObject = it->second.lock();
        if (oldObject && oldObject == object) {
            // 对象仍然存在，进行位置更新
            remove(object);
            insert(object);
        }
    } else {
        // 新对象，直接插入
        insert(object);
    }
}

std::vector<std::shared_ptr<Object>> HierarchicalSpatialIndex::query(const AABB& bounds)
{
    if (!m_root) return {};
    
    // 尝试使用缓存
    std::vector<std::shared_ptr<Object>> results;
    if (tryGetCachedQuery(bounds, results)) {
        m_stats.cacheHitRate += 1.0f;
        return results;
    }
    
    auto startTime = std::chrono::high_resolution_clock::now();
    
    queryNode(m_root.get(), bounds, results);
    
    auto endTime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime);
    
    // 更新统计信息
    m_stats.queryCount++;
    m_stats.avgQueryTime = (m_stats.avgQueryTime * (m_stats.queryCount - 1) + duration.count()) / m_stats.queryCount;
    
    // 缓存查询结果
    cacheQuery(bounds, results);
    
    return results;
}

std::vector<std::shared_ptr<Object>> HierarchicalSpatialIndex::queryWithLOD(
    const Camera& camera, const AABB& bounds)
{
    auto allObjects = query(bounds);
    std::vector<std::shared_ptr<Object>> lodFilteredObjects;
    
    for (auto& obj : allObjects) {
        LODLevel lod = calculateLOD(obj, camera);
        if (lod != LODLevel::CULLED) {
            lodFilteredObjects.push_back(obj);
        }
    }
    
    return lodFilteredObjects;
}

std::vector<HierarchicalSpatialIndex::SpatialGroup> 
HierarchicalSpatialIndex::querySpatialGroups(const Camera& camera)
{
    std::vector<SpatialGroup> groups;
    
    // 基于相机视锥体和距离创建空间分组
    QVector3D cameraPos = camera.getPosition();
    float viewDistance = camera.getFarPlane();
    
    AABB queryBounds;
    queryBounds.minX = cameraPos.x() - viewDistance;
    queryBounds.minY = cameraPos.z() - viewDistance;
    queryBounds.maxX = cameraPos.x() + viewDistance;
    queryBounds.maxY = cameraPos.z() + viewDistance;
    
    auto visibleObjects = query(queryBounds);
    
    // 按距离和LOD分组
    std::unordered_map<int, SpatialGroup> groupMap;
    
    for (auto& obj : visibleObjects) {
        float distance = calculateDistance(obj, camera);
        LODLevel lod = calculateLOD(obj, camera);
        
        if (lod == LODLevel::CULLED) continue;
        
        // 基于距离和LOD创建分组键
        int groupKey = static_cast<int>(lod) * 1000 + static_cast<int>(distance / 100.0f);
        
        auto& group = groupMap[groupKey];
        group.objects.push_back(obj);
        group.lodLevel = lod;
        group.distance = distance;
        
        // 更新组的包围盒
        AABB objBounds = obj->getBoundingBox();
        if (group.objects.size() == 1) {
            group.bounds = objBounds;
        } else {
            group.bounds.minX = std::min(group.bounds.minX, objBounds.minX);
            group.bounds.minY = std::min(group.bounds.minY, objBounds.minY);
            group.bounds.maxX = std::max(group.bounds.maxX, objBounds.maxX);
            group.bounds.maxY = std::max(group.bounds.maxY, objBounds.maxY);
        }
    }
    
    // 转换为向量并按优先级排序
    for (auto& [key, group] : groupMap) {
        group.priority = static_cast<uint32_t>(group.objects.size() * (4 - static_cast<int>(group.lodLevel)));
        groups.push_back(std::move(group));
    }
    
    std::sort(groups.begin(), groups.end(), [](const SpatialGroup& a, const SpatialGroup& b) {
        return a.priority > b.priority;
    });
    
    return groups;
}

LODLevel HierarchicalSpatialIndex::calculateLOD(const std::shared_ptr<Object>& object, const Camera& camera) const
{
    float distance = calculateDistance(object, camera);
    float farPlane = camera.getFarPlane();
    
    if (distance > farPlane) return LODLevel::CULLED;
    
    float normalizedDistance = distance / farPlane;
    
    if (normalizedDistance < 0.3f) return LODLevel::HIGH;
    else if (normalizedDistance < 0.6f) return LODLevel::MEDIUM;
    else if (normalizedDistance < 0.9f) return LODLevel::LOW;
    else return LODLevel::CULLED;
}

float HierarchicalSpatialIndex::calculateDistance(const std::shared_ptr<Object>& object, const Camera& camera) const
{
    if (!object || !object->getModel()) return std::numeric_limits<float>::max();
    
    AABB objBounds = object->getBoundingBox();
    QVector3D objCenter(
        (objBounds.minX + objBounds.maxX) * 0.5f,
        0.0f,
        (objBounds.minY + objBounds.maxY) * 0.5f
    );
    
    QVector3D cameraPos = camera.getPosition();
    return (objCenter - cameraPos).length();
}

void HierarchicalSpatialIndex::clear()
{
    if (m_root) {
        AABB worldBounds = m_root->bounds;
        m_root = std::make_unique<QuadTreeNode>(worldBounds, 0);
    }
    
    m_objectIndex.clear();
    
    // 清空空间网格
    for (auto& row : m_spatialGrid) {
        for (auto& cell : row) {
            if (cell) {
                cell->objects.clear();
                cell->staticObjects.clear();
                cell->dynamicObjects.clear();
            }
        }
    }
    
    // 清空LOD级别
    for (auto& lodLevel : m_lodLevels) {
        lodLevel.clear();
    }
    
    resetStatistics();
}

// 内部实现方法
void HierarchicalSpatialIndex::insertNode(QuadTreeNode* node, const std::shared_ptr<Object>& object)
{
    if (!node || !object || !object->getModel()) return;
    
    AABB objBounds = object->getBoundingBox();
    if (!node->bounds.overlaps(objBounds)) return;
    
    if (node->isLeaf()) {
        node->objects.push_back(object);
        node->objectCount++;
        
        if (node->needsSplit()) {
            node->split();
        }
    } else {
        // 尝试插入到子节点
        bool insertedToChild = false;
        for (auto& child : node->children) {
            if (child && child->bounds.overlaps(objBounds)) {
                insertNode(child.get(), object);
                insertedToChild = true;
            }
        }
        
        // 如果对象跨越多个子节点，保留在当前节点
        if (!insertedToChild) {
            node->objects.push_back(object);
            node->objectCount++;
        }
    }
}

void HierarchicalSpatialIndex::removeNode(QuadTreeNode* node, const std::shared_ptr<Object>& object)
{
    if (!node || !object) return;
    
    // 从当前节点移除
    auto it = std::find(node->objects.begin(), node->objects.end(), object);
    if (it != node->objects.end()) {
        node->objects.erase(it);
        node->objectCount--;
    }
    
    // 递归从子节点移除
    if (!node->isLeaf()) {
        for (auto& child : node->children) {
            if (child) {
                removeNode(child.get(), object);
            }
        }
        
        // 检查是否可以合并子节点
        if (node->canMerge()) {
            node->merge();
        }
    }
}

void HierarchicalSpatialIndex::queryNode(QuadTreeNode* node, const AABB& bounds,
                                        std::vector<std::shared_ptr<Object>>& results) const
{
    if (!node || !node->bounds.overlaps(bounds)) return;
    
    // 检查查询缓存
    if (node->isQueryCacheValid(bounds, m_currentFrame)) {
        for (auto& obj : node->cachedVisibleObjects) {
            results.push_back(obj);
        }
        return;
    }
    
    std::vector<std::shared_ptr<Object>> nodeResults;
    
    // 添加当前节点的对象
    for (auto& obj : node->objects) {
        if (obj && obj->getModel() && obj->getBoundingBox().overlaps(bounds)) {
            nodeResults.push_back(obj);
            results.push_back(obj);
        }
    }
    
    // 递归查询子节点
    if (!node->isLeaf()) {
        for (auto& child : node->children) {
            if (child) {
                queryNode(child.get(), bounds, results);
            }
        }
    }
    
    // 更新查询缓存
    const_cast<QuadTreeNode*>(node)->updateQueryCache(bounds, m_currentFrame, nodeResults);
}

std::pair<int, int> HierarchicalSpatialIndex::getGridCoordinates(const QVector2D& position) const
{
    if (!m_root) return {0, 0};
    
    int x = static_cast<int>((position.x() - m_root->bounds.minX) / m_gridCellSize);
    int y = static_cast<int>((position.y() - m_root->bounds.minY) / m_gridCellSize);
    
    x = std::clamp(x, 0, m_gridWidth - 1);
    y = std::clamp(y, 0, m_gridHeight - 1);
    
    return {x, y};
}

std::shared_ptr<SpatialCell> HierarchicalSpatialIndex::getCell(int x, int y) const
{
    if (x >= 0 && x < m_gridWidth && y >= 0 && y < m_gridHeight) {
        return m_spatialGrid[y][x];
    }
    return nullptr;
}

bool HierarchicalSpatialIndex::tryGetCachedQuery(const AABB& bounds, 
                                                std::vector<std::shared_ptr<Object>>& results) const
{
    // 简单的查询缓存检查
    if (!m_queryCache.empty()) {
        const auto& cache = m_queryCache.front();
        if (cache.bounds.minX == bounds.minX && cache.bounds.minY == bounds.minY &&
            cache.bounds.maxX == bounds.maxX && cache.bounds.maxY == bounds.maxY &&
            (m_currentFrame - cache.frame) <= 3) {
            results = cache.results;
            return true;
        }
    }
    return false;
}

void HierarchicalSpatialIndex::cacheQuery(const AABB& bounds, 
                                         const std::vector<std::shared_ptr<Object>>& results) const
{
    if (m_queryCache.size() >= MAX_CACHE_SIZE) {
        const_cast<std::queue<QueryCache>&>(m_queryCache).pop();
    }
    
    QueryCache cache;
    cache.bounds = bounds;
    cache.frame = m_currentFrame;
    cache.results = results;
    
    const_cast<std::queue<QueryCache>&>(m_queryCache).push(cache);
}

HierarchicalSpatialIndex::Statistics HierarchicalSpatialIndex::getStatistics() const
{
    Statistics stats = m_stats;
    if (m_root) {
        updateStatistics(m_root.get(), stats);
    }
    return stats;
}

void HierarchicalSpatialIndex::resetStatistics()
{
    m_stats = Statistics{};
}

void HierarchicalSpatialIndex::updateStatistics(QuadTreeNode* node, Statistics& stats) const
{
    if (!node) return;
    
    stats.totalNodes++;
    stats.totalObjects += node->objects.size();
    stats.maxDepth = std::max(stats.maxDepth, static_cast<size_t>(node->level));
    
    if (node->isLeaf()) {
        stats.leafNodes++;
    } else {
        for (auto& child : node->children) {
            if (child) {
                updateStatistics(child.get(), stats);
            }
        }
    }
    
    if (stats.leafNodes > 0) {
        stats.avgObjectsPerLeaf = static_cast<float>(stats.totalObjects) / static_cast<float>(stats.leafNodes);
    }
} 