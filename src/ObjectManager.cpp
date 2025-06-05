#include "ObjectManager.h"

ObjectManager::ObjectManager(Device& device, const AABB& worldBounds)
    :m_device(device),
    m_spatialIndex(worldBounds)
{
}

Object::ObjectID ObjectManager::createObject(const Object::Builder& builder, uint32_t chunkId)
{
    //����һ��Object
    Object object = Object::createObject();

    //����ObjectBuilder�е���Ϣ ���ModelBuidler ���ҹ���model
    Model::Builder modelBuilder;
    modelBuilder.vertices = builder.vertices;
    modelBuilder.indices = builder.indices;
    modelBuilder.type = builder.type;
    auto model = std::make_shared<Model>(m_device, modelBuilder);

    //����object
    object.setModel(model);
    auto id = object.getId();
    m_objects[id] = object;
    m_spatialIndex.insert(&object);

    object.setUpdateCallback(std::bind(&ObjectManager::onObjectUpdate, this, &object));

    return id;
}

void ObjectManager::removeObject(Object::ObjectID id)
{
    auto it = m_objects.find(id);
    if (it != m_objects.end())
    {
        m_spatialIndex.remove(&it->second);

        m_objects.erase(it);
    }
}

void ObjectManager::updateObject(Object::ObjectID id, const UpdateFunc& updateFunc)
{

    auto it = m_objects.find(id);
    if (it != m_objects.end())
    {
        auto& object = it->second;

        //��ʱ�Ƴ�object
        m_spatialIndex.remove(&object);

        updateFunc(object);

        //�������
        m_spatialIndex.insert(&object);

    }
}

Object* ObjectManager::getObject(Object::ObjectID id)
{
    auto it = m_objects.find(id);
    return (it != m_objects.end()) ? &it->second : nullptr;
}

std::vector<Object*> ObjectManager::getVisibleObjects(const AABB& bounds)
{
    return m_spatialIndex.query(bounds);
}

std::vector<Object*> ObjectManager::getObjectByType(ModelType type)
{
    std::vector<Object*> result;

    for (auto& [id, object] : m_objects)
    {
        if (object.getModel()->type() == type)
        {
            result.push_back(&object);
        }
    }

    return result;
}

std::vector<Object*> ObjectManager::getAllObjects()
{
    std::vector<Object*> result;
    result.reserve(m_objects.size());

    for (auto& [id, object] : m_objects) {
        result.push_back(&object);
    }

    return result;
}

void ObjectManager::onObjectUpdate(Object* object)
{
    if (object && object->needsUpdate())
    {
        //�ռ���������
        m_spatialIndex.insert(object);

        //֪ͨ�ϲ������
        if (m_updateCallback)
            m_updateCallback(object);

        object->clearUpdateFlags();
    }
}
