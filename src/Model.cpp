#include "Model.h"

#include <cassert>
#include <limits>

#ifdef max
#undef max
#endif

#ifdef min
#undef min
#endif

Model::Model(Device& device, Model::Builder& builder)
    : m_device(device),
    m_type(builder.type),
    m_vertices(std::move(builder.vertices)),
    m_indices(std::move(builder.indices)),
    m_vertexCount(m_vertices.size()),
    m_indexCount(m_indices.size())
{

    //createVertexBuffers(m_vertices);
    //createIndexBuffers(m_indices);
    for (const auto& v : m_vertices) {
        float x = v.position.x();
        float y = v.position.y();
        m_boundingBox.minX = std::min(m_boundingBox.minX, x);
        m_boundingBox.minY = std::min(m_boundingBox.minY, y);
        m_boundingBox.maxX = std::max(m_boundingBox.maxX, x);
        m_boundingBox.maxY = std::max(m_boundingBox.maxY, y);
    }
}
Model::~Model()
{

}


//void Model::bind(VkCommandBuffer commandBuffer)
//{
//    VkBuffer buffers[] = { m_vertexBuffer->getBuffer() };
//    VkDeviceSize offsets[] = { 0 };
//    vkCmdBindVertexBuffers(commandBuffer, 0, 1, buffers, offsets);
//
//    if (m_hasIndexBuffer)
//    {
//        vkCmdBindIndexBuffer(commandBuffer, m_indexBuffer->getBuffer(), 0, VK_INDEX_TYPE_UINT32);
//    }
//}
//void Model::draw(VkCommandBuffer commandBuffer, const Camera& camera)
//{
//    if (!m_hasIndexBuffer)
//    {
//        vkCmdDraw(commandBuffer, m_vertexCount, 1, 0, 0);
//    }
//    else
//    {
//        vkCmdDrawIndexed(commandBuffer, m_indexCount, 1, 0, 0, 0);
//    }
//}

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
    attributeDesciption[0].format = VK_FORMAT_R32G32B32_SFLOAT;
    attributeDesciption[0].offset = offsetof(Vertex, position);

    attributeDesciption[1].binding = 0;
    attributeDesciption[1].location = 1;
    attributeDesciption[1].format = VK_FORMAT_R32G32B32_SFLOAT;
    attributeDesciption[1].offset = offsetof(Vertex, color);

    return attributeDesciption;
}

//void Model::createVertexBuffers(const std::vector<Vertex>& vertices)
//{
//    //assert(m_vertexCount >= 3 && "Vertex Count must be at lease 3");
//
//    VkDeviceSize bufferSize = sizeof(vertices[0]) * m_vertexCount;
//
//    uint32_t vertexSize = sizeof(vertices[0]);
//
//    VMABuffer stagingBuffer(
//        m_device,
//        vertexSize,
//        m_vertexCount,
//        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
//        VMA_MEMORY_USAGE_CPU_TO_GPU
//    );
//
//    stagingBuffer.map();
//    stagingBuffer.writeToBuffer((void*)vertices.data());
//
//    m_vertexBuffer = std::make_shared<VMABuffer>(
//        m_device,
//        vertexSize,
//        m_vertexCount,
//        VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
//        VMA_MEMORY_USAGE_GPU_ONLY
//    );
//
//
//    m_device.copyBuffer(stagingBuffer.getBuffer(), m_vertexBuffer->getBuffer(), bufferSize);
//}
//
//void Model::createIndexBuffers(const std::vector<uint32_t>& indices)
//{
//    m_hasIndexBuffer = m_indexCount > 0;
//    //assert(m_indexCount >= 3 && "Vertex Count must be at lease 3");
//
//    if (!m_hasIndexBuffer)
//        return;
//
//    VkDeviceSize bufferSize = sizeof(indices[0]) * m_indexCount;
//    uint32_t indexSize = sizeof(indices[0]);
//
//    VMABuffer stagingBuffer{
//           m_device,
//           indexSize,
//           m_indexCount,
//           VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
//           VMA_MEMORY_USAGE_CPU_TO_GPU
//    };
//
//    stagingBuffer.map();
//    stagingBuffer.writeToBuffer((void*)indices.data());
//
//
//    m_indexBuffer = std::make_shared<VMABuffer>(
//        m_device,
//        indexSize,
//        m_indexCount,
//        VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
//        VMA_MEMORY_USAGE_GPU_ONLY
//    );
//
//    m_device.copyBuffer(stagingBuffer.getBuffer(), m_indexBuffer->getBuffer(), bufferSize);
//}