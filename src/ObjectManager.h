#pragma once

#include <unordered_map>
#include <vector>

#include "Object.h"
#include "QuadTree.h"
#include "Device.h"
#include "Buffer.h"
#include "FrameInfo.h"

using UpdateCallback = std::function<void(const std::shared_ptr<Object>&)>;
using UpdateFunc = std::function<void(std::shared_ptr<Object>&)>;

class ObjectManager
{
public:
    ObjectManager(Device& device, const AABB& worldBounds);
    ~ObjectManager() = default;

    Object::ObjectID createObject(const Object::Builder& builder, uint32_t chunkId);
    void removeObject(Object::ObjectID id);
    void updateObject(Object::ObjectID id, const UpdateFunc& updateFunc);


    std::shared_ptr<Object> getObject(Object::ObjectID id);
    std::vector<std::shared_ptr<Object>> getVisibleObjects(const AABB& bounds);
    std::vector<std::shared_ptr<Object>> getObjectByType(ModelType type);
    std::vector<std::shared_ptr<Object>> getAllObjects();

    void setObjectUpdateCallback(UpdateCallback&& callback)
    {
        m_updateCallback = std::move(callback);
    }

private:
    void onObjectUpdate(const std::shared_ptr<Object>& object);

private:
    Device& m_device;
    QuadTree                                                            m_spatialIndex;
    std::unordered_map<Object::ObjectID, std::shared_ptr<Object>>       m_objects;

    UpdateCallback                                                      m_updateCallback{ nullptr };


};

