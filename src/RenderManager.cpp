#include "RenderManager.h"

#ifdef max
#undef max
#endif

RenderManager::RenderManager(MyVulkanWindow& window, Device& device, VkDescriptorSetLayout globalSetLayout) :
    m_device(device),
    m_renderer(window, device),
    m_BufferPool(device),
    m_gpuCuller(std::make_unique<GpuFrustumCuller>(device))
{
    m_pointRenderSystem = std::make_unique<RenderSystem>(
        device,
        m_renderer.getSwapChainRenderPass(),
        globalSetLayout,
        VK_PRIMITIVE_TOPOLOGY_POINT_LIST
    );

    m_lineRenderSystem = std::make_unique<RenderSystem>(
        device,
        m_renderer.getSwapChainRenderPass(),
        globalSetLayout,
        VK_PRIMITIVE_TOPOLOGY_LINE_LIST
    );

    m_polygonRenderSystem = std::make_unique<RenderSystem>(
        device,
        m_renderer.getSwapChainRenderPass(),
        globalSetLayout,
        VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST
    );

    m_gpuCuller->initialize(10000);
    m_BufferPool.setSegmentUpdateCallback(std::bind(&RenderManager::drawCommandBufferUpdateCallback, this));
}

VkCommandBuffer RenderManager::beginFrame()
{
    return m_renderer.beginFrame();
}

void RenderManager::endFrame()
{
    m_renderer.endFrame();
}

void RenderManager::beginSwapChainRenderPass(VkCommandBuffer commandBuffer)
{
    m_renderer.beginSwapChainRenderPass(commandBuffer);
}

void RenderManager::endSwapChainRenderPass(VkCommandBuffer commandBuffer)
{
    m_renderer.endSwapChainRenderPass(commandBuffer);
}

void RenderManager::renderGPUDriven(const ObjectManager::ObjectDataPool& dataPool, FrameInfo& frameInfo)
{
    auto renderStart = std::chrono::high_resolution_clock::now();

    // 执行GPU驱动绘制
    executeGPUDrivenDraw(frameInfo);

    vkDeviceWaitIdle(m_device.device());
    m_gpuCuller->readDebugData();

    auto renderEnd = std::chrono::high_resolution_clock::now();
    auto renderTime = std::chrono::duration<float, std::milli>(renderEnd - renderStart).count();

    static int renderFrameCount = 0;
    if (++renderFrameCount % 60 == 0) {
        qDebug() << "[GPU-Render] 绘制时间:" << renderTime << "ms";
    }
}




uint32_t RenderManager::allocateRenderBuffer(const Object::Builder& builder)
{
    return m_BufferPool.allocateBuffer(builder);

}

RenderSystem* RenderManager::getRenderSystemByType(ModelType type)
{
    switch (type) {
    case ModelType::Point:   return m_pointRenderSystem.get();
    case ModelType::Line:    return m_lineRenderSystem.get();
    case ModelType::Polygon: return m_polygonRenderSystem.get();
    default: return nullptr;
    }
}

ModelType RenderManager::getChunkModelType(uint32_t chunkId)
{
    return m_BufferPool.getChunkType(chunkId);
}

void RenderManager::initializeGPUCulling(const ObjectManager::ObjectDataPool& dataPool)
{
    qDebug() << "[GPU-Driven] 初始化GPU剔除系统...";

    uint32_t objectCount = dataPool.m_count;
    m_gpuCuller->initialize(objectCount);

    qDebug() << "[GPU-Driven] GPU剔除系统就绪，最大容量:" << objectCount;
}

void RenderManager::uploadObjectDataToGPU(const ObjectManager::ObjectDataPool& dataPool)
{
    auto uploadStart = std::chrono::high_resolution_clock::now();

    // 检查容量
    if (dataPool.m_count > 100000) {
        m_gpuCuller->resize(static_cast<uint32_t>(dataPool.m_count * 1.3f));
    }

    // 上传数据
    m_gpuCuller->uploadObjectDataFromPool(dataPool, m_BufferPool);

    auto uploadEnd = std::chrono::high_resolution_clock::now();
    auto uploadTime = std::chrono::duration<float, std::milli>(uploadEnd - uploadStart).count();

    qDebug() << "[GPU-Driven] 对象数据上传完成 -" << dataPool.m_count << "个对象，耗时:" << uploadTime << "ms";
}

void RenderManager::executeGPUCulling(const ObjectManager::ObjectDataPool& dataPool, FrameInfo& frameInfo)
{
    auto cullingStart = std::chrono::high_resolution_clock::now();

    // 1. 首次初始化
    if (!m_gpuCullingInitialized) {
        initializeGPUCulling(dataPool);
        m_gpuCullingInitialized = true;
    }

    // 2. 上传对象数据（首次或变化时）
    if (m_gpuDataNeedsUpdate) {
        uploadObjectDataToGPU(dataPool);
        m_gpuDataNeedsUpdate = false;
    }

    m_gpuCuller->updateSegmentAddresses(m_BufferPool);
    // 3. 执行GPU剔除（在RenderPass外部）
    m_gpuCuller->performCullingGPUOnly(frameInfo, m_BufferPool);

    auto cullingEnd = std::chrono::high_resolution_clock::now();
    auto cullingTime = std::chrono::duration<float, std::milli>(cullingEnd - cullingStart).count();

    static int cullingFrameCount = 0;
    if (++cullingFrameCount % 60 == 0) {
        qDebug() << "[GPU-Culling] 剔除时间:" << cullingTime << "ms, 对象数:" << dataPool.m_count;
    }
}

void RenderManager::executeGPUDrivenDraw(FrameInfo& frameInfo)
{
    // 获取GPU生成的命令缓冲区
    //VkBuffer drawCommandBuffer = m_gpuCuller->getDrawCommandBUffer();
    //VkBuffer drawCountBuffer = m_gpuCuller->getDrawCountBuffer();

    // 按类型绘制
    std::array<ModelType, 3> types = { ModelType::Point, ModelType::Line, ModelType::Polygon };

    for (ModelType type : types) {
        auto* renderSystem = getRenderSystemByType(type);
        if (renderSystem) {
            renderSystem->bind(frameInfo.commandBuffer);
            // 绑定全局描述符集
            vkCmdBindDescriptorSets(
                frameInfo.commandBuffer,
                VK_PIPELINE_BIND_POINT_GRAPHICS,
                renderSystem->getPipelineLayout(),
                0,  // set 0
                1,  // 描述符集数量
                &frameInfo.globalDescriptorSet,  //  使用FrameInfo中的全局描述符集
                0, nullptr
            );

            m_BufferPool.drawGPUDriven(frameInfo.commandBuffer, type);
        }
    }
}

void RenderManager::drawCommandBufferUpdateCallback()
{
    if (m_gpuCuller && m_gpuCuller->isInit())
    {
        m_gpuCuller->setSegmentUpdateFlag(true);
        m_gpuCuller->updateSegmentAddresses(m_BufferPool);
    }
}


