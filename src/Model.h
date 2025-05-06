#pragma once

#include "Device.h"

#include <qvector2d.h>
#include <vector>

class Model
{
public:
    struct Vertex
    {
        QVector2D position;
        QVector3D color;

        static std::vector<VkVertexInputBindingDescription> getBindingDescription();
        static std::vector<VkVertexInputAttributeDescription> getAttributeDescription();
    };


    Model(Device& device, const std::vector<Vertex>& vertices);
    ~Model();

    void bind(VkCommandBuffer commandBuffer);
    void draw(VkCommandBuffer commandBuffer);

    // Not copyable or movable
    Model(const Model&) = delete;
    Model& operator=(const Model&) = delete;
    Model(Model&&) = delete;
    Model& operator=(Model&&) = delete;
private:
    void createVertexBuffers(const std::vector<Vertex>& vertices);

private:
    Device& m_device;
    VkBuffer        m_vertexBuffer;
    VkDeviceMemory  m_vertexBufferMemory;
    uint32_t        m_vertexCount;
};

