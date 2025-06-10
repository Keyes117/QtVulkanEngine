#pragma once

#include <array>
#include <vector>
#include <memory>
#include <unordered_map>
#include <queue>
#include <functional>

#include "Object.h"
#include "const.h"
#include "Camera.h"

// 空间索引配置
struct SpatialIndexConfig
{
    static constexpr int MAX_DEPTH = 10;
    static constexpr size_t MAX_OBJECTS_PER_NODE = 200;
    static constexpr size_t MIN_OBJECTS_FOR_SPLIT = 50;
    static constexpr float LOD_DISTANCE_MULTIPLIER = 2.0f;
    static constexpr int MAX_LOD_LEVELS = 5;
};

// LOD级别定义
enum class LODLevel : uint8_t
{
    HIGH = 0,    // 高精度 (近距离)
    MEDIUM = 1,  // 中等精度
    LOW = 2,     // 低精度
    CULLED = 3   // 完全剔除
};

// 空间网格单元
struct SpatialCell
{
    AABB bounds;
    std::vector<std::shared_ptr<Object>> objects;
    std::vector<std::shared_ptr<Object>> staticObjects;   // 静态对象
    std::vector<std::shared_ptr<Object>> dynamicObjects;  // 动态对象
    
    uint32_t cellId;
    int level;
    bool isDirty = false;
    uint32_t lastUpdateFrame = 0;
    
    // 邻居单元缓存
    std::array<std::weak_ptr<SpatialCell>, 8> neighbors;
    
    SpatialCell(const AABB& b, uint32_t id, int l) 
        : bounds(b), cellId(id), level(l) {}
    
    void addObject(const std::shared_ptr<Object>& obj);
    void removeObject(const std::shared_ptr<Object>& obj);
    void updateObject(const std::shared_ptr<Object>& obj);
    
    size_t getTotalObjectCount() const {
        return staticObjects.size() + dynamicObjects.size();
    }
};

// 四叉树节点 (改进版)
struct QuadTreeNode
{
    AABB bounds;
    int level;
    std::vector<std::shared_ptr<Object>> objects;
    std::array<std::unique_ptr<QuadTreeNode>, 4> children;
    
    // 性能优化字段
    uint32_t lastQueryFrame = 0;
    std::vector<std::shared_ptr<Object>> cachedVisibleObjects;
    AABB lastQueryBounds;
    
    // 空间统计信息
    uint32_t objectCount = 0;
    float density = 0.0f;
    
    QuadTreeNode(const AABB& b, int l) : bounds(b), level(l) {}
    
    bool isLeaf() const { return !children[0]; }
    bool needsSplit() const;
    bool canMerge() const;
    
    void split();
    void merge();
    void redistributeObjects();
    
    // 智能查询缓存
    bool isQueryCacheValid(const AABB& queryBounds, uint32_t currentFrame) const;
    void updateQueryCache(const AABB& queryBounds, uint32_t currentFrame, 
                         const std::vector<std::shared_ptr<Object>>& results);
};

// 层次化空间索引主类
class HierarchicalSpatialIndex
{
public:
    HierarchicalSpatialIndex(const AABB& worldBounds);
    ~HierarchicalSpatialIndex() = default;

    // 基本操作
    void insert(const std::shared_ptr<Object>& object);
    void remove(const std::shared_ptr<Object>& object);
    void update(const std::shared_ptr<Object>& object);
    void clear();

    // 高级查询
    std::vector<std::shared_ptr<Object>> query(const AABB& bounds);
    std::vector<std::shared_ptr<Object>> queryWithLOD(const Camera& camera, const AABB& bounds);
    std::vector<std::shared_ptr<Object>> queryFrustum(const Camera& camera);
    
    // 空间分组查询
    struct SpatialGroup
    {
        std::vector<std::shared_ptr<Object>> objects;
        AABB bounds;
        LODLevel lodLevel;
        float distance;
        uint32_t priority;
    };
    std::vector<SpatialGroup> querySpatialGroups(const Camera& camera);

    // 性能优化
    void optimizeStructure();
    void rebuildOptimized();
    
    // 统计信息
    struct Statistics
    {
        size_t totalNodes = 0;
        size_t leafNodes = 0;
        size_t totalObjects = 0;
        size_t maxDepth = 0;
        float avgObjectsPerLeaf = 0.0f;
        float memoryUsage = 0.0f;
        
        // 查询性能统计
        uint32_t queryCount = 0;
        float avgQueryTime = 0.0f;
        float cacheHitRate = 0.0f;
    };
    
    Statistics getStatistics() const;
    void resetStatistics();

private:
    // 核心数据结构
    std::unique_ptr<QuadTreeNode> m_root;
    std::unordered_map<Object::ObjectID, std::weak_ptr<Object>> m_objectIndex;
    
    // 空间网格系统 (用于快速邻域查询)
    std::vector<std::vector<std::shared_ptr<SpatialCell>>> m_spatialGrid;
    float m_gridCellSize;
    int m_gridWidth, m_gridHeight;
    
    // LOD系统
    std::array<std::vector<std::shared_ptr<Object>>, 4> m_lodLevels;
    
    // 性能优化
    mutable Statistics m_stats;
    uint32_t m_currentFrame = 0;
    
    // 缓存系统
    struct QueryCache
    {
        AABB bounds;
        uint32_t frame;
        std::vector<std::shared_ptr<Object>> results;
    };
    mutable std::queue<QueryCache> m_queryCache;
    static constexpr size_t MAX_CACHE_SIZE = 16;

    // 内部方法
    void insertNode(QuadTreeNode* node, const std::shared_ptr<Object>& object);
    void removeNode(QuadTreeNode* node, const std::shared_ptr<Object>& object);
    void queryNode(QuadTreeNode* node, const AABB& bounds, 
                   std::vector<std::shared_ptr<Object>>& results) const;
    
    // LOD计算
    LODLevel calculateLOD(const std::shared_ptr<Object>& object, const Camera& camera) const;
    float calculateDistance(const std::shared_ptr<Object>& object, const Camera& camera) const;
    
    // 空间网格操作
    void initializeSpatialGrid();
    std::pair<int, int> getGridCoordinates(const QVector2D& position) const;
    std::shared_ptr<SpatialCell> getCell(int x, int y) const;
    
    // 优化算法
    void balanceTree();
    void optimizeNodeDistribution(QuadTreeNode* node);
    void removeEmptyNodes();
    
    // 缓存管理
    bool tryGetCachedQuery(const AABB& bounds, std::vector<std::shared_ptr<Object>>& results) const;
    void cacheQuery(const AABB& bounds, const std::vector<std::shared_ptr<Object>>& results) const;
    
    // 统计更新
    void updateStatistics(QuadTreeNode* node, Statistics& stats) const;
};

// 空间索引工厂
class SpatialIndexFactory
{
public:
    enum class IndexType
    {
        QUAD_TREE,
        HIERARCHICAL,
        SPATIAL_HASH,
        OCTREE
    };
    
    static std::unique_ptr<HierarchicalSpatialIndex> create(
        IndexType type, const AABB& bounds);
    
    static std::unique_ptr<HierarchicalSpatialIndex> createOptimized(
        const AABB& bounds, const std::vector<std::shared_ptr<Object>>& objects);
}; 