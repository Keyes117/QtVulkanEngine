#include "ObjectManager.h"

ObjectManager::ObjectManager(Device& device, const AABB& worldBounds)\
    :m_device(device),
    m_spatialIndex(worldBounds)
{
    createCommandPool();
}

ObjectManager::~ObjectManager()
{
    for (auto& [_, batch] : m_batches)
    {
        if (batch.commandBuffer != VK_NULL_HANDLE)
        {
            vkFreeCommandBuffers(m_device.device(), m_commandPool, 1, nullptr);
        }
    }

    if (m_commandPool != VK_NULL_HANDLE)
    {
        vkDestroyCommandPool(m_device.device(), m_commandPool, nullptr);
    }

}

Object::ObjectID ObjectManager::addObject(Object&& object)
{
    auto id = object.getId();

    object.setUpdateCallback([this](Object* obj)
        {
            onObjectUpdate(obj);
        });

    m_objects[id] = std::move(object);
    auto& storeObject = m_objects[id];

    m_spatialIndex.insert(&storeObject);

    invalidateAffectedBatches(&storeObject);


    return id;
}

void ObjectManager::removeObject(Object::ObjectID id)
{
    auto it = m_objects.find(id);
    if (it != m_objects.end())
    {
        m_spatialIndex.remove(&it->second);
        invalidateAffectedBatches(&it->second);
        m_objects.erase(it);
    }
}

void ObjectManager::updateObject(Object::ObjectID id, const std::function<void(Object&)>& updateFunc)
{

    auto it = m_objects.find(id);
    if (it != m_objects.end())
    {
        auto& object = it->second;

        m_spatialIndex.remove(&object);

        updateFunc(object);

        m_spatialIndex.insert(&object);
        invalidateAffectedBatches(&object);
    }
}

void ObjectManager::render(FrameInfo& frameInfo, VkPipelineLayout pipelineLayout)
{
    Camera::Frustum2D frustum = frameInfo.camera.getFrustum2D();

    std::vector<Object*> visibleObjects;
    for (auto& [id, object] : m_objects) {
        AABB objBounds = object.getBoundingBox();
        if (frustum.insersects(objBounds)) {
            visibleObjects.push_back(&object);
        }
    }

    // 按模型分组
    std::unordered_map<Model*, std::vector<Object*>> objectsByModel;
    for (auto* object : visibleObjects) {
        if (object->getModel()) {
            objectsByModel[object->getModel().get()].push_back(object);
        }
    }

    // 更新和渲染批次
    for (auto& [model, objects] : objectsByModel) {
        auto& batch = getBatch(model);
        if (batch.needsUpdate) {
            updateBatch(batch, objects);
        }

        // 执行渲染
        if (batch.commandBuffer != VK_NULL_HANDLE && !objects.empty()) {
            vkCmdExecuteCommands(frameInfo.commandBuffer, 1, &batch.commandBuffer);
        }
    }
}

Object* ObjectManager::getObject(Object::ObjectID id)
{
    auto it = m_objects.find(id);
    return (it != m_objects.end()) ? &it->second : nullptr;
}

std::vector<Object*> ObjectManager::getObjectInBounds(const AABB& bounds)
{
    return m_spatialIndex.query(bounds);
}

void ObjectManager::createCommandPool()
{
    VkCommandPoolCreateInfo  poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    poolInfo.queueFamilyIndex = m_device.getGraphicsQueueFamily();

    if (vkCreateCommandPool(m_device.device(), &poolInfo, nullptr, &m_commandPool))
    {
        throw std::runtime_error("failed to create command pool for ObjectManager!");
    }
}

void ObjectManager::onObjectUpdate(Object* object)
{
    if (object)
    {
        invalidateAffectedBatches(object);
    }
}

ObjectManager::RenderBatch& ObjectManager::getBatch(Model* model)
{
    auto it = m_batches.find(model);
    if (it == m_batches.end())
    {
        RenderBatch batch;

        batch.instanceBuffer = std::make_unique<VMABuffer>(
            m_device,
            sizeof(InstanceData),
            100,
            VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
            VMA_MEMORY_USAGE_CPU_ONLY);
        batch.bufferCapacity = 100;

        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.commandPool = m_commandPool;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_SECONDARY;
        allocInfo.commandBufferCount = 1;

        vkAllocateCommandBuffers(m_device.device(), &allocInfo, &batch.commandBuffer);

        auto [newIt, _] = m_batches.emplace(model, std::move(batch));
        return newIt->second;
    }

    return it->second;
}

void ObjectManager::updateBatch(RenderBatch& batch, const std::vector<Object*>& objects)
{
    if (objects.empty())
    {
        batch.needsUpdate = false;
        return;
    }

    if (objects.size() > batch.bufferCapacity)
    {
        size_t newCapacity = objects.size() * 2;

        batch.instanceBuffer = std::make_unique<VMABuffer>(
            m_device,
            sizeof(InstanceData),
            newCapacity,
            VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
            VMA_MEMORY_USAGE_CPU_ONLY);

        batch.bufferCapacity = newCapacity;
    }

    std::vector<InstanceData> instanceDatas;
    instanceDatas.reserve(objects.size());

    for (auto* object : objects)
    {
        InstanceData data;
        data.transform = object->getTransform().mat4f();
        data.color = object->getColor();
        data.padding = 0.f;
        instanceDatas.push_back(data);
    }

    batch.instanceBuffer->writeToBuffer(instanceDatas.data(),
        instanceDatas.size() * sizeof(InstanceData));

    recordBatchCommands(batch, objects, VK_NULL_HANDLE); // 这里需要传入正确的pipeline layout

    batch.needsUpdate = false;
}

void ObjectManager::recordBatchCommands(RenderBatch& batch,
    const std::vector<Object*>& objects,
    VkPipelineLayout pipelineLayout)
{
    if (objects.empty())
        return;

    VkCommandBufferInheritanceInfo inheritanceInfo{};
    inheritanceInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO;
    inheritanceInfo.renderPass =

        VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT;
    vkBeginCommandBuffer(batch.commandBuffer, &beginInfo);

    auto* model = objects[0]->getModel().get();
    model->bind(batch.commandBuffer);

    VkBuffer instanceBuffer = batch.instanceBuffer->getBuffer();
    VkDeviceSize offset = 0;
    vkCmdBindVertexBuffers(batch.commandBuffer, 1, 1, &instanceBuffer, &offset);

    if (model->hasIndexBuffer())
    {
        vkCmdDrawIndexed(batch.commandBuffer,
            model->indexCount(),
            static_cast<uint32_t>(objects.size()),
            0, 0, 0);
    }
    else
    {
        vkCmdDraw(batch.commandBuffer,
            model->vertexCount(),
            static_cast<uint32_t>(objects.size()),
            0, 0
        );
    }
}

void ObjectManager::invalidateAffectedBatches(Object* object)
{
    if (object && object->getModel())
    {
        auto it = m_batches.find(object->getModel().get());
        if (it != m_batches.end())
        {
            it->second.needsUpdate = true;
        }
    }
}
