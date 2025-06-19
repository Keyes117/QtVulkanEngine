#pragma once

#include <mutex>
#include <unordered_map>
#include <vector>

#include "Object.h"
#include "Device.h"
#include "Buffer.h"
#include "FrameInfo.h"

using UpdateCallback = std::function<void(const std::shared_ptr<Object>&)>;
using UpdateFunc = std::function<void(std::shared_ptr<Object>&)>;



class ObjectManager
{
public:
    using ObjectID = uint64_t;
    static constexpr size_t FLOAT_NUM_PER_TRANSFORM = 16;
    static constexpr size_t FLOAT_NUM_PER_COLOR = 3;
    static constexpr size_t FLOAT_NUM_PER_BOUNDINGBOX = 4;
    struct ObjectDataPool
    {
        std::vector<float> m_transforms;    //16个 float 为一个transform
        std::vector<float> m_colors;        //3 个 float 为一个Color（RGB）
        std::vector<float> m_boundingBoxes; //4 个 float 为一个boundingBox

        std::vector<uint32_t> m_chunkIds;
        std::vector<uint64_t> m_objectIds;
        std::vector<uint8_t>  m_updateFlags;
        std::vector<uint8_t>  m_lodLevels;

        std::vector<bool>   m_activeFlags;

        size_t m_count = 0;
        size_t m_capacity = 0;

        void reserve(size_t newCapacity)
        {
            m_transforms.reserve(newCapacity * FLOAT_NUM_PER_BOUNDINGBOX);
            m_colors.reserve(newCapacity * FLOAT_NUM_PER_COLOR);
            m_boundingBoxes.reserve(newCapacity * FLOAT_NUM_PER_TRANSFORM);
            m_chunkIds.reserve(newCapacity);
            m_objectIds.reserve(newCapacity);
            m_updateFlags.reserve(newCapacity);
            m_lodLevels.reserve(newCapacity);
            m_activeFlags.reserve(newCapacity);
            m_capacity = newCapacity;
        }
        void clear()
        {
            m_transforms.clear();
            m_colors.clear();
            m_boundingBoxes.clear();
            m_chunkIds.clear();
            m_objectIds.clear();
            m_updateFlags.clear();
            m_lodLevels.clear();
            m_activeFlags.clear();
            m_count = 0;
        }

        const float* getTransformData() const { return m_transforms.data(); }
        const float* getColorData() const { return m_colors.data(); }
        const float* getBoundingBoxData() const { return m_boundingBoxes.data(); }

        size_t getTransformSize() const { return m_transforms.size() * sizeof(float); }
        size_t getColorSize() const { return m_colors.size() * sizeof(float); }
        size_t getBoundingBoxSize() const { return m_boundingBoxes.size() * sizeof(float); }

        // 压缩数据 - 移除非活跃对象
        void compactData();
    };

    struct ObjectHandle
    {
        ObjectID id;
        uint32_t index;     //对应数据在dataPool 中的索引
        bool valid;

        ObjectHandle() : id(0), index(0), valid(false) {}
        ObjectHandle(ObjectID _id, uint32_t _index) : id(_id), index(_index), valid(true) {}

        bool operator==(const ObjectHandle& other) const noexcept {
            return id == other.id && index == other.index && valid == other.valid;
        }

        bool operator!=(const ObjectHandle& other) const noexcept {
            return !(*this == other);
        }

        // 小于比较（用于std::map等有序容器）
        bool operator<(const ObjectHandle& other) const noexcept {
            if (id != other.id) return id < other.id;
            if (index != other.index) return index < other.index;
            return valid < other.valid;
        }

        // 优化的哈希函数
        struct Hash {
            std::size_t operator()(const ObjectHandle& handle) const noexcept {
                // 使用Boost风格的哈希组合
                std::size_t seed = 0;
                hashCombine(seed, handle.id);
                hashCombine(seed, handle.index);
                hashCombine(seed, handle.valid);
                return seed;
            }

        private:
            template<typename T>
            static void hashCombine(std::size_t& seed, const T& value) noexcept {
                std::hash<T> hasher;
                seed ^= hasher(value) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
            }
        };

        // 简化版本（如果只需要基于ID的哈希）
        struct SimpleHash {
            std::size_t operator()(const ObjectHandle& handle) const noexcept {
                return std::hash<ObjectID>{}(handle.id);
            }
        };
    };
    ObjectManager() = default;
    ~ObjectManager() = default;

    /*  void removeObject(Object::ObjectID id);
      void updateObject(Object::ObjectID id, const UpdateFunc& updateFunc);*/

      // 对象生命周期管理
    ObjectHandle createObject(const Object::Builder& builder, uint32_t chunkId);
    void destroyObject(ObjectHandle handle);
    bool isValid(ObjectHandle handle) const;

    // 单个对象数据访问
    void setTransform(ObjectHandle handle, const QMatrix4x4& transform);
    void setColor(ObjectHandle handle, const QVector3D& color);
    void setChunkId(ObjectHandle handle, uint32_t chunkId);
    void setBoundingBox(ObjectHandle handle, const AABB& bounds);
    void setLODLevel(ObjectHandle handle, uint8_t lodLevel);

    QMatrix4x4 getTransform(ObjectHandle handle) const;
    QVector3D getColor(ObjectHandle handle) const;
    uint32_t getChunkId(ObjectHandle handle) const;
    AABB getBoundingBox(ObjectHandle handle) const;
    uint8_t getLODLevel(ObjectHandle handle) const;
    uint32_t getObjectId(ObjectHandle handle) const;

    // 批量操作 - SOA的优势
    void updateTransforms(const std::vector<ObjectHandle>& handles, const std::vector<QMatrix4x4>& transforms);
    void updateColors(const std::vector<ObjectHandle>& handles, const std::vector<QVector3D>& colors);
    void markUpdated(ObjectHandle handle, uint8_t flags = 1);
    void clearUpdateFlags();

    // 查询操作
    std::vector<ObjectHandle> getUpdatedObjects() const;
    std::vector<ObjectHandle> getAllActiveObjects() const;
    std::vector<ObjectHandle> getObjectsByChunk(uint32_t chunkId) const;
    std::vector<ObjectHandle> getObjectsByLOD(uint8_t lodLevel) const;

    // 分组操作
    std::unordered_map<uint32_t, std::vector<ObjectHandle>> groupByChunk() const;
    std::unordered_map<uint8_t, std::vector<ObjectHandle>> groupByLOD() const;

    // SOA数据访问
    const ObjectDataPool& getDataPool() const { return m_dataPool; }
    ObjectDataPool& getDataPool() { return m_dataPool; }

    // 统计信息
    size_t getObjectCount() const { return m_dataPool.m_count; }
    size_t getActiveObjectCount() const;
    size_t getMemoryUsage() const;

    // 内存管理
    void compactMemory();
    void reserve(size_t capacity) { m_dataPool.reserve(capacity); }


private:
    ObjectDataPool                              m_dataPool;
    std::unordered_map<ObjectID, uint32_t>      m_idToIndex;
    std::vector<uint32_t>                       m_freeIndices;
    ObjectID                                    m_nextObjectId = 0;

    mutable std::mutex m_mutex; // 线程安全

    bool isValidInternal(ObjectHandle handle) const;  // 不加锁版本,上层函数已经对m_mutex 加锁时使用这个
    uint32_t allocateIndex();
    void releaseIndex(uint32_t index);
    bool isIndexValid(uint32_t index) const;
    void ensureCapacity(size_t requiredSize);


};

namespace std {
    template<>
    struct hash<ObjectManager::ObjectHandle> {
        std::size_t operator()(const ObjectManager::ObjectHandle& handle) const noexcept {
            // 使用简单有效的哈希组合
            std::size_t h1 = std::hash<ObjectManager::ObjectID>{}(handle.id);
            std::size_t h2 = std::hash<uint32_t>{}(handle.index);
            std::size_t h3 = std::hash<bool>{}(handle.valid);

            // FNV-1a风格的哈希组合
            constexpr std::size_t fnv_prime = 1099511628211ULL;
            constexpr std::size_t fnv_offset = 14695981039346656037ULL;

            std::size_t result = fnv_offset;
            result = (result ^ h1) * fnv_prime;
            result = (result ^ h2) * fnv_prime;
            result = (result ^ h3) * fnv_prime;

            return result;
        }
    };
}