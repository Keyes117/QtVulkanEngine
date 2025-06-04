#pragma once

#include <memory>
#include <unordered_map>

#include "Renderer.h"
#include "RenderSystem.h"
#include "BufferPool.h"
#include "Object.h"
#include "FrameInfo.h"


class RenderManager
{
public:
    struct InstanceData
    {
        QMatrix4x4 transform;
        QVector3D color;
        float padding;
    };

    struct RenderBatch
    {
        std::vector<Object*> objects;
        std::unique_ptr<VMABuffer> instanceBuffer;
        VkCommandBuffer commandBuffer;
        bool needsUpdate{ true };
        size_t bufferCapacity{ 0 };
    };

private:
    Device& m_device;

    BufferPool                                      m_BufferPool;
    std::unordered_map<Model*, RenderBatch>         m_batches;

    RenderBatch& getBatch(Model* model);
    void updateBatch(RenderBatch& batch, const std::vector<Object*>& objects);
    void recordBatchCommands(RenderBatch& batch, const std::vector<Object*>& objects, VkPipelineLayout pipelineLayout);
};

