#include "ObjectManager.h"

ObjectManager::ObjectManager(Device& device, const AABB& worldBounds)
    :m_device(device),
    m_spatialIndex(worldBounds)
{
}

Object::ObjectID ObjectManager::createObject(const Object::Builder& builder, uint32_t chunkId)
{
    //产生一个Object
    auto object = Object::createObject();

    //利用ObjectBuilder中的信息 填充ModelBuidler 并且构建model
    Model::Builder modelBuilder;
    modelBuilder.vertices = builder.vertices;
    modelBuilder.indices = builder.indices;
    modelBuilder.type = builder.type;
    auto model = std::make_shared<Model>(m_device, modelBuilder);

    //设置object
    object->setColor(builder.color);
    object->setTransform(builder.transform);
    object->setModel(model);
    object->setChunkId(chunkId);
    auto id = object->getId();
    m_objects[id] = object;
    m_spatialIndex.insert(object);

    object->setUpdateCallback(std::bind(&ObjectManager::onObjectUpdate, this, object));

    return id;
}

void ObjectManager::removeObject(Object::ObjectID id)
{
    auto it = m_objects.find(id);
    if (it != m_objects.end())
    {
        m_spatialIndex.remove(it->second);

        m_objects.erase(it);
    }
}

void ObjectManager::updateObject(Object::ObjectID id, const UpdateFunc& updateFunc)
{

    auto it = m_objects.find(id);
    if (it != m_objects.end())
    {
        auto& object = it->second;

        //临时移除object
        m_spatialIndex.remove(object);

        updateFunc(object);

        //重新添加
        m_spatialIndex.insert(object);

    }
}

std::shared_ptr<Object> ObjectManager::getObject(Object::ObjectID id)
{
    auto it = m_objects.find(id);
    return (it != m_objects.end()) ? it->second : nullptr;
}

std::vector<std::shared_ptr<Object>> ObjectManager::getVisibleObjects(const AABB& bounds)
{
    return m_spatialIndex.query(bounds);
}

std::vector<std::shared_ptr<Object>> ObjectManager::getObjectByType(ModelType type)
{
    std::vector<std::shared_ptr<Object>> result;

    for (auto& [id, object] : m_objects)
    {
        if (object->getModel()->type() == type)
        {
            result.push_back(object);
        }
    }

    return result;
}

std::vector<std::shared_ptr<Object>> ObjectManager::getAllObjects()
{
    std::vector<std::shared_ptr<Object>> result;
    result.reserve(m_objects.size());

    for (auto& [id, object] : m_objects) {
        result.push_back(object);
    }

    return result;
}

void ObjectManager::onObjectUpdate(const std::shared_ptr<Object>& object)
{
    if (object && object->needsUpdate())
    {
        // 先从空间索引中删除
        m_spatialIndex.remove(object);

        // 重新插入到新位置
        m_spatialIndex.insert(object);

        // 通知上层管理器
        if (m_updateCallback)
            m_updateCallback(object);

        object->clearUpdateFlags();
    }
}
