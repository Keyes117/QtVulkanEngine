#pragma once

#include "Buffer.h"
#include "Device.h"
#include "Camera.h"
#include "VMABuffer.h"
#include <qvector2d.h>
#include <vector>

enum class ModelType
{

    Point = 0,
    Line,
    Polygon,
    None
};

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
        ModelType           type;
    };

    //Model() = default;
    Model(Device& device, Model::Builder& builder);
    ~Model();

    void bind(VkCommandBuffer commandBuffer);
    void draw(VkCommandBuffer commandBuffer, const Camera& camera);

    const std::vector<Vertex>& getVerticeRef() const { return m_vertices; }
    const std::vector<uint32_t>& getIndicesRef() const { return m_indices; }
    AABB getBoundingBox() { return m_boundingBox; }
    uint32_t  vertexCount()const { return m_vertices.size(); }
    uint32_t  indexCount() const { return m_indices.size(); }
    bool hasIndexBuffer() const { return m_hasIndexBuffer; }
    ModelType type() const { return m_type; }
private:
    void createVertexBuffers(const std::vector<Vertex>& vertices);
    void createIndexBuffers(const std::vector<uint32_t>& indices);
private:
    Device& m_device;
    ModelType                   m_type;
    std::vector<Vertex>         m_vertices;
    std::vector<uint32_t>       m_indices;
    AABB                        m_boundingBox{ std::numeric_limits<float>::infinity(), std::numeric_limits<float>::infinity(),
        -std::numeric_limits<float>::infinity(), -std::numeric_limits<float>::infinity() };

    //std::shared_ptr<VMABuffer>  m_vertexBuffer;
    uint32_t                    m_vertexCount{ 0 };

    bool                        m_hasIndexBuffer;
    //std::shared_ptr<VMABuffer>  m_indexBuffer;
    uint32_t                    m_indexCount{ 0 };
};


