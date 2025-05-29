#pragma once

#include "Device.h"

class VMABuffer
{
public:
    VMABuffer(
        Device& device,
        VkDeviceSize  instanceSize,
        uint32_t    instanceCount,
        VkBufferUsageFlags  usageFlags,
        VmaMemoryUsage memoryPropertyFlags,
        VkDeviceSize minOffsetAlignment = 1);
    ~VMABuffer();


    VMABuffer(const VMABuffer&) = delete;
    VMABuffer& operator=(const VMABuffer&) = delete;
    VMABuffer(VMABuffer&&) = default;
    VMABuffer& operator=(VMABuffer&&) = default;

    VkResult map(VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize offset = 0);
    void unmap();

    void writeToBuffer(void* data, VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize offset = 0);
    VkResult flush(VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize offset = 0);
    VkResult invalidate(VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize offset = 0);
    VkDescriptorBufferInfo  descriptorInfo(VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize offset = 0);


    void writeToIndex(void* data, int index);
    VkResult flushIndex(int index);
    VkDescriptorBufferInfo decriptorInfoForIndex(int index);
    VkResult invalidateIndex(int index);

    VkBuffer getBuffer() const { return m_buffer; }
    void* getMappedMemory() const { return m_mapped; }
    uint32_t getInstanceCount() const { return m_instanceCount; }
    VkDeviceSize getInstanceSize() const { return m_instanceSize; }
    VkDeviceSize getAlignmentSize() const { return m_alignmentSize; }
    VkBufferUsageFlags getUsageFlags() const { return m_usageFlags; }
    VmaAllocation getAllocation() const { return m_allocation; }
    VkDeviceSize getBufferSize() const { return m_bufferSize; }

private:
    static VkDeviceSize getAlignment(VkDeviceSize instanceSize, VkDeviceSize minOffsetAlignment);

    Device& m_device;
    void* m_mapped = nullptr;

    VkBuffer                    m_buffer = VK_NULL_HANDLE;

    VkDeviceSize                m_bufferSize;
    uint32_t                    m_instanceCount;
    VkDeviceSize                m_instanceSize;
    VkDeviceSize                m_alignmentSize;
    VkBufferUsageFlags          m_usageFlags;
    VmaAllocation               m_allocation;
};

