//#include "OffscreenRenderer.h"
//
//OffscreenRenderer::OffscreenRenderer(Device& device)
//    :m_device(device),
//    m_offscreenTarget(std::make_unique<OffscreenRenderTarget>(m_device))
//{
//}
//
//OffscreenRenderer::~OffscreenRenderer()
//{
//}
//
//void OffscreenRenderer::startBatchCompositing(VkExtent2D extent, FrameInfo& frameInfo)
//{
//    m_isCompositing = true;
//    m_compositingInfo = CompositingInfo{};
//
//    m_offscreenTarget->createResource(extent);
//    m_offscreenTarget->beginRenderPass(frameInfo.commandBuffer);
//}
//
//bool OffscreenRenderer::renderNextSegment(FrameInfo& frameInfo,
//    const BufferPool& bufferPool,
//    RenderSystem* pointSystem,
//    RenderSystem* lineSystem,
//    RenderSystem* polygonSystem)
//{
//    if (!m_isCompositing)
//        return false;
//
//    if (m_compositingInfo.renderQueue.empty())
//    {
//        buildRenderQueue(bufferPool);
//    }
//
//    if (m_compositingInfo.currentSegmentIndex >= m_compositingInfo.renderQueue.size())
//        return false;
//
//    auto [type, segmentId] = m_compositingInfo.renderQueue[m_compositingInfo.currentSegmentIndex];
//
//
//    const auto& bufferSegments = bufferPool.getBufferSegments();
//    auto typeIt = bufferSegments.find(type);
//    if (typeIt == bufferSegments.end() || segmentId >= typeIt->second.size()) {
//        m_compositingInfo.currentSegmentIndex++;
//        return true;
//    }
//
//    const auto& segment = typeIt->second[segmentId];
//    if (segmentId < bufferSegments.size() && bufferSegments[segmentId].isActive) {
//        const auto& segment = bufferSegments[segmentId];
//
//        // 绑定Segment的缓冲区
//        VkBuffer vertexBuffers[] = { segment.vertexBuffer->getBuffer() };
//        VkDeviceSize offsets[] = { 0 };
//        vkCmdBindVertexBuffers(cmd, 0, 1, vertexBuffers, offsets);
//        vkCmdBindIndexBuffer(cmd, segment.indexBuffer->getBuffer(), 0, VK_INDEX_TYPE_UINT32);
//
//        // 执行GPU驱动绘制
//        vkCmdDrawIndexedIndirectCount(cmd,
//            segment.drawCommandBuffer->getBuffer(), 0,
//            segment.drawCountBuffer->getBuffer(), 0,
//            segment.chunks.size(), sizeof(VkDrawIndexedIndirectCommand));
//    }
//
//    // 移动到下一个Segment
//    m_compositingInfo.currentSegmentIndex++;
//    return true;
//
//}
//
//void OffscreenRenderer::endBatchCompositing()
//{
//    m_isCompositing = false;
//    qDebug() << "[OffscreenRenderManager] Batch compositing completed";
//}
//
//void OffscreenRenderer::copyToSwapChain(VkCommandBuffer cmd, VkImage& swapChainImage, VkExtent2D extent)
//{
//    if (m_offscreenTarget && m_offscreenTarget->isInitialized()) {
//        m_offscreenTarget->copyToSwapChain(cmd, swapChainImage, extent);
//    }
//}
//
//void OffscreenRenderer::buildRenderQueue(const BufferPool& bufferPool)
//{
//}
//
//void OffscreenRenderer::renderSegmentToOffscreen(FrameInfo& frameInfo, ModelType type, uint32_t segmentId, const BufferPool& bufferPool, RenderSystem* pointSystem, RenderSystem* lineSystem, RenderSystem* polygonSystem)
//{
//}
//
