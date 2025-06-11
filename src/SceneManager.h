#pragma once

#include <unordered_set>


#include "ObjectManager.h"
#include "RenderManager.h"
#include "FrameInfo.h"

class SceneManager
{
public:
    class SpatialIndex
    {
    public:
        void insert(ObjectManager::ObjectHandle handle, const AABB& bounds);
        void remove(ObjectManager::ObjectHandle handle);
        void update(ObjectManager::ObjectHandle handle, const AABB& newBounds);

        std::vector<ObjectManager::ObjectHandle> query(const AABB& bounds) const;
        std::vector<ObjectManager::ObjectHandle> queryRadius(const QVector3D& center, float radius) const;

        void clear()
        {
            m_spatialGrid.clear();
            m_objectToCells.clear();
        }
        size_t size() const { return m_objectToCells.size(); }

    private:
        struct SpatialCell
        {
            std::unordered_set<ObjectManager::ObjectHandle> objects;
        };

        std::unordered_map<uint64_t, SpatialCell> m_spatialGrid;
        std::unordered_map<ObjectManager::ObjectHandle, std::vector<uint64_t>> m_objectToCells;

        float m_cellSize = 1000.0f;

        uint64_t hash(const QVector3D& pos) const;
        std::vector<uint64_t> getCellsForBounds(const AABB& bounds) const;

    };

    struct CullingConfig
    {
        bool enableFrustumCulling = true;
        bool enableDistanceCulling = true;
        bool enbaleOcclusionCulling = false;
        float maxRenderDistance = 1000.f;
    };

    struct LODConfig
    {
        std::vector<float> distances = { 50.f, 200.f, 500.f };
        bool enableLOD = false;
        bool enableDynamicLOD = true;
    };

    struct SceneStats
    {
        uint32_t totalObjects = 0;
        uint32_t activeObjects = 0;
        uint32_t visibleObjects = 0;
        uint32_t culledObjects = 0;

        float cullingTime = 0.0f;
        float lodUpdateTime = 0.0f;
        float spatialUpdateTime = 0.0f;

        std::array<uint32_t, 4> objectsPerLOD = { 0 };

        void reset() { *this = SceneStats{}; }
    };

    SceneManager(MyVulkanWindow& window, Device& device, VkDescriptorSetLayout globalSetLayout, const AABB& worldBounds);
    ~SceneManager() = default;

    /*对象管理*/
    ObjectManager::ObjectHandle addObject(const Object::Builder& builder);
    void removeObject(ObjectManager::ObjectHandle handle);
    void updateObjectTransform(ObjectManager::ObjectHandle handle, const QMatrix4x4& transform);
    void updateObjectColor(ObjectManager::ObjectHandle handle, const QVector3D& color);

    void render(FrameInfo& frameInfo);
    void renderWithCulling(FrameInfo& frameInfo, const Camera& camera);


    std::vector<ObjectManager::ObjectHandle> getVisibleObjects(const AABB& frustum) const;
    std::vector<ObjectManager::ObjectHandle> getObjectsInRadius(const QVector3D& center, float radius) const;
    std::vector<ObjectManager::ObjectHandle> getObjectsInChunk(uint32_t chunkId) const;


    void setCullingConfig(const CullingConfig& config) { m_cullingConfig = config; }
    void setLODConfig(const LODConfig& config) { m_lodConfig = config; }

    const CullingConfig& getCullingConfig() const { return m_cullingConfig; }
    const LODConfig& getLODConfig() const { return m_lodConfig; }

    const SceneStats& getSceneStats() const { return m_sceneStats; }
    void resetStats() { m_sceneStats.reset(); }

    ObjectManager& getOBjectManager() { return m_objectManager; }
    RenderManager& getRenderManager() { return m_renderManager; }

    void updateSpatialIndex();
    void optimizeMemory();

private:
    // 核心功能
    std::vector<ObjectManager::ObjectHandle> performCulling(const Camera& camera);
    void updateLOD(const QVector3D& cameraPos);

    // 辅助方法
    void updateSpatialIndexForObject(ObjectManager::ObjectHandle handle);
    float calculateDistance(const AABB& bounds, const QVector3D& point) const;
    uint8_t calculateLODLevel(float distance) const;

private:
    ObjectManager m_objectManager;
    RenderManager m_renderManager;

    SpatialIndex m_spatialIndex;

    AABB m_worldBounds;
    CullingConfig m_cullingConfig;
    LODConfig m_lodConfig;
    SceneStats m_sceneStats;

    uint32_t m_frameCounter = 0;
};

