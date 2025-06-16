#pragma once


#include <vector>
#include <memory>

#include <qmatrix4x4.h>
#include <qvector3d.h>
#include <qvector4d.h>

#include "BufferPool.h"
#include "Camera.h"
#include "Descriptors.h"
#include "Device.h"
#include "Object.h"
#include "ObjectManager.h"
#include "VMABuffer.h"

#define  DEBUG_BUFFER_ENABLED

struct DebugData
{
    uint32_t totalProcessed;
    uint32_t visibleCount;
    uint32_t segmentDrawCounts[8];
    uint32_t drawIndex;
    uint32_t padding[1];
};

struct SegmentAddressInfo
{
    uint64_t drawCommandAddress;
    uint64_t drawCountAddress;
    uint32_t maxDrawCommands;
    uint32_t segmentIndex;
    uint32_t modelType;
    uint32_t padding[3];
};


struct ObjectGPUData
{
    float transform[16];        //4x4变换矩阵
    float boundingBox[4];       //minx,miny,maxx,maxy
    uint32_t chunkId;           //对应的chunkId
    uint32_t modelType;         //ModelType枚举值

    uint32_t indexOffset;
    uint32_t indexCount;
    uint32_t vertexOffset;
    uint32_t segmentIndex;

    uint32_t visible;           //0 = 不可见， 1 = 可见
    uint32_t padding[1] = { 0 };           //填充
};

struct CameraGPUData
{
    float viewMatrix[16];       //视图矩阵
    float projMatrix[16];       //投影矩阵
    float frustumPlanes[24];    //6个视锥面 4个float一个面(ax+by+cz+d = 0)
    float cameraPos[3];         //相机位置        
    float nearPlane;            //近平面
    float farPlane;             //远平面
    uint32_t padding[3] = { 0 };
};


struct CullingStats
{
    uint32_t totalObjects = 0;
    uint32_t visibleObjects = 0;
    uint32_t culledObjects = 0;
    float cullingTime = 0.f;
    float uploadTime = 0.f;
    float downloadTime = 0.f;
};

class GpuFrustumCuller
{
public:
    GpuFrustumCuller(Device& device);
    ~GpuFrustumCuller();

    void initialize(uint32_t maxObjects);
    bool isInit() { return m_isInit; }


    void uploadObjectDataFromPool(const ObjectManager::ObjectDataPool& dataPool, const BufferPool& bufferPool);
    void performCullingGPUOnly(FrameInfo& frameInfo, const BufferPool& bufferPool);

    //VkBuffer getDrawCommandBUffer() const { return m_drawCommandBuffer->getBuffer(); }
    //VkBuffer getDrawCountBuffer() const { return m_drawCountBuffer->getBuffer(); }

    const CullingStats& getStats() const { return m_stats; }
    void resize(uint32_t newMaxObjects);

    void updateSegmentAddresses(const BufferPool& bufferPool);
#ifdef DEBUG_BUFFER_ENABLED
    void readDebugData();
#endif // DEBUG_BUFFER_ENABLED
    void setSegmentUpdateFlag(bool NeedUpdate) { isSegmentNeedsUpdate = NeedUpdate; }

    void resetDrawCountBuffers(VkCommandBuffer cmd, const BufferPool& bufferPool);
private:
    void createBuffers(uint32_t maxObjects);
    void createDescriptorSetLayout();
    void createDescriptorPool();
    void createDescriptorSets();
    void createPipelineLayout();
    void createComputePipeline();

    void updateDescriptorSets();

    void updateCameraData(const Camera& camera);
    void uploadObjectData(const std::vector<std::shared_ptr<Object>>& objects);
    void uploadSegmentAddress();

    void matrixToFloatArray(const QMatrix4x4& matrix, float* array);
    void extractFrustumPlanes(const QMatrix4x4& viewProjMatrix, float* planes);


private:
    Device& m_device;
    bool m_isInit = false;

    // Compute Pipeline
    VkPipeline m_computePipeline = VK_NULL_HANDLE;
    VkPipelineLayout m_pipelineLayout = VK_NULL_HANDLE;
    VkShaderModule m_computeShader = VK_NULL_HANDLE;

    //TODO: 后续改用App中的全局描述符， 这里先把逻辑走通
    // 描述符 
    std::unique_ptr<DescriptorSetLayout> m_descriptorSetLayout;
    std::unique_ptr<DescriptorPool> m_descriptorPool;
    VkDescriptorSet m_descriptorSet = VK_NULL_HANDLE;

    // 缓冲区
    std::unique_ptr<VMABuffer> m_objectBuffer;       // 对象数据
    std::unique_ptr<VMABuffer> m_cameraBuffer;       // 相机数据
    std::unique_ptr<VMABuffer> m_segmentAddressBuffer;

#ifdef DEBUG_BUFFER_ENABLED
    std::unique_ptr<VMABuffer> m_debugBuffer;

#endif
    // 状态
    uint32_t m_maxObjects = 0;
    CullingStats m_stats;

    bool    isSegmentNeedsUpdate = false;
    std::vector<SegmentAddressInfo>     m_segmentAddresses;

    static constexpr uint32_t MAX_SEGMENTS = 64;
    // 工作组大小(根据GPU优化)
    static constexpr uint32_t WORKGROUP_SIZE = 64;
};

