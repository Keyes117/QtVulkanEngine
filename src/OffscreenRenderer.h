#pragma once

#include "Device.h"
#include "FrameInfo.h"
#include "BufferPool.h"
#include "RenderSystem.h"
#include "OffscreenRenderTarget.h"

#include <memory>
class OffscreenRenderer
{
public:
    OffscreenRenderer(Device& deivce);
    ~OffscreenRenderer();

    void startBatchCompositing(VkExtent2D extent, FrameInfo& frameInfo);
    bool renderNextSegment(FrameInfo& frameInfo, const BufferPool& bufferPool,
        RenderSystem* pointSystem = nullptr, RenderSystem* lineSystem = nullptr, RenderSystem* polygonSystem = nullptr);
    void endBatchCompositing();

    void copyToSwapChain(VkCommandBuffer cmd, VkImage& swapChainImage, VkExtent2D extent);

    bool isCompositing() const { return m_isCompositing; }

private:
    Device& m_device;
    bool        m_isCompositing{ false };

    std::unique_ptr<OffscreenRenderTarget> m_offscreenTarget;

    struct CompositingInfo {
        uint32_t currentSegmentIndex{ 0 };
        uint32_t currentTypeIndex{ 0 };
        std::vector<std::pair<ModelType, uint32_t>> renderQueue;
        bool isFirstSegment{ true };
    };

    CompositingInfo m_compositingInfo;

    void buildRenderQueue(const BufferPool& bufferPool);
    void renderSegmentToOffscreen(FrameInfo& frameInfo, ModelType type, uint32_t segmentId,
        const BufferPool& bufferPool, RenderSystem* pointSystem,
        RenderSystem* lineSystem, RenderSystem* polygonSystem);
};

