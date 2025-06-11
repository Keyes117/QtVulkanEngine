#include "ObjectManager.h"

void ObjectManager::ObjectDataPool::compactData()
{
    if (m_count == 0)
        return;

    size_t writeIndex = 0;

    for (size_t readIndex = 0; readIndex < m_count; readIndex++)
    {
        if (!m_activeFlags[readIndex])
            continue;

        if (writeIndex != readIndex)
        {
            std::copy(m_transforms.begin() + readIndex * 16,
                m_transforms.begin() + (readIndex + 1) * 16,
                m_transforms.begin() + writeIndex * 16);


            std::copy(m_colors.begin() + readIndex * 4,
                m_colors.begin() + (readIndex + 1) * 4,
                m_colors.begin() + writeIndex * 4);

            std::copy(m_boundingBoxes.begin() + readIndex * 6,
                m_boundingBoxes.begin() + (readIndex + 1) * 6,
                m_boundingBoxes.begin() + writeIndex * 6);

            // 移动元数据
            m_chunkIds[writeIndex] = m_chunkIds[readIndex];
            m_objectIds[writeIndex] = m_objectIds[readIndex];
            m_updateFlags[writeIndex] = m_updateFlags[readIndex];
            m_lodLevels[writeIndex] = m_lodLevels[readIndex];
            m_activeFlags[writeIndex] = true;
        }

        writeIndex++;
    }

    m_count = writeIndex;
    m_transforms.resize(m_count * 16);
    m_colors.resize(m_count * 4);
    m_boundingBoxes.resize(m_count * 6);
    m_chunkIds.resize(m_count);
    m_objectIds.resize(m_count);
    m_updateFlags.resize(m_count);
    m_lodLevels.resize(m_count);
    m_activeFlags.resize(m_count);
}

ObjectManager::ObjectHandle ObjectManager::createObject(const Object::Builder& builder, uint32_t chunkId)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    uint32_t index = allocateIndex();
    ObjectID id = m_nextObjectId++;

    ensureCapacity(m_dataPool.m_count + 1);

    QMatrix4x4 transform = builder.transform.mat4f();
    const float* matData = transform.constData();
    size_t transformOffset = index * FLOAT_NUM_PER_TRANSFORM;

    if (m_dataPool.m_transforms.size() <= transformOffset + FLOAT_NUM_PER_TRANSFORM - 1)
    {
        m_dataPool.m_transforms.resize((index + 1) * FLOAT_NUM_PER_TRANSFORM);
    }

    std::copy(matData, matData + FLOAT_NUM_PER_TRANSFORM, m_dataPool.m_transforms.begin() + transformOffset);

    size_t colorOffset = index * FLOAT_NUM_PER_COLOR;
    if (m_dataPool.m_colors.size() <= colorOffset + FLOAT_NUM_PER_COLOR - 1)
    {
        m_dataPool.m_colors.resize((index + 1) * FLOAT_NUM_PER_COLOR);
    }

    m_dataPool.m_colors[colorOffset] = builder.color.x();
    m_dataPool.m_colors[colorOffset + 1] = builder.color.y();
    m_dataPool.m_colors[colorOffset + 2] = builder.color.z();

    size_t boundsOffset = index * FLOAT_NUM_PER_BOUNDINGBOX;
    if (m_dataPool.m_boundingBoxes.size() <= boundsOffset + FLOAT_NUM_PER_BOUNDINGBOX - 1)
    {
        m_dataPool.m_boundingBoxes.resize((index + 1) * FLOAT_NUM_PER_BOUNDINGBOX);
    }

    m_dataPool.m_boundingBoxes[boundsOffset] = builder.bounds.minX;
    m_dataPool.m_boundingBoxes[boundsOffset + 1] = builder.bounds.minY;
    //m_dataPool.m_boundingBoxes[boundsOffset] = builder.bounds.minZ; //后续三维数据可能用到
    m_dataPool.m_boundingBoxes[boundsOffset + 2] = builder.bounds.maxX;
    m_dataPool.m_boundingBoxes[boundsOffset + 3] = builder.bounds.maxY;
    //m_dataPool.m_boundingBoxes[boundsOffset] = builder.bounds.maxZ;

    if (m_dataPool.m_chunkIds.size() <= index)
    {
        m_dataPool.m_chunkIds.resize(index + 1);
        m_dataPool.m_objectIds.resize(index + 1);
        m_dataPool.m_updateFlags.resize(index + 1);
        m_dataPool.m_lodLevels.resize(index + 1);
        m_dataPool.m_activeFlags.resize(index + 1);
    }

    m_dataPool.m_chunkIds[index] = chunkId; // 稍后由SceneManager设置
    m_dataPool.m_objectIds[index] = static_cast<uint32_t>(id);
    m_dataPool.m_updateFlags[index] = 1; // 标记为需要更新
    m_dataPool.m_lodLevels[index] = 0;
    m_dataPool.m_activeFlags[index] = true;

    // 更新计数
    if (index >= m_dataPool.m_count) {
        m_dataPool.m_count = index + 1;
    }

    // 建立映射
    m_idToIndex[id] = index;

    return ObjectHandle(id, index);
}

void ObjectManager::destroyObject(ObjectHandle handle)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    if (!isValidInternal(handle))
        return;

    uint32_t index = handle.index;
    m_dataPool.m_activeFlags[index] = false;

    m_idToIndex.erase(handle.id);

    releaseIndex(index);
}

bool ObjectManager::isValid(ObjectHandle handle) const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return isIndexValid(handle.index) &&
        m_idToIndex.find(handle.id) != m_idToIndex.end() &&
        m_idToIndex.at(handle.id) == handle.index;
}

void ObjectManager::setTransform(ObjectHandle handle, const QMatrix4x4& transform)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    if (!isValidInternal(handle))
        return;

    uint32_t index = handle.index;
    const float* matData = transform.constData();
    size_t offset = index * FLOAT_NUM_PER_TRANSFORM;

    std::copy(matData, matData + FLOAT_NUM_PER_TRANSFORM, m_dataPool.m_transforms.begin() + offset);
    m_dataPool.m_updateFlags[index] |= 1;

}

void ObjectManager::setColor(ObjectHandle handle, const QVector3D& color)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!isValidInternal(handle))
        return;

    uint32_t index = handle.index;
    size_t offset = index * FLOAT_NUM_PER_COLOR;

    m_dataPool.m_colors[offset] = color.x();
    m_dataPool.m_colors[offset + 1] = color.y();
    m_dataPool.m_colors[offset + 2] = color.z();

    m_dataPool.m_updateFlags[index] |= 2;
}

void ObjectManager::setChunkId(ObjectHandle handle, uint32_t chunkId)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    if (!isValidInternal(handle)) return;

    m_dataPool.m_chunkIds[handle.index] = chunkId;
}

void ObjectManager::setBoundingBox(ObjectHandle handle, const AABB& bounds)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    if (!isValidInternal(handle)) return;

    uint32_t index = handle.index;
    size_t offset = index * FLOAT_NUM_PER_BOUNDINGBOX;

    m_dataPool.m_boundingBoxes[offset] = bounds.minX;
    m_dataPool.m_boundingBoxes[offset + 1] = bounds.minY;
    //m_dataPool.m_boundingBoxes[offset + 2] = 0.0f;
    m_dataPool.m_boundingBoxes[offset + 2] = bounds.maxX;
    m_dataPool.m_boundingBoxes[offset + 3] = bounds.maxY;
    //m_dataPool.m_boundingBoxes[offset + 5] = 0.0f;
}

void ObjectManager::setLODLevel(ObjectHandle handle, uint8_t lodLevel)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    if (!isValidInternal(handle)) return;

    m_dataPool.m_lodLevels[handle.index] = lodLevel;
}

QMatrix4x4 ObjectManager::getTransform(ObjectHandle handle) const
{
    std::lock_guard<std::mutex> lock(m_mutex);

    if (!isValidInternal(handle)) return QMatrix4x4();

    uint32_t index = handle.index;
    size_t offset = index * FLOAT_NUM_PER_TRANSFORM;

    QMatrix4x4 transform;
    float* data = transform.data();
    std::copy(m_dataPool.m_transforms.begin() + offset,
        m_dataPool.m_transforms.begin() + offset + FLOAT_NUM_PER_TRANSFORM,
        data);

    return transform;
}

QVector3D ObjectManager::getColor(ObjectHandle handle) const
{
    std::lock_guard<std::mutex> lock(m_mutex);

    if (!isValidInternal(handle)) return QVector3D(1, 1, 1);

    uint32_t index = handle.index;
    size_t offset = index * FLOAT_NUM_PER_COLOR;

    return QVector3D(
        m_dataPool.m_colors[offset],
        m_dataPool.m_colors[offset + 1],
        m_dataPool.m_colors[offset + 2]
    );
}

uint32_t ObjectManager::getChunkId(ObjectHandle handle) const
{
    std::lock_guard<std::mutex> lock(m_mutex);

    if (!isValidInternal(handle)) return 0;

    return m_dataPool.m_chunkIds[handle.index];
}

AABB ObjectManager::getBoundingBox(ObjectHandle handle) const
{
    std::lock_guard<std::mutex> lock(m_mutex);

    if (!isValidInternal(handle)) return AABB{ 0, 0, 0, 0 };

    uint32_t index = handle.index;
    size_t offset = index * FLOAT_NUM_PER_COLOR;

    return AABB{
       m_dataPool.m_boundingBoxes[offset],     // minX
       m_dataPool.m_boundingBoxes[offset + 1], // minY
       m_dataPool.m_boundingBoxes[offset + 2], // maxX
       m_dataPool.m_boundingBoxes[offset + 3]  // maxY
    };
}

uint8_t ObjectManager::getLODLevel(ObjectHandle handle) const
{
    std::lock_guard<std::mutex> lock(m_mutex);

    if (!isValidInternal(handle)) return 0;

    return m_dataPool.m_lodLevels[handle.index];
}

uint32_t ObjectManager::getObjectId(ObjectHandle handle) const
{
    std::lock_guard<std::mutex> lock(m_mutex);

    if (!isValidInternal(handle)) return 0;

    return m_dataPool.m_objectIds[handle.index];
}
void ObjectManager::updateTransforms(const std::vector<ObjectHandle>& handles, const std::vector<QMatrix4x4>& transforms)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    if (handles.size() != transforms.size()) return;

    for (size_t i = 0; i < handles.size(); ++i) {
        if (isValidInternal(handles[i])) {
            uint32_t index = handles[i].index;
            const float* matData = transforms[i].constData();
            size_t offset = index * 16;

            std::copy(matData, matData + 16, m_dataPool.m_transforms.begin() + offset);
            m_dataPool.m_updateFlags[index] |= 1;
        }
    }
}
void ObjectManager::updateColors(const std::vector<ObjectHandle>& handles, const std::vector<QVector3D>& colors)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    if (handles.size() != colors.size()) return;

    for (size_t i = 0; i < handles.size(); ++i) {
        if (isValidInternal(handles[i])) {
            uint32_t index = handles[i].index;
            size_t offset = index * 4;

            m_dataPool.m_colors[offset] = colors[i].x();
            m_dataPool.m_colors[offset + 1] = colors[i].y();
            m_dataPool.m_colors[offset + 2] = colors[i].z();

            m_dataPool.m_updateFlags[index] |= 2;
        }
    }
}
void ObjectManager::markUpdated(ObjectHandle handle, uint8_t flags)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    if (!isValidInternal(handle))
        return;
    m_dataPool.m_updateFlags[handle.index] |= flags;
}

void ObjectManager::clearUpdateFlags()
{
    std::lock_guard<std::mutex> lock(m_mutex);

    std::fill(m_dataPool.m_updateFlags.begin(), m_dataPool.m_updateFlags.end(), 0);
}

std::vector<ObjectManager::ObjectHandle> ObjectManager::getUpdatedObjects() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<ObjectHandle> updatedObjects;

    for (size_t i = 0; i < m_dataPool.m_count; i++)
    {
        if (m_dataPool.m_activeFlags[i] && m_dataPool.m_updateFlags[i])
            updatedObjects.emplace_back(m_dataPool.m_objectIds[i], static_cast<uint32_t>(i));
    }

    return updatedObjects;
}

std::vector<ObjectManager::ObjectHandle> ObjectManager::getAllActiveObjects() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<ObjectHandle> activeObjects;

    for (size_t i = 0; i < m_dataPool.m_count; i++)
    {
        if (m_dataPool.m_activeFlags[i])
            activeObjects.emplace_back(m_dataPool.m_objectIds[i], static_cast<uint32_t>(i));
    }

    return activeObjects;
}

std::vector<ObjectManager::ObjectHandle> ObjectManager::getObjectsByChunk(uint32_t chunkId) const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<ObjectHandle> result;

    for (size_t i = 0; i < m_dataPool.m_count; i++)
    {
        if (m_dataPool.m_activeFlags[i] && m_dataPool.m_chunkIds[i] == chunkId)
            result.emplace_back(m_dataPool.m_objectIds[i], static_cast<uint32_t>(i));
    }

    return result;
}

std::vector<ObjectManager::ObjectHandle> ObjectManager::getObjectsByLOD(uint8_t lodLevel) const
{
    return std::vector<ObjectHandle>();
}

std::unordered_map<uint32_t, std::vector<ObjectManager::ObjectHandle>> ObjectManager::groupByChunk() const
{
    std::lock_guard<std::mutex> lock(m_mutex);

    std::unordered_map<uint32_t, std::vector<ObjectHandle>> groups;

    for (size_t i = 0; i < m_dataPool.m_count; ++i) {
        if (m_dataPool.m_activeFlags[i]) {
            uint32_t chunkId = m_dataPool.m_chunkIds[i];
            groups[chunkId].emplace_back(m_dataPool.m_objectIds[i], static_cast<uint32_t>(i));
        }
    }

    return groups;
}

std::unordered_map<uint8_t, std::vector<ObjectManager::ObjectHandle>> ObjectManager::groupByLOD() const
{
    return std::unordered_map<uint8_t, std::vector<ObjectHandle>>();
}

size_t ObjectManager::getActiveObjectCount() const
{
    std::lock_guard<std::mutex> lock(m_mutex);

    return std::count(m_dataPool.m_activeFlags.begin(),
        m_dataPool.m_activeFlags.begin() + m_dataPool.m_count, true);
}

size_t ObjectManager::getMemoryUsage() const
{
    std::lock_guard<std::mutex> lock(m_mutex);

    return  m_dataPool.getTransformSize() +
        m_dataPool.getColorSize() +
        m_dataPool.getBoundingBoxSize() +
        m_dataPool.m_chunkIds.size() * sizeof(uint32_t) +
        m_dataPool.m_objectIds.size() * sizeof(uint32_t) +
        m_dataPool.m_updateFlags.size() * sizeof(uint8_t) +
        m_dataPool.m_lodLevels.size() * sizeof(uint8_t) +
        m_dataPool.m_activeFlags.size() * sizeof(bool);
}

void ObjectManager::compactMemory()
{
    std::lock_guard<std::mutex> lock(m_mutex);

    m_dataPool.compactData();

    // 重建ID到索引的映射
    m_idToIndex.clear();
    for (size_t i = 0; i < m_dataPool.m_count; ++i) {
        if (m_dataPool.m_activeFlags[i]) {
            m_idToIndex[m_dataPool.m_objectIds[i]] = static_cast<uint32_t>(i);
        }
    }

    // 清空自由索引列表
    m_freeIndices.clear();
}

bool ObjectManager::isValidInternal(ObjectHandle handle) const
{
    return isIndexValid(handle.index) &&
        m_idToIndex.find(handle.id) != m_idToIndex.end() &&
        m_idToIndex.at(handle.id) == handle.index;
}

uint32_t ObjectManager::allocateIndex()
{
    if (!m_freeIndices.empty())
    {
        uint32_t index = m_freeIndices.back();
        m_freeIndices.pop_back();
        return index;
    }

    return static_cast<uint32_t>(m_dataPool.m_count);
}

void ObjectManager::releaseIndex(uint32_t index)
{
    m_freeIndices.push_back(index);
}

bool ObjectManager::isIndexValid(uint32_t index) const
{
    return index < m_dataPool.m_count && m_dataPool.m_activeFlags[index];
}

void ObjectManager::ensureCapacity(size_t requiredSize)
{
    if (requiredSize > m_dataPool.m_capacity) {
        size_t newCapacity = qMax(requiredSize, m_dataPool.m_capacity * 2);
        m_dataPool.reserve(newCapacity);
    }
}

