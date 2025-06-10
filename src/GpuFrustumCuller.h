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
#include "VMABuffer.h"



struct ObjectGPUData
{
    float transform[16];        //4x4�任����
    float boundingBox[4];       //minx,miny,maxx,maxy
    uint32_t chunkId;           //��Ӧ��chunkId
    uint32_t modelType;         //ModelTypeö��ֵ
    uint32_t visible;           //0 = ���ɼ��� 1 = �ɼ�
    uint32_t padding;           //���
};

struct CameraGPUData
{
    float viewMatrix[16];       //��ͼ����
    float projMatrix[16];       //ͶӰ����
    float frustumPlanes[24];    //6����׶�� 4��floatһ����(ax+by+cz+d = 0)
    float cameraPos[3];         //���λ��        
    float nearPlane;            //��ƽ��
    float farPlane;             //Զƽ��
    uint32_t padding[3];
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

    std::vector<uint32_t> performCulling(
        const std::vector<std::shared_ptr<Object>>& objects,
        const Camera& camera
    );

    const CullingStats& getStats() const { return m_stats; }
    void resize(uint32_t newMaxObjects);

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
    std::vector<uint32_t> downloadResults(uint32_t objectCount);

    void matrixToFloatArray(const QMatrix4x4& matrix, float* array);
    void extractFrustumPlanes(const QMatrix4x4& viewProjMatrix, float* planes);

private:
    Device& m_device;
    bool m_isInit = false;

    // Compute Pipeline
    VkPipeline m_computePipeline = VK_NULL_HANDLE;
    VkPipelineLayout m_pipelineLayout = VK_NULL_HANDLE;
    VkShaderModule m_computeShader = VK_NULL_HANDLE;

    //TODO: ��������App�е�ȫ���������� �����Ȱ��߼���ͨ
    // ������ 
    std::unique_ptr<DescriptorSetLayout> m_descriptorSetLayout;
    std::unique_ptr<DescriptorPool> m_descriptorPool;
    VkDescriptorSet m_descriptorSet = VK_NULL_HANDLE;

    // ������
    std::unique_ptr<VMABuffer> m_objectBuffer;       // ��������
    std::unique_ptr<VMABuffer> m_cameraBuffer;       // �������
    std::unique_ptr<VMABuffer> m_resultBuffer;       // ���������(�ɼ���������)
    std::unique_ptr<VMABuffer> m_countBuffer;        // �ɼ���������

    void* m_resultMappedMemory = nullptr;
    void* m_countMappedMemory = nullptr;
    bool m_buffersAreMapped = false;

    // ״̬
    uint32_t m_maxObjects = 0;
    CullingStats m_stats;


    // �������С(����GPU�Ż�)
    static constexpr uint32_t WORKGROUP_SIZE = 64;
};

