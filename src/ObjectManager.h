#pragma once

#include <unordered_map>
#include <vector>

#include "Object.h"
#include "QuadTree.h"
#include "Device.h"
#include "Buffer.h"
#include "FrameInfo.h"

class ObjectManager
{
public:
    ObjectManager(Device& device, const AABB& worldBounds);
    ~ObjectManager();

    Object::ObjectID addObject(Object&& object);
    void removeObject(Object::ObjectID id);
    void updateObject(Object::ObjectID id, const std::function<void(Object&)>& updateFunc);



    Object* getObject(Object::ObjectID id);
    std::vector<Object*>    getObjectInBounds(const AABB& bounds);
    std

private:
    Device& m_device;
    QuadTree                                        m_spatialIndex;
    std::unordered_map<Object::ObjectID, Object>    m_objects;

    VkCommandPool                                   m_commandPool;

    void createCommandPool();
    void onObjectUpdate(Object* object);

    void invalidateAffectedBatches(Object* object);
};

