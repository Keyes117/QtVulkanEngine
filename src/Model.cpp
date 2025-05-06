#include "Model.h"

#include <cassert>

Model::Model(Device& device, const std::vector<Vertex>& vertices)
    :m_device{ device }
{
    createVertexBuffers(vertices);
}

Model::~Model()
{
    vkDestroyBuffer(m_device.device(), m_vertexBuffer, nullptr);
    vkFreeMemory(m_device.device(), m_vertexBufferMemory, nullptr);
}

void Model::draw(VkCommandBuffer commandBuffer)
{
    vkCmdDraw(commandBuffer, m_vertexCount, 1, 0, 0);
}

void Model::bind(VkCommandBuffer commandBuffer)
{
    VkBuffer buffers[] = { m_vertexBuffer };
    VkDeviceSize offsets[] = { 0 };
    vkCmdBindVertexBuffers(commandBuffer, 0, 1, buffers, offsets);
}

void Model::createVertexBuffers(const std::vector<Vertex>& vertices)
{
    m_vertexCount = static_cast<uint32_t>(vertices.size());
    assert(m_vertexCount >= 3 && "Vertex Count must be at lease 3");
    VkDeviceSize bufferSize = sizeof(vertices[0]) * m_vertexCount;

    m_device.createBuffer(
        bufferSize,
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        m_vertexBuffer,
        m_vertexBufferMemory
    );

    void* data;
    vkMapMemory(m_device.device(), m_vertexBufferMemory, 0, bufferSize, 0, &data);
    memcpy(data, vertices.data(), static_cast<size_t>(bufferSize));
    vkUnmapMemory(m_device.device(), m_vertexBufferMemory);
}

std::vector<VkVertexInputBindingDescription> Model::Vertex::getBindingDescription()
{
    std::vector<VkVertexInputBindingDescription> bindingDescription(1);
    bindingDescription[0].binding = 0;
    bindingDescription[0].stride = sizeof(Vertex);
    bindingDescription[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    return bindingDescription;
}

std::vector<VkVertexInputAttributeDescription> Model::Vertex::getAttributeDescription()
{
    std::vector<VkVertexInputAttributeDescription> attributeDesciption(2);
    attributeDesciption[0].binding = 0;
    attributeDesciption[0].location = 0;
    attributeDesciption[0].format = VK_FORMAT_R32G32_SFLOAT;
    attributeDesciption[0].offset = offsetof(Vertex, position);

    attributeDesciption[1].binding = 0;
    attributeDesciption[1].location = 1;
    attributeDesciption[1].format = VK_FORMAT_R32G32B32_SFLOAT;
    attributeDesciption[1].offset = offsetof(Vertex, color);

    return attributeDesciption;
}
