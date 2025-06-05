#include "SceneManager.h"

SceneManager::SceneManager(MyVulkanWindow& window,
    Device& device,
    VkDescriptorSetLayout globalSetLayout,
    const AABB& worldBounds) :
    m_objectManager(device, worldBounds),
    m_renderManager(window, device, globalSetLayout)
{
    m_objectManager.setObjectUpdateCallback(
        std::bind(&SceneManager::onObjectChanged,
            this,
            std::placeholders::_1));

}

Object::ObjectID SceneManager::addObject(const Object::Builder& builder)
{

    // 首先renderManager 为 Object 分配缓冲
    uint32_t chunkId = m_renderManager.allocateRenderBuffer(builder);

    // 然后ObjectManager 将Object 实例化并且进行管理 
    Object::ObjectID objectId = m_objectManager.createObject(builder, chunkId);
    //TODO: 这段逻辑 也许直接放在createObject中就行
    m_objectManager.updateObject(objectId, [&builder](Object& object)
        {
            object.setColor(builder.color);
            object.setTransform(builder.transform);
        });

    return objectId;
}

void SceneManager::render(FrameInfo& frameInfo)
{
    std::vector<Object*> objectsToRender;
    auto chunks = m_renderManager.getVisibleChunks(frameInfo.camera, ModelType::Line);

    for (auto chunkId : chunks)
    {

        const BufferPool::Chunk* chunk = m_renderManager.getChunk(chunkId);
        std::vector<Object*> chunkObjects = m_objectManager.getVisibleObjects(chunk->bounds);

        for (Object* obj : chunkObjects) {
            if (obj &&
                obj->getChunkID() == chunkId &&
                obj->getModel()->type() == ModelType::Line)
            {
                objectsToRender.push_back(obj);
            }
        }

        if (!objectsToRender.empty()) {
            // 3.1 调用RenderManager进行批量渲染
            m_renderManager.renderObjects(objectsToRender, frameInfo);
        }
    }
}

void SceneManager::onObjectChanged(Object* object)
{

}


