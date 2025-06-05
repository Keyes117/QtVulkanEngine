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

    // ����renderManager Ϊ Object ���仺��
    uint32_t chunkId = m_renderManager.allocateRenderBuffer(builder);

    // Ȼ��ObjectManager ��Object ʵ�������ҽ��й��� 
    Object::ObjectID objectId = m_objectManager.createObject(builder, chunkId);
    //TODO: ����߼� Ҳ��ֱ�ӷ���createObject�о���
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
            // 3.1 ����RenderManager����������Ⱦ
            m_renderManager.renderObjects(objectsToRender, frameInfo);
        }
    }
}

void SceneManager::onObjectChanged(Object* object)
{

}


