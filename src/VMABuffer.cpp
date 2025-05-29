#include "VMABuffer.h"

#include <cassert>

VkDeviceSize VMABuffer::getAlignment(VkDeviceSize instanceSize, VkDeviceSize minOffsetAlignment)
{
    if (minOffsetAlignment > 0)
    {
        return (instanceSize + minOffsetAlignment - 1) & ~(minOffsetAlignment - 1);
    }
    return instanceSize;
}


VMABuffer::VMABuffer(Device& device,
    VkDeviceSize instanceSize,
    uint32_t instanceCount,
    VkBufferUsageFlags usageFlags,
    VmaMemoryUsage memoryUsage,
    VkDeviceSize minOffsetAlignment)
    : m_device(device),
    m_instanceSize(instanceSize),
    m_instanceCount(instanceCount),
    m_usageFlags(usageFlags)
{
    m_alignmentSize = getAlignment(instanceSize, minOffsetAlignment);
    m_bufferSize = m_alignmentSize * instanceCount;

    device.createBufferVMA(m_bufferSize,
        m_usageFlags,
        memoryUsage,
        m_buffer,
        m_allocation);
}

VMABuffer::~VMABuffer()
{
    if (m_mapped)
        unmap();
    if (m_buffer != VK_NULL_HANDLE && m_allocation)
    {
        vmaDestroyBuffer(m_device.allocator(), m_buffer, m_allocation);
    }
}

VkResult VMABuffer::map(VkDeviceSize size, VkDeviceSize offset)
{
    assert(m_allocation && "Buffer must have a valid allocation");
    if (m_mapped)
        unmap();
    void* data = nullptr;
    return vmaMapMemory(m_device.allocator(), m_allocation, &m_mapped);
}

void VMABuffer::unmap()
{
    if (m_mapped)
    {
        vmaUnmapMemory(m_device.allocator(), m_allocation);
        m_mapped = nullptr;
    }
}

void VMABuffer::writeToBuffer(void* data, VkDeviceSize size, VkDeviceSize offset)
{
    assert(m_mapped && "Cannot copy to unmapped buffer");

    if (size == VK_WHOLE_SIZE)
    {
        memcpy(m_mapped, data, m_bufferSize);
    }
    else
    {
        char* memOffset = (char*)m_mapped;
        memOffset += offset;
        memcpy(memOffset, data, size);
    }
}

VkResult VMABuffer::flush(VkDeviceSize size, VkDeviceSize offset)
{
    assert(m_allocation && "Buffer must have a valid allocation");
    return vmaFlushAllocation(m_device.allocator(), m_allocation, offset, size);

}

VkResult VMABuffer::invalidate(VkDeviceSize size, VkDeviceSize offset)
{
    assert(m_allocation && "Buffer must have a valid allocation");
    return vmaInvalidateAllocation(m_device.allocator(), m_allocation, offset, size);
}

VkDescriptorBufferInfo VMABuffer::descriptorInfo(VkDeviceSize size, VkDeviceSize offset)
{
    return VkDescriptorBufferInfo{ m_buffer,offset,size };
}

void VMABuffer::writeToIndex(void* data, int index)
{
    writeToBuffer(data, m_instanceSize, index * m_alignmentSize);
}

VkResult VMABuffer::flushIndex(int index)
{
    return flush(m_alignmentSize, index * m_alignmentSize);
}

VkDescriptorBufferInfo VMABuffer::decriptorInfoForIndex(int index)
{
    return descriptorInfo(m_alignmentSize, index * m_alignmentSize);
}

VkResult VMABuffer::invalidateIndex(int index)
{
    return invalidate(m_alignmentSize, index * m_alignmentSize);
}




