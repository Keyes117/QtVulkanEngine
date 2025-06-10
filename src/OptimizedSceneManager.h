#pragma once

#include "OptimizedObjectManager.h"
#include "SpatialRenderOptimizer.h"
#include "RenderManager.h"
#include "FrameInfo.h"
#include "Camera.h"

class OptimizedSceneManager
{
public:
    OptimizedSceneManager(MyVulkanWindow& window, Device& device, 
                         VkDescriptorSetLayout globalSetLayout, 
                         const AABB& worldBounds);
    ~OptimizedSceneManager() = default;

    // 对象管理接口
    Object::ObjectID addObject(const Object::Builder& builder);
    void removeObject(Object::ObjectID id);
    void updateObject(Object::ObjectID id, const UpdateFunc& updateFunc);
    
    // 批量操作
    void beginBatchOperations();
    void endBatchOperations();
    
    // 渲染接口
    void render(FrameInfo& frameInfo);
    
    // 性能优化控制
    void enableAdvancedCulling(bool enable) { m_enableAdvancedCulling = enable; }
    void enableOcclusionCulling(bool enable) { m_enableOcclusionCulling = enable; }
    void enableLODSystem(bool enable) { m_enableLODSystem = enable; }
    void enableSpatialBatching(bool enable) { m_enableSpatialBatching = enable; }
    
    // 性能监控
    struct PerformanceReport
    {
        OptimizedObjectManager::PerformanceMetrics objectManagerMetrics;
        SpatialRenderOptimizer::OptimizationStats renderOptimizerStats;
        HierarchicalSpatialIndex::Statistics spatialIndexStats;
        
        struct FrameStats
        {
            float totalFrameTime = 0.0f;
            float updateTime = 0.0f;
            float cullingTime = 0.0f;
            float renderTime = 0.0f;
            
            size_t totalObjects = 0;
            size_t visibleObjects = 0;
            size_t drawCalls = 0;
            size_t triangles = 0;
            
            float fps = 0.0f;
            float avgFrameTime = 0.0f;
        } frameStats;
    };
    
    PerformanceReport getPerformanceReport() const;
    void resetPerformanceCounters();
    
    // 调试和可视化
    void enableDebugVisualization(bool enable) { m_debugVisualization = enable; }
    void visualizeSpatialIndex(FrameInfo& frameInfo);
    void visualizeCullingResults(FrameInfo& frameInfo);
    
    // 访问器
    OptimizedObjectManager& getObjectManager() { return *m_objectManager; }
    RenderManager& getRenderManager() { return m_renderManager; }
    SpatialRenderOptimizer& getRenderOptimizer() { return *m_renderOptimizer; }

private:
    // 核心组件
    std::unique_ptr<OptimizedObjectManager> m_objectManager;
    RenderManager m_renderManager;
    std::unique_ptr<SpatialRenderOptimizer> m_renderOptimizer;
    
    // 空间索引
    std::shared_ptr<HierarchicalSpatialIndex> m_spatialIndex;
    
    // 优化开关
    bool m_enableAdvancedCulling = true;
    bool m_enableOcclusionCulling = true;
    bool m_enableLODSystem = true;
    bool m_enableSpatialBatching = true;
    bool m_debugVisualization = false;
    
    // 性能监控
    mutable PerformanceReport m_performanceReport;
    std::chrono::high_resolution_clock::time_point m_lastFrameTime;
    std::vector<float> m_frameTimes;
    static constexpr size_t MAX_FRAME_SAMPLES = 60;
    
    // 缓存
    struct RenderCache
    {
        std::vector<std::shared_ptr<Object>> lastVisibleObjects;
        std::vector<SpatialRenderBatch> lastRenderBatches;
        Camera lastCamera;
        uint32_t lastFrame = 0;
    } m_renderCache;
    
    // 内部方法
    void initializeComponents(MyVulkanWindow& window, Device& device, 
                             VkDescriptorSetLayout globalSetLayout, 
                             const AABB& worldBounds);
    
    void updatePerformanceMetrics();
    void onObjectChanged(const std::shared_ptr<Object>& object);
    
    // 渲染流程
    std::vector<std::shared_ptr<Object>> getVisibleObjects(const Camera& camera);
    std::vector<SpatialRenderBatch> createOptimizedBatches(
        const std::vector<std::shared_ptr<Object>>& visibleObjects,
        const Camera& camera);
    
    void renderOptimizedBatches(const std::vector<SpatialRenderBatch>& batches,
                               FrameInfo& frameInfo);
    
    // 调试渲染
    void renderDebugInfo(FrameInfo& frameInfo);
    void renderSpatialIndexBounds(FrameInfo& frameInfo);
    void renderObjectBounds(FrameInfo& frameInfo);
}; 