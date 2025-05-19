#pragma once

#include "Buffer.h"
#include "Device.h"

#include <qvector2d.h>
#include <vector>


class Model
{
public:
    struct Vertex
    {
        QVector3D position{};
        QVector3D color{};

        static std::vector<VkVertexInputBindingDescription> getBindingDescription();
        static std::vector<VkVertexInputAttributeDescription> getAttributeDescription();
    };

    struct Builder
    {
        std::vector<Vertex> vertices{};
        std::vector<uint32_t> indices{};
    };


    Model(Device& device, const Model::Builder& builder);
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
    void createIndexBuffers(const std::vector<uint32_t>& indices);

private:
    Device& m_device;

    std::unique_ptr<Buffer>     m_vertexBuffer;
    uint32_t                    m_vertexCount;

    bool                        m_hasIndexBuffer;
    std::unique_ptr<Buffer>     m_indexBuffer;
    uint32_t                    m_indexCount;
};


