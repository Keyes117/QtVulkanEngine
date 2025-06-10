#pragma once

#include <memory>
#include <vector>
#include <unordered_map>
#include <functional>

#include "HierarchicalSpatialIndex.h"
#include "RenderManager.h"
#include "Camera.h"
#include "Object.h"
#include "const.h"

// 空间渲染优化配置
struct SpatialRenderConfig
{
    static constexpr size_t MAX_BATCH_SIZE = 1000;
    static constexpr float LOD_DISTANCE_THRESHOLD = 500.0f;
    static constexpr size_t MAX_DRAW_CALLS_PER_FRAME = 500;
    static constexpr float OCCLUSION_CULLING_THRESHOLD = 0.8f;
    static constexpr bool ENABLE_INSTANCED_RENDERING = true;
    static constexpr bool ENABLE_INDIRECT_RENDERING = true;
};

// 渲染批次
struct SpatialRenderBatch
{
    ModelType modelType;
    LODLevel lodLevel;
    std::vector<std::shared_ptr<Object>> objects;
    AABB combinedBounds;
    
    // 渲染状态
    uint32_t instanceCount = 0;
    uint32_t drawCallCount = 0;
    float distanceFromCamera = 0.0f;
    uint32_t priority = 0;
    
    // 缓冲区信息
    uint32_t bufferOffset = 0;
    uint32_t bufferSize = 0;
    bool needsUpdate = true;
    
    SpatialRenderBatch(ModelType type, LODLevel lod) 
        : modelType(type), lodLevel(lod) 
    {
        combinedBounds = AABB(
            std::numeric_limits<float>::max(),
            std::numeric_limits<float>::max(),
            std::numeric_limits<float>::lowest(),
            std::numeric_limits<float>::lowest()
        );
    }
    
    void addObject(const std::shared_ptr<Object>& obj);
    void updateBounds();
    float calculatePriority(const Camera& camera) const;
};

// 遮挡剔除信息
struct OcclusionInfo
{
    AABB bounds;
    float occlusionFactor = 0.0f;
    bool isOccluded = false;
    uint32_t occluderCount = 0;
    std::vector<std::shared_ptr<Object>> occluders;
};

// 空间渲染优化器
class SpatialRenderOptimizer
{
public:
    SpatialRenderOptimizer(RenderManager& renderManager, 
                          std::shared_ptr<HierarchicalSpatialIndex> spatialIndex);
    ~SpatialRenderOptimizer() = default;

    // 主要优化接口
    std::vector<SpatialRenderBatch> optimizeRenderBatches(
        const std::vector<std::shared_ptr<Object>>& visibleObjects,
        const Camera& camera);
    
    std::vector<SpatialRenderBatch> createSpatialBatches(
        const std::vector<HierarchicalSpatialIndex::SpatialGroup>& spatialGroups,
        const Camera& camera);

    // 视锥体剔除优化
    std::vector<std::shared_ptr<Object>> performAdvancedCulling(
        const std::vector<std::shared_ptr<Object>>& objects,
        const Camera& camera);

    // 遮挡剔除
    std::vector<std::shared_ptr<Object>> performOcclusionCulling(
        const std::vector<std::shared_ptr<Object>>& objects,
        const Camera& camera);

    // LOD管理
    void updateLODLevels(std::vector<std::shared_ptr<Object>>& objects,
                        const Camera& camera);

    // 批次合并和分割
    void mergeBatches(std::vector<SpatialRenderBatch>& batches);
    void splitLargeBatches(std::vector<SpatialRenderBatch>& batches);

    // 性能监控
    struct OptimizationStats
    {
        // 批次统计
        size_t totalBatches = 0;
        size_t mergedBatches = 0;
        size_t splitBatches = 0;
        
        // 剔除统计
        size_t totalObjects = 0;
        size_t visibleObjects = 0;
        size_t frustumCulled = 0;
        size_t occlusionCulled = 0;
        size_t lodCulled = 0;
        
        // 时间统计
        float batchingTime = 0.0f;
        float cullingTime = 0.0f;
        float occlusionTime = 0.0f;
        float lodTime = 0.0f;
        
        // 渲染统计
        uint32_t drawCalls = 0;
        uint32_t instancedDrawCalls = 0;
        uint32_t indirectDrawCalls = 0;
        
        // 优化效果
        float batchingEfficiency = 0.0f;
        float cullingEfficiency = 0.0f;
        float memoryUsage = 0.0f;
    };
    
    OptimizationStats getOptimizationStats() const { return m_stats; }
    void resetOptimizationStats() { m_stats = OptimizationStats{}; }

    // 配置设置
    void setMaxBatchSize(size_t size) { m_maxBatchSize = size; }
    void setLODDistanceThreshold(float threshold) { m_lodDistanceThreshold = threshold; }
    void setEnableOcclusionCulling(bool enable) { m_enableOcclusionCulling = enable; }
    void setEnableInstancedRendering(bool enable) { m_enableInstancedRendering = enable; }

private:
    RenderManager& m_renderManager;
    std::shared_ptr<HierarchicalSpatialIndex> m_spatialIndex;
    
    // 配置参数
    size_t m_maxBatchSize = SpatialRenderConfig::MAX_BATCH_SIZE;
    float m_lodDistanceThreshold = SpatialRenderConfig::LOD_DISTANCE_THRESHOLD;
    bool m_enableOcclusionCulling = true;
    bool m_enableInstancedRendering = SpatialRenderConfig::ENABLE_INSTANCED_RENDERING;
    
    // 性能统计
    mutable OptimizationStats m_stats;
    
    // 缓存
    struct BatchCache
    {
        std::vector<SpatialRenderBatch> lastBatches;
        uint32_t lastFrame = 0;
        AABB lastCameraBounds;
        Camera lastCamera;
    } m_batchCache;
    
    // 遮挡剔除相关
    std::unordered_map<Object::ObjectID, OcclusionInfo> m_occlusionCache;
    
    // 内部方法
    
    // 批次管理
    void groupObjectsByTypeAndLOD(
        const std::vector<std::shared_ptr<Object>>& objects,
        const Camera& camera,
        std::unordered_map<int, std::vector<std::shared_ptr<Object>>>& groups);
    
    int calculateBatchKey(const std::shared_ptr<Object>& object, 
                         const Camera& camera) const;
    
    bool shouldMergeBatches(const SpatialRenderBatch& batch1, 
                           const SpatialRenderBatch& batch2) const;
    
    bool shouldSplitBatch(const SpatialRenderBatch& batch) const;
    
    // 剔除算法
    bool isInFrustum(const std::shared_ptr<Object>& object, 
                     const Camera& camera) const;
    
    bool isOccluded(const std::shared_ptr<Object>& object,
                    const std::vector<std::shared_ptr<Object>>& potentialOccluders,
                    const Camera& camera) const;
    
    float calculateOcclusionFactor(const std::shared_ptr<Object>& object,
                                  const std::shared_ptr<Object>& occluder,
                                  const Camera& camera) const;
    
    // LOD计算
    LODLevel calculateOptimalLOD(const std::shared_ptr<Object>& object,
                                const Camera& camera) const;
    
    float calculateLODDistance(const std::shared_ptr<Object>& object,
                              const Camera& camera) const;
    
    // 空间分析
    void analyzeSpatialDistribution(
        const std::vector<std::shared_ptr<Object>>& objects,
        std::vector<AABB>& clusters) const;
    
    float calculateSpatialDensity(const AABB& bounds,
                                 const std::vector<std::shared_ptr<Object>>& objects) const;
    
    // 缓存管理
    bool tryGetCachedBatches(const Camera& camera,
                            std::vector<SpatialRenderBatch>& batches);
    
    void cacheBatches(const Camera& camera,
                     const std::vector<SpatialRenderBatch>& batches);
    
    // 性能优化
    void optimizeBatchOrder(std::vector<SpatialRenderBatch>& batches,
                           const Camera& camera);
    
    void updateRenderingPriorities(std::vector<SpatialRenderBatch>& batches,
                                  const Camera& camera);
    
    // 统计更新
    void updateStatistics(const std::vector<SpatialRenderBatch>& batches,
                         const std::vector<std::shared_ptr<Object>>& culledObjects);
    
    void recordOptimizationTime(const std::string& operation, float duration);
};

// 渲染优化策略
class RenderOptimizationStrategy
{
public:
    virtual ~RenderOptimizationStrategy() = default;
    
    virtual std::vector<SpatialRenderBatch> optimize(
        const std::vector<std::shared_ptr<Object>>& objects,
        const Camera& camera,
        SpatialRenderOptimizer& optimizer) = 0;
    
    virtual float calculatePriority(const SpatialRenderBatch& batch,
                                   const Camera& camera) = 0;
};

// 距离优先策略
class DistanceBasedStrategy : public RenderOptimizationStrategy
{
public:
    std::vector<SpatialRenderBatch> optimize(
        const std::vector<std::shared_ptr<Object>>& objects,
        const Camera& camera,
        SpatialRenderOptimizer& optimizer) override;
    
    float calculatePriority(const SpatialRenderBatch& batch,
                           const Camera& camera) override;
};

// 类型优先策略
class TypeBasedStrategy : public RenderOptimizationStrategy
{
public:
    std::vector<SpatialRenderBatch> optimize(
        const std::vector<std::shared_ptr<Object>>& objects,
        const Camera& camera,
        SpatialRenderOptimizer& optimizer) override;
    
    float calculatePriority(const SpatialRenderBatch& batch,
                           const Camera& camera) override;
};

// 空间聚类策略
class SpatialClusteringStrategy : public RenderOptimizationStrategy
{
public:
    std::vector<SpatialRenderBatch> optimize(
        const std::vector<std::shared_ptr<Object>>& objects,
        const Camera& camera,
        SpatialRenderOptimizer& optimizer) override;
    
    float calculatePriority(const SpatialRenderBatch& batch,
                           const Camera& camera) override;
};

// 优化器工厂
class SpatialRenderOptimizerFactory
{
public:
    enum class OptimizationLevel
    {
        BASIC,
        ADVANCED,
        ULTRA
    };
    
    static std::unique_ptr<SpatialRenderOptimizer> create(
        OptimizationLevel level,
        RenderManager& renderManager,
        std::shared_ptr<HierarchicalSpatialIndex> spatialIndex);
        
    static std::unique_ptr<RenderOptimizationStrategy> createStrategy(
        OptimizationLevel level);
}; 