#include "SceneManager.h"

SceneManager::SceneManager(MyVulkanWindow& window,
    Device& device,
    VkDescriptorSetLayout globalSetLayout,
    const AABB& worldBounds) :
    m_objectManager(),
    m_renderManager(window, device, globalSetLayout)
{
}

ObjectManager::ObjectHandle SceneManager::addObject(const Object::Builder& builder)
{
    uint32_t chunkId = m_renderManager.allocateRenderBuffer(builder);
    auto handle = m_objectManager.createObject(builder, chunkId);

    AABB bounds = m_objectManager.getBoundingBox(handle);
    return handle;
}

std::vector<ObjectManager::ObjectHandle> SceneManager::addObjectBatch(const std::vector<Object::Builder>& builders)
{
    return std::vector<ObjectManager::ObjectHandle>();
}


void SceneManager::updateObjectTransform(ObjectManager::ObjectHandle handle, const QMatrix4x4& transform)
{
    if (!m_objectManager.isValid(handle))
        return;

    m_objectManager.setTransform(handle, transform);
}

void SceneManager::updateObjectColor(ObjectManager::ObjectHandle handle, const QVector3D& color)
{
    if (!m_objectManager.isValid(handle))
        return;

    m_objectManager.setColor(handle, color);
}

void SceneManager::culling(FrameInfo& frameInfo)
{

    const auto& dataPool = m_objectManager.getDataPool();

    m_renderManager.executeGPUCulling(dataPool, frameInfo);
}

void SceneManager::render(FrameInfo& frameInfo)
{
    //m_renderManager.testRenderSegment(frameInfo);
    //return;

    auto renderStart = std::chrono::high_resolution_clock::now();

    m_frameCounter++;

    const auto& dataPool = m_objectManager.getDataPool();

    m_renderManager.renderGPUDriven(dataPool, frameInfo);
    m_objectManager.clearUpdateFlags();

    auto renderEnd = std::chrono::high_resolution_clock::now();
    auto renderTime = std::chrono::duration<float, std::milli>(renderEnd - renderStart).count();

    qDebug() << "Render frame time:" << renderTime << "ms";

}




