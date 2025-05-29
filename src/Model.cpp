#include "Model.h"

#include <cassert>
#include <limits>

#ifdef max
#undef max
#endif

#ifdef min
#undef min
#endif

Model::Model(Model::Builder& builder)
    : m_type(builder.type),
    m_vertices(std::move(builder.vertices)),
    m_indices(std::move(builder.indices))
{
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
